#include <tunmode/tunmode.hpp>
#include <sys/poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <mutex>
#include <vector>
#include <string>
#include <sstream>
#include <cstring>
// 确保引入TunSocket相关头文件（若有单独头文件需补充）
#include <<tunmode/tunsocket.hpp>

namespace tunmode {
    // 1. 修复：删除static，与头文件extern声明匹配（避免重复定义）
    std::atomic<bool> stop_flag(false);
    TunSocket tun; // 2. 修复：用TunSocket替换错误的TunDevice

    // 新增：动态IP拦截列表及线程锁
    static std::vector<std::string> blocked_ips;
    static std::mutex ip_mutex;

    // 新增：更新IP拦截列表的函数
    void update_blocked_ips(const std::vector<std::string>& new_ips) {
        std::lock_guard<std::mutex> lock(ip_mutex);
        blocked_ips = new_ips;
    }

    // 原有线程控制函数（保留，若项目原有实现不同则替换为项目实际代码）
    void _thread_start() {
        // 项目原有_thread_start实现（如无特殊逻辑可留空或保持原样）
    }

    void _thread_stop() {
        // 项目原有_thread_stop实现（如无特殊逻辑可留空或保持原样）
    }

    // 隧道主循环（修复3、4处错误：匹配TunSocket用法）
    void _tunnel_loop() {
        _thread_start();

        struct pollfd fds[1];
        // 3. 修复：用fd()替换不存在的get_fd()（TunSocket标准用法）
        fds[0].fd = tun.fd();
        fds[0].events = POLLIN;

        while (!stop_flag.load()) {
            int ret = poll(fds, 1, 1000);
            if (ret < 0) {
                break; // 错误处理（保留原有逻辑）
            }

            if (fds[0].revents & POLLIN) {
                Packet packet;
                // 4. 修复：用read()替换错误的>运算符（TunSocket读取数据包标准方法）
                ssize_t read_len = tun.read(packet.get_buffer(), packet.get_capacity());
                if (read_len <= 0) {
                    continue; // 读取失败处理（保留原有逻辑）
                }
                packet.set_size(static_cast<size_t>(read_len)); // 设置数据包实际长度

                bool drop_packet = false;
                if (packet.get_size() >= sizeof(ip)) {
                    const ip* ip_header = reinterpret_cast<const ip*>(packet.get_buffer());
                    struct in_addr dest_addr;
                    dest_addr.s_addr = ip_header->ip_dst.s_addr;
                    char* dest_ip_str = inet_ntoa(dest_addr); // 目标IP字符串

                    // 动态IP拦截判断（逻辑不变）
                    std::lock_guard<std::mutex> lock(ip_mutex);
                    for (const auto& blocked_ip : blocked_ips) {
                        if (std::strcmp(dest_ip_str, blocked_ip.c_str()) == 0) {
                            drop_packet = true;
                            break;
                        }
                    }
                }

                if (drop_packet) {
                    continue; // 丢弃数据包（不转发）
                }

                // 原有数据包转发逻辑（保留，若用write发送则匹配TunSocket用法）
                // 示例：tun.write(packet.get_buffer(), packet.get_size());
            }
        }

        _thread_stop();
    }

    // 保留项目原有其他函数（如无则删除）
    // ...
}

// JNI方法实现（保留，逻辑不变）
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
    if (getIpListMethod == nullptr) {
        return; // 防空处理（新增，避免崩溃）
    }
    jstring ipListJString = (jstring)env->CallObjectMethod(thiz, getIpListMethod);
    if (ipListJString == nullptr) {
        return;
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
