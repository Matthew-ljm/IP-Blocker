#include <tunmode/tunmode.hpp>
#include <tunmode/socket/sessionsocket.hpp>
#include <tunmode/manager/tcpmanager.hpp>
#include <tunmode/manager/udpmanager.hpp>

#include <future>
#include <string>
#include <vector>
#include <thread>
#include <sstream>
#include <algorithm>

#include <poll.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include <misc/logger.hpp>

namespace tunmode
{
    namespace params
    {
        JavaVM* jvm;
        TunSocket tun;
        in_addr net_iface;
        in_addr dns_address;
        jobject TunModeService_object;
        std::atomic<bool> stop_flag;

        std::promise<void> tunnel_promise;
        std::atomic<int> thread_count;

        // 存储拦截IP列表
        std::vector<std::string> blocked_ips;
    }

    TCPManager tcp_session_manager;
    UDPManager udp_manager;

    void set_jvm(JavaVM* jvm)
    {
        params::jvm = jvm;
    }

    void initialize(JNIEnv* env, jobject TunModeService_object)
    {
        params::tun = 0;
        SessionSocket::tun = &params::tun;
        params::TunModeService_object = env->NewGlobalRef(TunModeService_object);

        params::stop_flag.store(false);
        params::thread_count.store(0);

        // 初始化默认拦截IP
        params::blocked_ips.clear();
        params::blocked_ips.push_back("192.168.0.102");
    }

    // 设置拦截列表
    void set_blocked_ips(const std::string& ips_str) {
        params::blocked_ips.clear();

        std::stringstream ss(ips_str);
        std::string line;
        while (std::getline(ss, line)) {
            // 简单处理：去除前后空白
            size_t start = line.find_first_not_of(" \t\r\n");
            if (start != std::string::npos) {
                size_t end = line.find_last_not_of(" \t\r\n");
                std::string clean_ip = line.substr(start, end - start + 1);
                if (!clean_ip.empty()) {
                    params::blocked_ips.push_back(clean_ip);
                }
            }
        }

        // 如果没有有效的IP，使用默认值
        if (params::blocked_ips.empty()) {
            params::blocked_ips.push_back("192.168.0.102");
        }

        LOGI_("Blocked IPs updated, count: %d", params::blocked_ips.size());
    }

    int get_jni_env(JNIEnv** env)
    {
        int status = params::jvm->GetEnv((void**)env, JNI_VERSION_1_6);

        if (status == JNI_EDETACHED) {
            if (params::jvm->AttachCurrentThread(env, nullptr) != 0) {
                return 2; // Failed to attach
            }
            return 1; // Attached, need detach
        }

        return 0; // Already attached
    }

    void _thread_start()
    {
        params::thread_count++;
    }

    void _thread_stop()
    {
        int val = params::thread_count.fetch_sub(1) - 1;

        if (val == 0)
        {
            params::tunnel_promise.set_value();
        }
    }

    void _tunnel_loop()
    {
        _thread_start();

        while (!params::stop_flag.load())
        {
            int revents = 0;
            int ret = params::tun.poll(2000, revents);

            if (ret == -1)
            {
                break;
            }
            else if (ret == 0)
            {
                continue;    // Timeout reached
            }
            else
            {
                if (revents & POLLIN)
                {
                    Packet packet;
                    params::tun > packet;

                    bool drop_packet = false;
                    if (packet.get_size() >= sizeof(ip))
                    {
                        const ip* ip_header = reinterpret_cast<const ip*>(packet.get_buffer());
                        struct in_addr dest_addr;
                        dest_addr.s_addr = ip_header->ip_dst.s_addr;
                        char* dest_ip_str = inet_ntoa(dest_addr);

                        // 使用存储的列表进行拦截检查
                        for (const auto& blocked_ip : params::blocked_ips)
                        {
                            if (strcmp(dest_ip_str, blocked_ip.c_str()) == 0)
                            {
                                drop_packet = true;
                                break;
                            }
                        }
                    }

                    if (drop_packet)
                    {
                        continue;
                    }

                    // 原有逻辑不变
                    switch (packet.get_protocol())
                    {
                        case TUNMODE_PROTOCOL_TCP:
                            tcp_session_manager.handle_packet(packet);
                            break;

                        case TUNMODE_PROTOCOL_UDP:
                            udp_manager.handle_packet(packet);
                            break;

                        default:
                            break;
                    }
                }
                else
                {
                    params::stop_flag.store(true);
                }
            }
        }

        _thread_stop();
    }

    void _run_loops()
    {
        std::thread tunnel_loop_thread(_tunnel_loop);
        tunnel_loop_thread.detach();
    }

    void _cleanup()
    {

    }

    void _tunnel_closed()
    {
        params::tun.close();
        params::tun = 0;

        if (params::TunModeService_object == 0) {
            return;
        }

        JNIEnv* env = nullptr;
        int status = get_jni_env(&env);

        if (status == 2)
        {
            return;
        }

        jclass TunModeService_class = env->FindClass("com/matthew/ipblocker/interceptor/services/TunModeService");
        jmethodID TunModeService_tunnelClosed_methodID = env->GetMethodID(
                TunModeService_class,
                "tunnelClosed",
                "()V"
        );

        env->CallVoidMethod(params::TunModeService_object, TunModeService_tunnelClosed_methodID);

        if (status == 1) {
            params::jvm->DetachCurrentThread();
        }
    }

    void open_tunnel()
    {
        params::stop_flag.store(false);
        params::tunnel_promise = std::promise<void>();

        std::future tunnel_future = params::tunnel_promise.get_future();

        _run_loops();

        LOGI_("----- [Tunnel opened] -----");
        tunnel_future.wait();

        _cleanup();
        _tunnel_closed();
        LOGI_("----- [Tunnel closed] -----");
    }

    void close_tunnel()
    {
        params::stop_flag.store(true);
    }
}

// JNI导出函数 - 修改为MainActivity的方法
extern "C" JNIEXPORT void JNICALL
Java_com_matthew_ipblocker_interceptor_activities_MainActivity_setBlockedIPsNative(
        JNIEnv* env,
        jobject thiz,
        jstring j_blocked_ips) {

    const char* blocked_ips_str = env->GetStringUTFChars(j_blocked_ips, nullptr);
    if (blocked_ips_str != nullptr) {
        tunmode::set_blocked_ips(blocked_ips_str);
        env->ReleaseStringUTFChars(j_blocked_ips, blocked_ips_str);
    }
}