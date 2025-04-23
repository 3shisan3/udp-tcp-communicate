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
    Impl() : sockfd_(-1), running_(false) {}

    ~Impl()
    {
        stop();
    }

    bool start(const std::string &local_ip, int local_port, const Config &config)
    {
        // 创建socket
        sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd_ < 0)
            return false;

        // 设置socket选项
        struct timeval tv;
        tv.tv_sec = config.recv_timeout_ms / 1000;
        tv.tv_usec = (config.recv_timeout_ms % 1000) * 1000;
        setsockopt(sockfd_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        tv.tv_sec = config.send_timeout_ms / 1000;
        tv.tv_usec = (config.send_timeout_ms % 1000) * 1000;
        setsockopt(sockfd_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        // 绑定地址
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(local_ip.c_str());
        addr.sin_port = htons(local_port);

        if (bind(sockfd_, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            close(sockfd_);
            sockfd_ = -1;
            return false;
        }

        // 启动接收线程
        running_ = true;
        recv_thread_ = std::thread(&Impl::recvLoop, this, config.max_packet_size);

        return true;
    }

    void stop()
    {
        running_ = false;
        if (recv_thread_.joinable())
        {
            recv_thread_.join();
        }
        if (sockfd_ != -1)
        {
            close(sockfd_);
            sockfd_ = -1;
        }
    }

    bool sendTo(const std::string &dest_addr, int dest_port,
                const void *data, size_t size)
    {
        struct sockaddr_in dest;
        memset(&dest, 0, sizeof(dest));
        dest.sin_family = AF_INET;
        dest.sin_addr.s_addr = inet_addr(dest_addr.c_str());
        dest.sin_port = htons(dest_port);

        ssize_t sent = sendto(sockfd_, data, size, 0,
                              (struct sockaddr *)&dest, sizeof(dest));
        return sent == static_cast<ssize_t>(size);
    }

    void setCallback(ReceiveCallback cb)
    {
        std::lock_guard<std::mutex> lock(cb_mutex_);
        callback_ = std::move(cb);
    }

private:
    void recvLoop(int max_packet_size)
    {
        std::vector<char> buffer(max_packet_size);
        struct sockaddr_in src_addr;
        socklen_t addr_len = sizeof(src_addr);

        while (running_)
        {
            ssize_t recv_len = recvfrom(sockfd_, buffer.data(), buffer.size(), 0,
                                        (struct sockaddr *)&src_addr, &addr_len);

            if (recv_len <= 0)
            {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    continue;
                break; // 发生错误
            }

            std::string src_ip = inet_ntoa(src_addr.sin_addr);
            int src_port = ntohs(src_addr.sin_port);

            // 调用回调
            std::lock_guard<std::mutex> lock(cb_mutex_);
            if (callback_)
            {
                callback_(src_ip, src_port, buffer.data(), recv_len);
            }
        }
    }

    int sockfd_ = -1;
    std::atomic<bool> running_;
    std::thread recv_thread_;
    ReceiveCallback callback_;
    std::mutex cb_mutex_;
};

// UdpCore成员函数实现
UdpCore::UdpCore() : impl_(std::make_unique<Impl>()) {}
UdpCore::~UdpCore() = default;

int UdpCore::initialize()
{

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

void UdpCore::setReceiveCallback(ReceiveCallback callback)
{
    impl_->setCallback(std::move(callback));
}

void UdpCore::setConfig(const Config &config)
{
    config_ = config;
}