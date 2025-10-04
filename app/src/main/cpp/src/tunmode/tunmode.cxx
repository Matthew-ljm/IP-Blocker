#include <<tunmode/tunmode.hpp>
#include <<tunmode/socket/sessionsocket.hpp>
#include <<tunmode/manager/tcpmanager.hpp>
#include <<tunmode/manager/udpmanager.hpp>

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
	}

	TCPManager tcp_session_manager;
	UDPManager udp_session_manager;

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
		// 【新增1：写死拦截IP名单，替换成你的目标IP】
		const char* blocked_ips[] = {"192.168.1.1", "8.8.8.8", "10.0.0.5"};
		const int blocked_count = sizeof(blocked_ips) / sizeof(blocked_ips[0]);

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

					// 【新增2：解析目的IP + 判断是否拦截】
					bool drop_packet = false;
					// 确保数据包长度足够解析IP头（避免越界）
					if (packet.get_size() >= sizeof(ip))
					{
						// 解析IP头中的目的IP（复用系统结构体，不修改原有Packet逻辑）
						const ip* ip_header = reinterpret_cast<const ip*>(packet.get_buffer());
						struct in_addr dest_addr;
						dest_addr.s_addr = ip_header->ip_dst.s_addr;
						char* dest_ip_str = inet_ntoa(dest_addr); // 转为字符串格式

						// 比对拦截名单
						for (int i = 0; i < blocked_count; i++)
						{
							if (strcmp(dest_ip_str, blocked_ips[i]) == 0)
							{
								drop_packet = true;
								break;
							}
						}
					}
					// 命中拦截名单则丢弃，不执行后续转发
					if (drop_packet)
					{
						continue;
					}

					// 【原有协议分发逻辑：完全未修改】
					switch (packet.get_protocol())
					{
					case TUNMODE_PROTOCOL_TCP:
						tcp_session_manager.handle_packet(packet);
						break;

					case TUNMODE_PROTOCOL_UDP:
						udp_session_manager.handle_packet(packet);
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

		jclass TunModeService_class = env->FindClass("git/gxosty/tunmode/interceptor/services/TunModeService");
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
