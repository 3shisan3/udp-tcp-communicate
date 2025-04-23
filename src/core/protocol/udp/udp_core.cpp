#include "udp_core.h"

#include <cstring>
#include <iostream>
#include <functional>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

#include "common/config_wrapper.h"

class UdpCommunicateCore::Impl
{
public:
    Impl() : is_running_(false)
    {
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    }

    ~Impl()
    {
        stop();
#ifdef _WIN32
        WSACleanup();
#endif
    }

    void start()
    {
        if (!is_running_.exchange(true))
        {
            receiver_thread_ = std::thread(&Impl::receiverLoop, this);
        }
    }

    void stop()
    {
        if (is_running_.exchange(false))
        {
            if (receiver_thread_.joinable())
            {
                receiver_thread_.join();
            }
            closeAllSockets();
        }
    }

    bool addListeningSocket(const std::string &addr, int port)
    {
        std::lock_guard<std::mutex> lock(socket_mutex_);

        // 检查是否已存在该端口的监听
        for (const auto &sock : sockets_)
        {
            if (sock.port == port)
            {
                return true;
            }
        }

        SocketType sockfd = createAndBindSocket(addr, port);
        if (sockfd == INVALID_SOCKET)
        {
            return false;
        }

        sockets_.push_back({sockfd, port});
        return true;
    }

    bool sendData(const std::string &dest_addr, int dest_port,
                  const void *data, size_t size)
    {
        std::lock_guard<std::mutex> lock(send_mutex_);

        SocketType sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd == INVALID_SOCKET)
        {
            return false;
        }

        sockaddr_in dest_addr_in = {};
        dest_addr_in.sin_family = AF_INET;
        dest_addr_in.sin_port = htons(dest_port);
        inet_pton(AF_INET, dest_addr.c_str(), &dest_addr_in.sin_addr);

        ssize_t sent_bytes = sendto(sockfd,
                                    reinterpret_cast<const char *>(data),
                                    static_cast<int>(size),
                                    0,
                                    reinterpret_cast<const sockaddr *>(&dest_addr_in),
                                    sizeof(dest_addr_in));
#ifdef _WIN32
        closesocket(sockfd);
#else
        close(sockfd);
#endif

        return sent_bytes == static_cast<ssize_t>(size);
    }

    void addSubscriber(const std::string &key, communicate::SubscribebBase *sub)
    {
        std::lock_guard<std::mutex> lock(sub_mutex_);
        subscribers_[key] = sub;
    }

    communicate::SubscribebBase *getSubscriber(const std::string &key)
    {
        std::lock_guard<std::mutex> lock(sub_mutex_);
        auto it = subscribers_.find(key);
        return it != subscribers_.end() ? it->second : nullptr;
    }

    static std::string createSubKey(const std::string &addr, int port)
    {
        return addr + ":" + std::to_string(port);
    }

private:
    struct ListeningSocket
    {
        SocketType fd;
        int port;
    };

    void receiverLoop()
    {
        constexpr int BUFFER_SIZE = 65536;
        char buffer[BUFFER_SIZE];

        while (is_running_.load())
        {
            std::vector<ListeningSocket> sockets = getCurrentSockets();

            if (sockets.empty())
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            // 平台相关的poll实现
#ifdef _WIN32
            std::vector<WSAPOLLFD> pollfds;
            for (const auto &sock : sockets)
            {
                pollfds.push_back({sock.fd, POLLIN, 0});
            }
            int ret = WSAPoll(pollfds.data(), static_cast<ULONG>(pollfds.size()), 100);
#else
            std::vector<pollfd> pollfds;
            for (const auto &sock : sockets)
            {
                pollfds.push_back({sock.fd, POLLIN, 0});
            }
            int ret = poll(pollfds.data(), pollfds.size(), 100);
#endif

            if (ret <= 0)
                continue;

            for (size_t i = 0; i < pollfds.size(); ++i)
            {
                if (pollfds[i].revents & POLLIN)
                {
                    processIncomingData(pollfds[i].fd);
                }
            }
        }
    }

    void processIncomingData(SocketType sockfd)
    {
        constexpr int BUFFER_SIZE = 65536;
        char buffer[BUFFER_SIZE];

        sockaddr_in src_addr = {};
        socklen_t addr_len = sizeof(src_addr);

        ssize_t recv_len = recvfrom(sockfd, buffer, BUFFER_SIZE, 0,
                                    reinterpret_cast<sockaddr *>(&src_addr), &addr_len);
        if (recv_len <= 0)
            return;

        char src_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &src_addr.sin_addr, src_ip, INET_ADDRSTRLEN);
        int src_port = ntohs(src_addr.sin_port);

        std::shared_ptr<void> msg_data(malloc(recv_len), free);
        memcpy(msg_data.get(), buffer, recv_len);

        std::string specific_key = createSubKey(src_ip, src_port);
        std::string any_key = createSubKey("", src_port);

        if (auto sub = getSubscriber(specific_key))
        {
            sub->handleMsg(msg_data);
        }
        else if (auto sub = getSubscriber(any_key))
        {
            sub->handleMsg(msg_data);
        }
    }

    SocketType createAndBindSocket(const std::string &addr, int port)
    {
        SocketType sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd == INVALID_SOCKET)
        {
            return INVALID_SOCKET;
        }

        int opt = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
                   reinterpret_cast<const char *>(&opt), sizeof(opt));

        sockaddr_in serv_addr = {};
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);
        serv_addr.sin_addr.s_addr = addr.empty() ? INADDR_ANY : inet_addr(addr.c_str());

        if (bind(sockfd, reinterpret_cast<const sockaddr *>(&serv_addr),
                 sizeof(serv_addr)) == SOCKET_ERROR)
        {
#ifdef _WIN32
            closesocket(sockfd);
#else
            close(sockfd);
#endif
            return INVALID_SOCKET;
        }

        // 设置为非阻塞
#ifdef _WIN32
        u_long mode = 1;
        ioctlsocket(sockfd, FIONBIO, &mode);
#else
        fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK);
#endif

        return sockfd;
    }

    void closeAllSockets()
    {
        std::lock_guard<std::mutex> lock(socket_mutex_);
        for (const auto &sock : sockets_)
        {
#ifdef _WIN32
            closesocket(sock.fd);
#else
            close(sock.fd);
#endif
        }
        sockets_.clear();
    }

    std::vector<ListeningSocket> getCurrentSockets()
    {
        std::lock_guard<std::mutex> lock(socket_mutex_);
        return sockets_;
    }

    std::atomic<bool> is_running_;
    std::thread receiver_thread_;
    std::mutex send_mutex_;
    std::mutex sub_mutex_;
    std::mutex socket_mutex_;
    std::vector<ListeningSocket> sockets_;
    std::unordered_map<std::string, communicate::SubscribebBase *> subscribers_;
};

// UdpCommunicateCore 方法实现
UdpCommunicateCore::UdpCommunicateCore() : pimpl_(std::make_unique<Impl>()) {}
UdpCommunicateCore::~UdpCommunicateCore() = default;

int UdpCommunicateCore::initialize()
{
    // 获取配置文件实例
    auto &cfg = SingletonTemplate<ConfigWrapper>::getSingletonInstance().getCfgInstance();
    // 获得一些参数配置项
    m_config.max_packet_size = cfg.getValue("max_packet_size", 1024);
    m_config.recv_timeout_ms = cfg.getValue("recv_timeout_ms", 100);
    m_config.send_timeout_ms = cfg.getValue("send_timeout_ms", 100);
    // 获得订阅端口列表
    auto subscribe_list = cfg.getList<ConfigInterface::CommInfo>("subscribe_list");

    for (const auto &item : subscribe_list)
    {
        if (!pimpl_->addListeningSocket(item.IP, item.Port))
        {
            // std::cerr << "Failed to add listening socket for " << item.IP << ":" << item.Port << std::endl;
            return -1;
        }
    }

    return 0;
}

bool UdpCommunicateCore::send(const std::string &dest_addr, int dest_port,
                              const void *data, size_t size)
{
    return pimpl_->sendData(dest_addr, dest_port, data, size);
}

int UdpCommunicateCore::receiveMessage(char *addr, int port, communicate::SubscribebBase *sub)
{
    std::string addr_str(addr ? addr : "");
    if (!pimpl_->addListeningSocket(addr_str, port))
    {
        return -1;
    }

    std::string key = UdpCommunicateCore::Impl::createSubKey(addr_str, port);
    pimpl_->addSubscriber(key, sub);
    pimpl_->start();
    return 0;
}

void UdpCommunicateCore::shutdown()
{
    pimpl_->stop();
}