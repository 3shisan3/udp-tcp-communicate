#include "udp_core.h"

#include <arpa/inet.h>
#include <atomic>
#include <cstring>
#include <netinet/in.h>
#include <stdexcept>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

#include "common/config_wrapper.h"


// 实现类（PIMPL模式）
class UdpCore::Impl
{
public:
    Impl() = default;
    ~Impl() { stop(); }

    bool start(const std::vector<ConfigInterface::CommInfo> &subscribe_list,
               const CoreConfig &core_config,
               const std::unordered_map<std::string, SubscribebBase *> addr_recvDealFunc)
    {
        config_ = core_config;
        // 绑定所有订阅端口
        for (const auto &endpoint : subscribe_list)
        {
            // 拼接IP和端口
            std::string target = endpoint.IP + ":" + std::to_string(endpoint.Port);
            SubscribebBase *dealObj = addr_recvDealFunc.find(target) == addr_recvDealFunc.end() ? addr_recvDealFunc.at("") : addr_recvDealFunc.at(target);
            if (!bindPort(endpoint.IP, endpoint.Port, core_config, dealObj))
            {
                return false;
            }
        }

        running_ = true;
        return true;
    }

    void stop()
    {
        running_ = false;

        for (auto &thread : recv_threads_)
        {
            if (thread.joinable())
                thread.join();
        }

        for (int fd : socket_fds_)
        {
            close(fd);
        }
        socket_fds_.clear();
    }

    bool sendTo(const std::string &dest_addr, int dest_port,
                const void *data, size_t size)
    {
        struct sockaddr_in dest;
        memset(&dest, 0, sizeof(dest));
        dest.sin_family = AF_INET;
        dest.sin_addr.s_addr = inet_addr(dest_addr.c_str());
        dest.sin_port = htons(dest_port);

        ssize_t sent = sendto(socket_fds_[0], data, size, 0,
                              (struct sockaddr *)&dest, sizeof(dest));
        return sent == static_cast<ssize_t>(size);
    }

private:
    bool bindPort(const std::string &ip, int port, const CoreConfig &config, SubscribebBase *dealObj)
    {
        int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd < 0)
            return false;

        // 设置socket选项
        struct timeval tv;
        tv.tv_sec = config.recv_timeout_ms / 1000;
        tv.tv_usec = (config.recv_timeout_ms % 1000) * 1000;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        tv.tv_sec = config.send_timeout_ms / 1000;
        tv.tv_usec = (config.send_timeout_ms % 1000) * 1000;
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        // 绑定地址
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(ip.c_str());
        addr.sin_port = htons(port);

        if (bind(sockfd, (sockaddr *)&addr, sizeof(addr)))
        {
            close(sockfd);
            return false;
        }

        socket_fds_.push_back(sockfd);

        // 为每个socket启动接收线程
        recv_threads_.emplace_back([this, sockfd, dealObj]() {
            receiveLoop(sockfd, dealObj);
        });

        return true;
    }

    void receiveLoop(int sockfd, SubscribebBase *dealObj)
    {
        std::vector<char> buffer(config_.max_packet_size);

        while (running_)
        {
            sockaddr_in src_addr{};
            socklen_t addr_len = sizeof(src_addr);

            ssize_t recv_len = recvfrom(
                sockfd, buffer.data(), buffer.size(), 0,
                (sockaddr *)&src_addr, &addr_len);

            if (recv_len > 0)
            {
                std::lock_guard<std::mutex> lock(callback_mutex_);
                if (dealObj)
                {
                    callback_(
                        inet_ntoa(src_addr.sin_addr),
                        ntohs(src_addr.sin_port),
                        buffer.data(),
                        recv_len);
                }
            }
        }
    }

    CoreConfig config_;
    std::atomic<bool> running_{false};
    std::vector<int> socket_fds_;
    std::vector<std::thread> recv_threads_;
    std::mutex callback_mutex_;
};

// UdpCore成员函数实现
UdpCore::UdpCore() : impl_(std::make_unique<Impl>()) {}
UdpCore::~UdpCore() = default;

int UdpCore::initialize()
{
    // 获取配置文件实例
    auto &cfg = SingletonTemplate<ConfigWrapper>::getSingletonInstance().getCfgInstance();
    // 获得一些参数配置项
    m_config.max_packet_size = cfg.getValue("max_packet_size", 1024);
    m_config.recv_timeout_ms = cfg.getValue("recv_timeout_ms", 100);
    m_config.send_timeout_ms = cfg.getValue("send_timeout_ms", 100);
    // 获得订阅端口列表
    auto subscribe_list = cfg.getList<ConfigInterface::CommInfo>("subscribe_list");
    // 获得发送对象列表
    auto send_list = cfg.getList<ConfigInterface::CommInfo>("send_list");

    if (impl_->start(subscribe_list, m_config))
    {

    }
    // return impl_->start(local_ip, local_port, config_);
    return 0;
}

void UdpCore::shutdown()
{
    impl_->stop();
}

bool UdpCore::send(const std::string &dest_addr, int dest_port,
                   const void *data, size_t size)
{
    return impl_->sendTo(dest_addr, dest_port, data, size);
}

// void UdpCore::setReceiveCallback(ReceiveCallback callback)
// {
//     impl_->setCallback(std::move(callback));
// }