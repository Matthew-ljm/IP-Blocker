#include <tunmode/tunmode.hpp>
#include <tunmode/socket/sessionsocket.hpp>
#include <tunmode/manager/tcpmanager.hpp>
#include <tunmode/manager/udpmanager.hpp>

#include <future>
#include <string>
#include <vector>
#include <thread>

#include <poll.h>
#include <unistd.h>
// 新增：解析IP需要的系统头文件（不影响原有逻辑）
#include <netinet/ip.h>
#include <arpa/inet.h>

#include <misc/logger.hpp>
namespace tunmode {
    // 原有参数定义（完全保留）
    namespace params {
        static std::atomic stop_flag(false);
        static TunDevice tun;
        // 其他原有参数...
    }
// 新增：动态IP拦截列表及线程锁
static std::vector<std::string> blocked_ips;
static std::mutex ip_mutex;

// 新增：更新IP拦截列表的函数
void update_blocked_ips(const std::vector<std::string>& new_ips) {
    std::lock_guard<std::mutex> lock(ip_mutex);
    blocked_ips = new_ips;
}

// 原有线程控制函数（完全保留）
void _thread_start() {
    // 原有实现...
}

void _thread_stop() {
    // 原有实现...
}

// 隧道主循环（仅修改IP拦截判断部分）
void _tunnel_loop() {
    _thread_start();

    struct pollfd fds[1];
    fds[0].fd = params::tun.get_fd();
    fds[0].events = POLLIN;

    while (!params::stop_flag.load()) {
        int ret = poll(fds, 1, 1000);
        if (ret < 0) {
            // 错误处理（原有代码保留）
            break;
        }

        if (fds[0].revents & POLLIN) {
            Packet packet;
            if (!(params::tun > packet)) {
                // 读取失败处理（原有代码保留）
                continue;
            }

            bool drop_packet = false;
            if (packet.get_size() >= sizeof(ip)) {
                const ip* ip_header = reinterpret_cast<const ip*>(packet.get_buffer());
                struct in_addr dest_addr;
                dest_addr.s_addr = ip_header->ip_dst.s_addr;
                char* dest_ip_str = inet_ntoa(dest_addr);  // 目标IP字符串

                // 修改：使用动态IP列表替换原有写死列表
                std::lock_guard<std::mutex> lock(ip_mutex);
                for (const auto& blocked_ip : blocked_ips) {
                    if (std::strcmp(dest_ip_str, blocked_ip.c_str()) == 0) {
                        drop_packet = true;
                        break;
                    }
                }
            }

            if (drop_packet) {
                // 丢弃数据包（不转发）
                continue;
            }

            // 原有数据包转发逻辑（完全保留）
            // ...
        }
    }

    _thread_stop();
}

// 其他原有函数（完全保留）
// ...

}
// 新增：JNI方法实现（从Java层同步IP列表）
extern "C" JNIEXPORT void JNICALL
Java_git_gxosty_tunmode_interceptor_activities_MainActivity_updateBlockedIpsInNative(
        JNIEnv* env, jobject thiz) {
    // 获取Java层的IP列表字符串
    jclass mainActivityClass = env->GetObjectClass(thiz);
    jmethodID getIpListMethod = env->GetMethodID(
            mainActivityClass, 
            "getIpListAsString", 
            "()Ljava/lang/String;"
    );
    jstring ipListJString = (jstring)env->CallObjectMethod(thiz, getIpListMethod);
    if (ipListJString == nullptr) {
        return; // 空列表处理
    }
// 转换为C++字符串
const char* ipListChars = env->GetStringUTFChars(ipListJString, nullptr);
std::string ipListStr(ipListChars);
env->ReleaseStringUTFChars(ipListJString, ipListChars);

// 解析逗号分隔的IP列表
std::vector<std::string> newIpList;
std::istringstream iss(ipListStr);
std::string ip;
while (std::getline(iss, ip, ',')) {
    if (!ip.empty()) {
        newIpList.push_back(ip);
    }
}

// 更新到C++层的拦截列表
tunmode::update_blocked_ips(newIpList);

}
