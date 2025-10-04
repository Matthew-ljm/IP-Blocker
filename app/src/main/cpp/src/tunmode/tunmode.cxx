#include "tunmode/tunmode.hpp" // 修复1：删除多余的`<`，用双引号（项目内部头文件标准用法）
#include <sys/poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <mutex>
#include <vector>
#include <string>
#include <sstream>
#include <cstring>
// 修复2：删除不存在的<<tunmode/tunsocket.hpp>引入，复用tunmode.hpp（假设TunSocket已在其中声明）

namespace tunmode {
    // 1. 与头文件extern声明匹配，删除static（避免重复定义）
    std::atomic<bool> stop_flag(false);
    TunSocket tun; // 若tunmode.hpp已声明TunSocket，此处可直接使用

    // 新增：动态IP拦截列表及线程锁
    static std::vector<std::string> blocked_ips;
    static std::mutex ip_mutex;

    // 新增：更新IP拦截列表的函数
    void update_blocked_ips(const std::vector<std::string>& new_ips) {
        std::lock_guard<std::mutex> lock(ip_mutex);
        blocked_ips = new_ips;
    }

    // 原有线程控制函数（保留项目实际实现）
    void _thread_start() {
        // 项目原有_thread_start逻辑（如无则留空）
    }

    void _thread_stop() {
        // 项目原有_thread_stop逻辑（如无则留空）
    }

    // 隧道主循环（修复TunSocket用法）
    void _tunnel_loop() {
        _thread_start();

        struct pollfd fds[1];
        // 假设TunSocket用fd()获取文件描述符（若项目用其他方法，替换为实际函数名）
        fds[0].fd = tun.fd();
        fds[0].events = POLLIN;

        while (!stop_flag.load()) {
            int ret = poll(fds, 1, 1000);
            if (ret < 0) {
                break; // 错误处理（保留原有逻辑）
            }

            if (fds[0].revents & POLLIN) {
                Packet packet;
                // 假设TunSocket用read()读取数据包（替换为项目实际读取方法）
                ssize_t read_len = tun.read(packet.get_buffer(), packet.get_capacity());
                if (read_len <= 0) {
                    continue; // 读取失败处理
                }
                packet.set_size(static_cast<size_t>(read_len)); // 设置数据包长度

                bool drop_packet = false;
                if (packet.get_size() >= sizeof(ip)) {
                    const ip* ip_header = reinterpret_cast<const ip*>(packet.get_buffer());
                    struct in_addr dest_addr;
                    dest_addr.s_addr = ip_header->ip_dst.s_addr;
                    char* dest_ip_str = inet_ntoa(dest_addr);

                    // 动态IP拦截判断
                    std::lock_guard<std::mutex> lock(ip_mutex);
                    for (const auto& blocked_ip : blocked_ips) {
                        if (std::strcmp(dest_ip_str, blocked_ip.c_str()) == 0) {
                            drop_packet = true;
                            break;
                        }
                    }
                }

                if (drop_packet) {
                    continue; // 丢弃数据包
                }

                // 原有数据包转发逻辑（如用write发送，替换为项目实际方法）
                // tun.write(packet.get_buffer(), packet.get_size());
            }
        }

        _thread_stop();
    }

    // 保留项目原有其他函数
    // ...
}

// JNI方法实现（不变）
extern "C" JNIEXPORT void JNICALL
Java_git_gxosty_tunmode_interceptor_activities_MainActivity_updateBlockedIpsInNative(
        JNIEnv* env, jobject thiz) {
    jclass mainActivityClass = env->GetObjectClass(thiz);
    jmethodID getIpListMethod = env->GetMethodID(
            mainActivityClass, 
            "getIpListAsString", 
            "()Ljava/lang/String;"
    );
    if (getIpListMethod == nullptr) {
        return;
    }
    jstring ipListJString = (jstring)env->CallObjectMethod(thiz, getIpListMethod);
    if (ipListJString == nullptr) {
        return;
    }

    const char* ipListChars = env->GetStringUTFChars(ipListJString, nullptr);
    std::string ipListStr(ipListChars);
    env->ReleaseStringUTFChars(ipListJString, ipListChars);

    std::vector<std::string> newIpList;
    std::istringstream iss(ipListStr);
    std::string ip;
    while (std::getline(iss, ip, ',')) {
        if (!ip.empty()) {
            newIpList.push_back(ip);
        }
    }

    tunmode::update_blocked_ips(newIpList);
}
