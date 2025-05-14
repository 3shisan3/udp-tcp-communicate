#include "udp_core.h"

#include <cstring>
#include <iostream>
#include <functional>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

class UdpCommunicateCore::Impl
{
public:
    Impl(CoreConfig& config) : is_running_(false), config_(config)
    {
        LOG_TRACE("UDP Core Impl constructor");
#ifdef THREAD_POOL_MODE
        thread_pool_ = std::make_unique<ThreadPoolWrapper>(config.thread_pool_size);
        LOG_DEBUG("Created thread pool with size: {}", config.thread_pool_size);
#endif
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
            LOG_ERROR("WSAStartup failed");
        else
            LOG_DEBUG("WSAStartup successful");
#endif
    }

    ~Impl()
    {
        LOG_TRACE("UDP Core Impl destructor");
        stop();
#ifdef _WIN32
        WSACleanup();
        LOG_DEBUG("WSACleanup called");
#endif
    }

    void start()
    {
        if (!is_running_.exchange(true))
        {
            LOG_INFO("Starting UDP receiver thread");
            receiver_thread_ = std::thread(&Impl::receiverLoop, this);
        }
    }

    void stop()
    {
        if (is_running_.exchange(false))
        {
            LOG_INFO("Stopping UDP receiver thread");
            if (receiver_thread_.joinable())
            {
                receiver_thread_.join();
                LOG_DEBUG("Receiver thread joined");
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
                LOG_WARNING("Port {} is already being listened on", port);
                return true;
            }
        }

        SocketType sockfd = createAndBindSocket(addr, port);
        if (sockfd == INVALID_SOCKET)
        {
            LOG_ERROR("Failed to create/bind socket for {}:{}", addr, port);
            return false;
        }

        sockets_.push_back({sockfd, port});
        LOG_INFO("Added listening socket for {}:{}", addr, port);
        return true;
    }

    bool sendData(const std::string &dest_addr, int dest_port,
                  const void *data, size_t size)
    {
        LOG_TRACE("Attempting to send {} bytes to {}:{}", size, dest_addr, dest_port);
        std::lock_guard<std::mutex> lock(send_mutex_);

        SocketType sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd == INVALID_SOCKET)
        {
            LOG_ERROR("Failed to create socket for sending");
            return false;
        }
        // 强制设置 SO_REUSEADDR，允许端口复用
        int opt = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
                       reinterpret_cast<const char *>(&opt), sizeof(opt)) == SOCKET_ERROR)
        {
            LOG_ERROR("Failed to set SO_REUSEADDR on socket");
#ifdef _WIN32
            closesocket(sockfd);
#else
            close(sockfd);
#endif
            return false;
        }
        // 设置发送超时
#ifdef _WIN32
        DWORD send_timeout = config_.send_timeout_ms;
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&send_timeout, sizeof(send_timeout));
#else
        struct timeval tv;
        tv.tv_sec = config_.send_timeout_ms / 1000;
        tv.tv_usec = (config_.send_timeout_ms % 1000) * 1000;
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv));
#endif
        // 如果配置了源端口，则绑定
        if (config_.source_port > 0)
        {
            sockaddr_in local_addr = {};
            local_addr.sin_family = AF_INET;
            local_addr.sin_port = htons(config_.source_port);
            local_addr.sin_addr.s_addr = INADDR_ANY;

            if (bind(sockfd, reinterpret_cast<const sockaddr *>(&local_addr),
                     sizeof(local_addr)) == SOCKET_ERROR)
            {
#ifdef _WIN32
                int error = WSAGetLastError();
                LOG_ERROR("Bind failed with error: {}", error);
                closesocket(sockfd);
#else
                LOG_ERROR("Bind failed: {}", strerror(errno));
                close(sockfd);
#endif
                return false;
            }
        }
        // 初始化目标地址
        sockaddr_in dest_addr_in = {};
        dest_addr_in.sin_family = AF_INET;
        dest_addr_in.sin_port = htons(dest_port);

        if (inet_pton(AF_INET, dest_addr.c_str(), &dest_addr_in.sin_addr) <= 0)
        {
            LOG_ERROR("Invalid destination address: {}", dest_addr);
#ifdef _WIN32
            closesocket(sockfd);
#else
            close(sockfd);
#endif
            return false;
        }
        // 分片发送逻辑
        const char *data_ptr = reinterpret_cast<const char *>(data);
        size_t remaining = size;
        bool success = true;
        while (remaining > 0)
        {
            size_t chunk_size = (remaining > config_.max_send_packet_size) ? config_.max_send_packet_size : remaining;

            ssize_t sent_bytes = sendto(
                sockfd,
                data_ptr,
                static_cast<int>(chunk_size),
                0,
                reinterpret_cast<const sockaddr *>(&dest_addr_in),
                sizeof(dest_addr_in));
            if (sent_bytes != static_cast<ssize_t>(chunk_size))
            {
                LOG_ERROR("Failed to send complete chunk (sent {} of {} bytes)", sent_bytes, chunk_size);
                success = false;
                break;
            }
            data_ptr += sent_bytes;
            remaining -= sent_bytes;
        }
        // 关闭socket
#ifdef _WIN32
        closesocket(sockfd);
#else
        close(sockfd);
#endif

        if (success)
            LOG_DEBUG("Successfully sent {} bytes to {}:{}", size, dest_addr, dest_port);
        else
            LOG_ERROR("Failed to send complete message to {}:{}", dest_addr, dest_port);
        return success;
    }

    void addSubscriber(const std::string &key, communicate::SubscribebBase *sub)
    {
        LOG_DEBUG("Adding subscriber for key: {}", key);
        std::unique_lock<std::shared_mutex> lock(sub_mutex_);
        subscribers_[key] = sub;
    }

    communicate::SubscribebBase *getSubscriber(const std::string &key)
    {
        LOG_TRACE("Get subscriber for key: {}", key);
        std::shared_lock<std::shared_mutex> lock(sub_mutex_);
        auto it = subscribers_.find(key);
        LOG_DEBUG("Success subscriber for key: {}", it != subscribers_.end() ? key : "");
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

    /* 拓展可参考sogou/workflow 实现轮询线程池 */
    void receiverLoop()
    {
        LOG_INFO("Receiver thread started");
        constexpr int BUFFER_SIZE = 65536;
        char buffer[BUFFER_SIZE];

        while (is_running_.load())
        {
            std::vector<ListeningSocket> sockets = getCurrentSockets();

            if (sockets.empty())
            {
                LOG_TRACE("No sockets to poll, sleeping");
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

            if (ret < 0)
            {
                LOG_ERROR("Poll error: {}", strerror(errno));
                continue;
            }
            else if (ret == 0)
            {
                LOG_TRACE("Poll timeout");
                continue;
            }

            for (size_t i = 0; i < pollfds.size(); ++i)
            {
                if (pollfds[i].revents & POLLIN)
                {
                    LOG_TRACE("Data available on socket {}", i);
                    // recvfrom，getsockname 非线程安全操作，不将整个处理加入线程池
                    processIncomingData(pollfds[i].fd);
                }
            }
        }
        LOG_INFO("Receiver thread exiting");
    }

    void processIncomingData(SocketType sockfd)
    {
        // 使用配置的最大包大小
        std::vector<char> buffer(config_.max_receive_packet_size);
        // 接收数据（获取发送方信息）
        sockaddr_in src_addr = {};
        socklen_t addr_len = sizeof(src_addr);
        ssize_t recv_len = recvfrom(sockfd, buffer.data(), config_.max_receive_packet_size, 0,
                                    reinterpret_cast<sockaddr *>(&src_addr), &addr_len);
        if (recv_len <= 0)
        {
            LOG_ERROR("recvfrom failed: {}", strerror(errno));
            return;
        }
        
        LOG_DEBUG("Received {} bytes from socket {}", recv_len, sockfd);
        
        // 复制数据到共享内存（避免线程竞争）
        auto msg_data = std::shared_ptr<void>(malloc(recv_len), free);
        memcpy(msg_data.get(), buffer.data(), recv_len);

        // 获取发送方IP和端口
        char src_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &src_addr.sin_addr, src_ip, INET_ADDRSTRLEN);
        int src_port = ntohs(src_addr.sin_port);
        // 获取本地该消息来源IP和端口
        sockaddr_in local_addr = {};
        socklen_t local_addr_len = sizeof(local_addr);
        char local_ip[INET_ADDRSTRLEN] = {0};
        int local_port = 0;
        if (getsockname(sockfd, (sockaddr *)&local_addr, &local_addr_len) == 0)
        {
            inet_ntop(AF_INET, &local_addr.sin_addr, local_ip, INET_ADDRSTRLEN);
            local_port = ntohs(local_addr.sin_port);
        }

        LOG_TRACE("Message from {}:{} to {}:{}", src_ip, src_port, local_ip, local_port);

        // 捕获关键信息（避免线程间传递复杂对象）
        struct MatchContext
        {
            std::string sender_key;
            std::string local_key;
            std::string wildcard_key;
            std::string any_key;
        };
        auto context = std::make_shared<MatchContext>();
        context->sender_key = createSubKey(src_ip, src_port);           // 精确发送方
        context->local_key = createSubKey(local_ip, local_port);        // 精确本地
        context->wildcard_key = createSubKey("localhost", local_port);  // 本地通用匹配前缀+指定端口
        context->any_key = createSubKey("", 0);                         // 完全通配

        // 生成匹配任务
        auto process_msg = [this, context, msg_data] {
            if (auto sub = getSubscriber(context->sender_key) ?:
                           getSubscriber(context->local_key)  ?:
                           getSubscriber(context->wildcard_key) ?:
                           getSubscriber(context->any_key))
            {
                sub->handleMsg(msg_data);
            }
            else
            {
                LOG_WARNING("No subscriber found for message");
            }
        };

#ifdef THREAD_POOL_MODE
        thread_pool_->enqueue(process_msg);
#else
        process_msg();
#endif
    }

    SocketType createAndBindSocket(const std::string &addr, int port)
    {
        // 1. 创建 UDP Socket
        // AF_INET: IPv4 地址族
        // SOCK_DGRAM: UDP 协议
        // 0: 自动选择协议（UDP 默认是 IPPROTO_UDP）
        SocketType sockfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sockfd == INVALID_SOCKET)
        {
            LOG_ERROR("Failed to create socket: {}", strerror(errno));
            return INVALID_SOCKET;
        }
        // 2. 设置 SO_REUSEADDR 选项
        // 允许地址和端口重用（避免 bind() 失败，特别是在程序崩溃后快速重启时）
        int opt = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&opt), sizeof(opt)) == SOCKET_ERROR)
        {
            LOG_ERROR("Failed to set SO_REUSEADDR: {}", strerror(errno));
#ifdef _WIN32
            closesocket(sockfd);
#else
            close(sockfd);
#endif
            return INVALID_SOCKET;
        }
        // 3. 初始化服务器地址结构
        sockaddr_in serv_addr = {};
        serv_addr.sin_family = AF_INET;         // IPv4 地址族
        serv_addr.sin_port = htons(port);       // 端口号（转换为网络字节序）
        // 4. 设置监听的 IP 地址（设备多网卡的情况）
        // 如果 addr 为空，则监听所有网卡（INADDR_ANY）
        // 否则，解析指定的 IP 地址（inet_addr 将点分十进制转换为网络字节序）
        serv_addr.sin_addr.s_addr = addr.empty() ? INADDR_ANY : inet_addr(addr.c_str());
        // 5. 绑定 Socket 到指定的 IP 和端口
        if (bind(sockfd, reinterpret_cast<const sockaddr *>(&serv_addr), sizeof(serv_addr)) == SOCKET_ERROR)
        {
            LOG_ERROR("Failed to bind socket to {}:{} - {}", addr.empty() ? "INADDR_ANY" : addr, port, strerror(errno));
#ifdef _WIN32
            closesocket(sockfd);
#else
            close(sockfd);
#endif
            return INVALID_SOCKET;
        }
        // 设置为非阻塞
        // 这样 recvfrom() 不会阻塞，如果没有数据可读，会立即返回错误（EWOULDBLOCK/EAGAIN）
#ifdef _WIN32
        u_long mode = 1;
        ioctlsocket(sockfd, FIONBIO, &mode);
#else
        fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK);
#endif

        LOG_DEBUG("Successfully created and bound socket for {}:{}", addr.empty() ? "INADDR_ANY" : addr, port);
        return sockfd;
    }

    void closeAllSockets()
    {
        LOG_DEBUG("Closing all sockets");
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
    CoreConfig& config_;
#ifdef THREAD_POOL_MODE
    std::unique_ptr<ThreadPoolWrapper> thread_pool_;
#endif
    std::thread receiver_thread_;
    std::mutex send_mutex_;
    std::shared_mutex sub_mutex_;   // 读写锁
    std::mutex socket_mutex_;
    std::vector<ListeningSocket> sockets_;
    std::unordered_map<std::string, communicate::SubscribebBase *> subscribers_;
};

// UdpCommunicateCore 方法实现
UdpCommunicateCore::UdpCommunicateCore() : pimpl_(std::make_unique<Impl>(m_config))
{
    LOG_TRACE("UdpCommunicateCore constructor");
}
UdpCommunicateCore::~UdpCommunicateCore()
{
    LOG_TRACE("UdpCommunicateCore destructor");
}

int UdpCommunicateCore::initialize()
{
    LOG_INFO("Initializing UDP communication core");
    // 获取配置文件实例
    auto &cfg = SingletonTemplate<ConfigWrapper>::getSingletonInstance().getCfgInstance();
    // 获得一些参数配置项
    m_config.max_send_packet_size = cfg.getValue("max_send_packet_size", 1024);
    m_config.max_receive_packet_size = cfg.getValue("max_receive_packet_size", 65507);
    m_config.recv_timeout_ms = cfg.getValue("recv_timeout_ms", 100);
    m_config.send_timeout_ms = cfg.getValue("send_timeout_ms", 100);
    m_config.source_port = cfg.getValue("source_port", 0);
    m_config.thread_pool_size = cfg.getValue("thread_pool_size", 3);

    LOG_DEBUG("Configuration loaded - max_send: {}, max_recv: {}, send_timeout: {}ms, recv_timeout: {}ms, source_port: {}, thread_pool: {}",
              m_config.max_send_packet_size, m_config.max_receive_packet_size,
              m_config.send_timeout_ms, m_config.recv_timeout_ms,
              m_config.source_port, m_config.thread_pool_size);

    // 获得需要监听的端口列表
    auto listen_list = cfg.getList<ConfigInterface::CommInfo>("listen_list");
    for (const auto &item : listen_list)
    {
        LOG_DEBUG("Adding listen address: {}:{}", item.IP, item.Port);
        if (!pimpl_->addListeningSocket(item.IP, item.Port))
        {
            LOG_ERROR("Failed to add listening socket for {}:{}", item.IP, item.Port);
            return -1;
        }
    }

    LOG_INFO("UDP communication core initialized successfully");
    return 0;
}

bool UdpCommunicateCore::send(const std::string &dest_addr, int dest_port,
                              const void *data, size_t size)
{
    LOG_DEBUG("Sending data to {}:{} (size: {})", dest_addr, dest_port, size);
    return pimpl_->sendData(dest_addr, dest_port, data, size);
}

int UdpCommunicateCore::addListenAddr(const char *addr, int port)
{
    std::string addr_str(addr ? addr : "");
    LOG_DEBUG("Adding listen address: {}:{}", addr_str, port);
    if (!pimpl_->addListeningSocket(addr_str, port))
    {
        LOG_ERROR("Failed to add listening socket for {}:{}", addr_str, port);
        return -1;
    }
    return 0;
}

int UdpCommunicateCore::addSubscribe(const char *addr, int port, communicate::SubscribebBase *sub)
{
    std::string addr_str(addr ? addr : "");
    std::string key = UdpCommunicateCore::Impl::createSubKey(addr_str, port);
    LOG_DEBUG("Adding subscriber for key: {}", key);
    pimpl_->addSubscriber(key, sub);
    pimpl_->start();
    return 0;
}

void UdpCommunicateCore::shutdown()
{
    LOG_INFO("Shutting down UDP communication core");
    pimpl_->stop();
}

void UdpCommunicateCore::setSendPort(int port)
{
    LOG_DEBUG("Setting source port to {}", port);
    m_config.source_port = port;
}