#include "tcp_core.h"

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

class TcpCommunicateCore::Impl
{
public:
    Impl(CoreConfig& config) : is_running_(false), config_(config)
    {
        LOG_TRACE("TCP Core Impl constructor");
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
        LOG_TRACE("TCP Core Impl destructor");
        stop();
        cleanConnections();
#ifdef _WIN32
        WSACleanup();
        LOG_DEBUG("WSACleanup called");
#endif
    }

    void start()
    {
        LOG_TRACE("Start listening for TCP connections");
        if (!is_running_.exchange(true))
        {
            LOG_INFO("Starting TCP acceptor thread");
            acceptor_thread_ = std::thread(&Impl::acceptorLoop, this);
            
            LOG_INFO("Starting TCP receiver thread");
            receiver_thread_ = std::thread(&Impl::receiverLoop, this);
        }
    }

    void stop()
    {
        LOG_TRACE("Stop listening for TCP connections");
        if (is_running_.exchange(false))
        {
            LOG_INFO("Stopping TCP threads");
            
            // 通知所有线程停止
            stop_signal_.notify_all();
            
            if (acceptor_thread_.joinable())
            {
                acceptor_thread_.join();
                LOG_DEBUG("Acceptor thread joined");
            }
            
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
        std::string key = addr + ":" + std::to_string(port);
        
        std::lock_guard<std::mutex> lock(socket_mutex_);

        // 检查是否已存在该地址的监听
        for (const auto &sock : listen_sockets_)
        {
            if (sock.addr_port == key)
            {
                LOG_WARNING("The addr_key {} is already being listened on", key);
                return true;
            }
        }

        SocketType sockfd = createAndBindSocket(addr, port);
        if (sockfd == INVALID_SOCKET)
        {
            LOG_ERROR("Failed to create/bind listen socket for {}:{}", addr, port);
            return false;
        }

        // 开始监听
        if (listen(sockfd, config_.listen_backlog) == SOCKET_ERROR)
        {
            LOG_ERROR("Failed to listen on socket for {}:{} - {}", addr, port, strerror(errno));
#ifdef _WIN32
            closesocket(sockfd);
#else
            close(sockfd);
#endif
            return false;
        }

        listen_sockets_.push_back({sockfd, key});
        LOG_INFO("Added listening socket for {}:{}", addr, port);
        return true;
    }

    bool connectToServer(const std::string &addr, int port)
    {
        // 检查连接数限制
        if (current_connections_.load() >= config_.max_connections)
        {
            LOG_ERROR("Maximum connections limit reached ({}), cannot create new connection",
                      config_.max_connections);
            return false;
        }

        std::string key = addr + ":" + std::to_string(port);
        
        std::lock_guard<std::mutex> lock(conn_mutex_);
        
        // 检查是否已存在该目标地址的连接
        if (connections_.find(key) != connections_.end())
        {
            LOG_WARNING("Connection to {} already exists", key);
            return true;
        }

        SocketType sockfd = createAndConnectSocket(addr, port);
        if (sockfd == INVALID_SOCKET)
        {
            LOG_ERROR("Failed to connect to {}:{}", addr, port);
            return false;
        }

        // 创建完整的ConnectionInfo
        ConnectionInfo conn;
        conn.fd = sockfd;
        conn.remote_addr = addr;
        conn.remote_port = port;

        // 获取本地地址信息
        sockaddr_in local_addr = {};
        socklen_t addr_len = sizeof(local_addr);
        if (getsockname(sockfd, (sockaddr *)&local_addr, &addr_len) == 0)
        {
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &local_addr.sin_addr, ip, INET_ADDRSTRLEN);
            conn.local_addr = ip;
            conn.local_port = ntohs(local_addr.sin_port);
        }

        connections_[key] = conn;
        LOG_INFO("Connected to {}:{}", addr, port);
        // 成功创建后增加计数
        current_connections_++;
        return true;
    }

    bool sendData(const std::string &dest_addr, int dest_port,
                  const void *data, size_t size)
    {
        LOG_TRACE("Attempting to send {} bytes to {}:{}", size, dest_addr, dest_port);

        SocketType sockfd = getConnection(dest_addr, dest_port);
        if (sockfd == INVALID_SOCKET)
        {
            LOG_WARNING("No existing connection, creating new one");
            if (!connectToServer(dest_addr, dest_port))
            {
                return false;
            }
            sockfd = getConnection(dest_addr, dest_port);
            if (sockfd == INVALID_SOCKET)
            {
                return false;
            }
        }

        return doSend(sockfd, data, size);
    }

    bool doSend(SocketType sockfd, const void* data, size_t size)
    {
        // 分片发送逻辑
        const char *data_ptr = reinterpret_cast<const char *>(data);
        size_t remaining = size;
        bool success = true;
        
        while (remaining > 0)
        {
            size_t chunk_size = (remaining > config_.max_send_packet_size) ? 
                               config_.max_send_packet_size : remaining;
            
            // 使用::send表示调用系统函数而非成员函数
            ssize_t sent_bytes = ::send(sockfd, data_ptr, chunk_size, 0);
            if (sent_bytes <= 0)
            {
                LOG_ERROR("Failed to send chunk (error: {})", strerror(errno));
                success = false;
                break;
            }
            
            data_ptr += sent_bytes;
            remaining -= sent_bytes;
        }

        if (success)
            LOG_DEBUG("Successfully sent {} bytes", size);
        else
            LOG_ERROR("Failed to send complete message");
        
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
        std::string addr_port;
    };

    struct ConnectionInfo
    {
        SocketType fd;
        std::string remote_addr;
        int remote_port;
        std::string local_addr;
        int local_port;

        operator SocketType() const { return fd; }
    };

    void acceptorLoop()
    {
        LOG_INFO("Acceptor thread started");
        
        while (is_running_.load())
        {
            std::vector<ListeningSocket> sockets = getCurrentListenSockets();

            if (sockets.empty())
            {
                LOG_TRACE("No sockets to accept, sleeping");
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }

            // 使用poll检查哪些监听socket有新的连接
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

            // 处理有新的连接的socket
            for (size_t i = 0; i < pollfds.size(); ++i)
            {
                if (pollfds[i].revents & POLLIN)
                {
                    LOG_TRACE("New connection on socket {}", i);
                    acceptNewConnection(pollfds[i].fd);
                }
            }
        }
        LOG_INFO("Acceptor thread exiting");
    }

    void acceptNewConnection(SocketType listen_sock)
    {
        if (current_connections_.load() >= config_.max_connections)
        {
            LOG_WARNING("Maximum connections limit reached ({}), rejecting new connection",
                        config_.max_connections);
            return;
        }

        sockaddr_in client_addr = {};
        socklen_t addr_len = sizeof(client_addr);
        
        SocketType client_sock = accept(listen_sock, 
                                      reinterpret_cast<sockaddr*>(&client_addr), 
                                      &addr_len);
        if (client_sock == INVALID_SOCKET)
        {
            LOG_ERROR("Accept failed: {}", strerror(errno));
            return;
        }

        // 获取客户端信息
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        int client_port = ntohs(client_addr.sin_port);

        // 获取本地地址信息
        sockaddr_in local_addr = {};
        socklen_t local_addr_len = sizeof(local_addr);
        char local_ip[INET_ADDRSTRLEN] = {0};
        int local_port = 0;
        if (getsockname(listen_sock, (sockaddr *)&local_addr, &local_addr_len) == 0)
        {
            inet_ntop(AF_INET, &local_addr.sin_addr, local_ip, INET_ADDRSTRLEN);
            local_port = ntohs(local_addr.sin_port);
        }

        LOG_INFO("Accepted new connection from {}:{} to {}:{}", 
                client_ip, client_port, local_ip, local_port);

        // 设置socket选项
        setupSocketOptions(client_sock);

        // 添加到连接池
        ConnectionInfo conn;
        conn.fd = client_sock;
        conn.remote_addr = client_ip;
        conn.remote_port = client_port;
        conn.local_addr = local_ip;
        conn.local_port = local_port;
        
        std::lock_guard<std::mutex> lock(conn_mutex_);
        active_connections_[client_sock] = conn;
  
        current_connections_++;
    }

    void receiverLoop()
    {
        LOG_INFO("Receiver thread started");
        constexpr int BUFFER_SIZE = 65536;
        char buffer[BUFFER_SIZE];

        while (is_running_.load())
        {
            std::vector<SocketType> sockets = getCurrentConnections();

            if (sockets.empty())
            {
                LOG_TRACE("No connections to poll, sleeping");
                std::unique_lock<std::mutex> lock(conn_mutex_);
                stop_signal_.wait_for(lock, std::chrono::milliseconds(100));
                continue;
            }

            // 使用poll检查哪些连接有数据可读
#ifdef _WIN32
            std::vector<WSAPOLLFD> pollfds;
            for (const auto &sock : sockets)
            {
                pollfds.push_back({sock, POLLIN, 0});
            }
            int ret = WSAPoll(pollfds.data(), static_cast<ULONG>(pollfds.size()), 100);
#else
            std::vector<pollfd> pollfds;
            for (const auto &sock : sockets)
            {
                pollfds.push_back({sock, POLLIN, 0});
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

            // 处理有数据的连接
            for (size_t i = 0; i < pollfds.size(); ++i)
            {
                if (pollfds[i].revents & POLLIN)
                {
                    LOG_TRACE("Data available on connection {}", i);
                    processIncomingData(pollfds[i].fd);
                }
                else if (pollfds[i].revents & (POLLHUP | POLLERR | POLLNVAL))
                {
                    LOG_INFO("Connection closed or error detected");
                    closeConnection(pollfds[i].fd);
                }
            }
        }
        LOG_INFO("Receiver thread exiting");
    }

    void processIncomingData(SocketType sockfd)
    {
        // 使用配置的最大包大小
        std::vector<char> buffer;
        
        // 接收数据
        ssize_t recv_len = recv(sockfd, buffer.data(), buffer.size(), 0);
        if (recv_len <= 0)
        {
            if (recv_len == 0)
            {
                LOG_INFO("Connection closed by peer");
            }
            else
            {
                LOG_ERROR("recv failed: {}", strerror(errno));
            }
            closeConnection(sockfd);
            return;
        }
        
        LOG_DEBUG("Received {} bytes from socket {}", recv_len, sockfd);
        
        // 获取连接信息
        ConnectionInfo conn_info = getConnectionInfo(sockfd);
        if (conn_info.fd == INVALID_SOCKET)
        {
            LOG_ERROR("Failed to get connection info");
            return;
        }

        // 复制数据到共享内存
        auto msg_data = std::shared_ptr<void>(malloc(recv_len), free);
        memcpy(msg_data.get(), buffer.data(), recv_len);

        // 生成匹配键
        auto context = std::make_shared<MatchContext>();
        context->sender_key = createSubKey(conn_info.remote_addr, conn_info.remote_port);
        context->local_key = createSubKey(conn_info.local_addr, conn_info.local_port);
        context->wildcard_key = createSubKey("localhost", conn_info.local_port);
        context->any_key = createSubKey("", 0);

        // 生成处理任务
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
        TcpCommunicateCore::s_thread_pool_->enqueue(process_msg);
#else
        process_msg();
#endif
    }

    SocketType createAndBindSocket(const std::string &addr, int port)
    {
        // 创建TCP Socket
        SocketType sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sockfd == INVALID_SOCKET)
        {
            LOG_ERROR("Failed to create socket: {}", strerror(errno));
            return INVALID_SOCKET;
        }
        
        // 设置SO_REUSEADDR选项
        int opt = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
                      reinterpret_cast<const char *>(&opt), sizeof(opt)) == SOCKET_ERROR)
        {
            LOG_ERROR("Failed to set SO_REUSEADDR: {}", strerror(errno));
#ifdef _WIN32
            closesocket(sockfd);
#else
            close(sockfd);
#endif
            return INVALID_SOCKET;
        }

        // 设置TCP_NODELAY选项（禁用Nagle算法）
        if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, 
                      reinterpret_cast<const char *>(&opt), sizeof(opt)) == SOCKET_ERROR)
        {
            LOG_WARNING("Failed to set TCP_NODELAY: {}", strerror(errno));
        }

        // 初始化服务器地址结构
        sockaddr_in serv_addr = {};
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);
        serv_addr.sin_addr.s_addr = addr.empty() ? INADDR_ANY : inet_addr(addr.c_str());

        // 绑定Socket
        if (bind(sockfd, reinterpret_cast<const sockaddr *>(&serv_addr), sizeof(serv_addr)) == SOCKET_ERROR)
        {
            LOG_ERROR("Failed to bind socket to {}:{} - {}", 
                     addr.empty() ? "INADDR_ANY" : addr, port, strerror(errno));
#ifdef _WIN32
            closesocket(sockfd);
#else
            close(sockfd);
#endif
            return INVALID_SOCKET;
        }

        LOG_DEBUG("Successfully created and bound socket for {}:{}", 
                 addr.empty() ? "INADDR_ANY" : addr, port);
        return sockfd;
    }

    SocketType createAndConnectSocket(const std::string &addr, int port)
    {
        // 创建TCP Socket
        SocketType sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sockfd == INVALID_SOCKET)
        {
            LOG_ERROR("Failed to create socket: {}", strerror(errno));
            return INVALID_SOCKET;
        }

        // 设置socket选项
        setupSocketOptions(sockfd);

        // 设置连接超时
#ifdef _WIN32
        DWORD timeout = config_.connect_timeout_ms;
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout));
#else
        struct timeval tv;
        tv.tv_sec = config_.connect_timeout_ms / 1000;
        tv.tv_usec = (config_.connect_timeout_ms % 1000) * 1000;
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv));
#endif

        // 绑定源地址（如果配置）
        if (!config_.source_addr.source_ip.empty() || config_.source_addr.source_port > 0)
        {
            sockaddr_in local_addr = {};
            local_addr.sin_family = AF_INET;
            local_addr.sin_port = htons(config_.source_addr.source_port);
            if (!config_.source_addr.source_ip.empty())
            {
                inet_pton(AF_INET, config_.source_addr.source_ip.c_str(), &local_addr.sin_addr);
            }
            else
            {
                local_addr.sin_addr.s_addr = INADDR_ANY;
            }

            if (bind(sockfd, reinterpret_cast<const sockaddr*>(&local_addr),
                     sizeof(local_addr)) == SOCKET_ERROR)
            {
#ifdef _WIN32
                int error = WSAGetLastError();
                LOG_ERROR("Bind to {}:{} failed - {}", 
                     config_.source_addr.source_ip.empty() ? "ANY" : config_.source_addr.source_ip,
                     config_.source_addr.source_port, error);
                closesocket(sockfd);
#else
                LOG_ERROR("Bind to {}:{} failed - {}", 
                     config_.source_addr.source_ip.empty() ? "ANY" : config_.source_addr.source_ip,
                     config_.source_addr.source_port, strerror(errno));
                close(sockfd);
#endif
                return INVALID_SOCKET;
            }
        }

        // 连接服务器
        sockaddr_in serv_addr = {};
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);
        inet_pton(AF_INET, addr.c_str(), &serv_addr.sin_addr);

        if (connect(sockfd, reinterpret_cast<const sockaddr *>(&serv_addr), sizeof(serv_addr)) == SOCKET_ERROR)
        {
            LOG_ERROR("Failed to connect to {}:{} - {}", addr, port, strerror(errno));
#ifdef _WIN32
            closesocket(sockfd);
#else
            close(sockfd);
#endif
            return INVALID_SOCKET;
        }

        LOG_INFO("Connected to {}:{}", addr, port);
        return sockfd;
    }

    void setupSocketOptions(SocketType sockfd)
    {
        // 设置发送和接收超时
#ifdef _WIN32
        DWORD send_timeout = config_.send_timeout_ms;
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&send_timeout, sizeof(send_timeout));
        DWORD recv_timeout = config_.recv_timeout_ms;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&recv_timeout, sizeof(recv_timeout));
#else
        struct timeval tv;
        tv.tv_sec = config_.send_timeout_ms / 1000;
        tv.tv_usec = (config_.send_timeout_ms % 1000) * 1000;
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv));
        
        tv.tv_sec = config_.recv_timeout_ms / 1000;
        tv.tv_usec = (config_.recv_timeout_ms % 1000) * 1000;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
#endif

        // 设置TCP keepalive选项
        if (config_.keepalive_time != 0)
        {
            int opt = 1;
            if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, 
                          reinterpret_cast<const char *>(&opt), sizeof(opt)) == SOCKET_ERROR)
            {
                LOG_WARNING("Failed to set SO_KEEPALIVE: {}", strerror(errno));
            }
            
#ifdef __linux__
            // Linux特有的keepalive参数设置
            opt = 300;                      // 开始发送keepalive探测包前的空闲时间(秒)
            setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &opt, sizeof(opt));
            
            opt = config_.keepalive_time;   // keepalive 探测包发送间隔(秒)
            setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &opt, sizeof(opt));
            
            opt = 3;                        // 认为连接失效之前，最多发送的​​探测包次数​​
            setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, &opt, sizeof(opt));
#endif
        }

        // 设置TCP_NODELAY选项（禁用Nagle算法）
        int opt = 1;
        if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, 
                      reinterpret_cast<const char *>(&opt), sizeof(opt)) == SOCKET_ERROR)
        {
            LOG_WARNING("Failed to set TCP_NODELAY: {}", strerror(errno));
        }
    }

    void closeAllSockets()
    {
        LOG_DEBUG("Closing all sockets");
        
        {
            std::lock_guard<std::mutex> lock(socket_mutex_);
            for (const auto &sock : listen_sockets_)
            {
#ifdef _WIN32
                closesocket(sock.fd);
#else
                close(sock.fd);
#endif
            }
            listen_sockets_.clear();
        }
        
        {
            std::lock_guard<std::mutex> lock(conn_mutex_);
            for (auto &[_, conn] : connections_)
            {
#ifdef _WIN32
                closesocket(conn.fd);
#else
                close(conn.fd);
#endif
            }
            connections_.clear();
            
            for (auto &[_, sock] : active_connections_)
            {
#ifdef _WIN32
                closesocket(sock.fd);
#else
                close(sock.fd);
#endif
            }
            active_connections_.clear();
        }
    }

    std::vector<ListeningSocket> getCurrentListenSockets()
    {
        std::lock_guard<std::mutex> lock(socket_mutex_);
        return listen_sockets_;
    }

    std::vector<SocketType> getCurrentConnections()
    {
        std::lock_guard<std::mutex> lock(conn_mutex_);
        std::vector<SocketType> sockets;
        for (const auto &[sockfd, conn] : active_connections_)
        {
            sockets.push_back(sockfd);
        }
        return sockets;
    }

    ConnectionInfo getConnectionInfo(SocketType sockfd)
    {
        std::lock_guard<std::mutex> lock(conn_mutex_);
        auto it = active_connections_.find(sockfd);
        if (it != active_connections_.end())
        {
            return it->second;
        }
        return {INVALID_SOCKET};
    }

    void closeConnection(SocketType sockfd)
    {
        std::lock_guard<std::mutex> lock(conn_mutex_);
        auto it = active_connections_.find(sockfd);
        if (it != active_connections_.end())
        {
#ifdef WIN32
            closesocket(sockfd);
#else
            close(sockfd);
#endif
            active_connections_.erase(it);
            LOG_INFO("Closed connection {}", sockfd);

            current_connections_--;
        }
    }

    void cleanConnections()
    {
        LOG_TRACE("Clean up all connections");
        std::lock_guard<std::mutex> lock(conn_mutex_);
        for (auto& [_, conn] : connections_) {
            if (conn.fd != INVALID_SOCKET) {
#ifdef _WIN32
                closesocket(conn.fd);
#else
                close(conn.fd);
#endif
            }
        }
        connections_.clear();

        current_connections_.store(0);
    }

    SocketType getConnection(const std::string &addr, int port)
    {
        std::string key = addr + ":" + std::to_string(port);
        std::lock_guard<std::mutex> lock(conn_mutex_);
        auto it = connections_.find(key);
        return it != connections_.end() ? it->second.fd : INVALID_SOCKET;
    }

    struct MatchContext
    {
        std::string sender_key;
        std::string local_key;
        std::string wildcard_key;
        std::string any_key;
    };

private:
    std::atomic<bool> is_running_;
    CoreConfig& config_;
    std::atomic<int> current_connections_{0};
    std::thread acceptor_thread_;
    std::thread receiver_thread_;
    std::mutex socket_mutex_;
    std::mutex conn_mutex_;
    std::shared_mutex sub_mutex_;
    std::condition_variable stop_signal_;
    std::vector<ListeningSocket> listen_sockets_;
    std::unordered_map<std::string, ConnectionInfo> connections_;       // 主动连接池
    std::unordered_map<SocketType, ConnectionInfo> active_connections_; // 所有活动连接
    std::unordered_map<std::string, communicate::SubscribebBase *> subscribers_;
};

#ifdef THREAD_POOL_MODE
// 初始化共享线程池（作为static成员）
std::unique_ptr<ThreadPoolWrapper> TcpCommunicateCore::s_thread_pool_ = nullptr;
#endif

TcpCommunicateCore::TcpCommunicateCore() : pimpl_(std::make_unique<Impl>(m_config))
{
    LOG_TRACE("TcpCommunicateCore constructor");
}

TcpCommunicateCore::~TcpCommunicateCore()
{
    LOG_TRACE("TcpCommunicateCore destructor");
}

int TcpCommunicateCore::initialize()
{
    LOG_INFO("Initializing TCP communication core");
    auto &cfg = SingletonTemplate<ConfigWrapper>::getSingletonInstance().getCfgInstance();

    // 加载配置
    m_config.max_send_packet_size = cfg.getValue("max_send_packet_size", 1024);
    m_config.recv_timeout_ms = cfg.getValue("recv_timeout_ms", 100);
    m_config.send_timeout_ms = cfg.getValue("send_timeout_ms", 100);
    m_config.connect_timeout_ms = cfg.getValue("connect_timeout_ms", 5000);
    m_config.source_addr.source_port = cfg.getValue("source_port", 0);
    m_config.source_addr.source_ip = cfg.getValue("source_ip", (std::string)"");
    m_config.thread_pool_size = cfg.getValue("thread_pool_size", 3);
    m_config.max_connections = cfg.getValue("max_connections", 100);
    m_config.listen_backlog = cfg.getValue("listen_backlog", 10);
    m_config.keepalive_time = cfg.getValue("keepalive", 60);

    LOG_DEBUG("Configuration loaded - max_send: {}, send_timeout: {}ms, recv_timeout: {}ms, connect_timeout: {}ms, source_addr: {}:{}, thread_pool: {}",
              m_config.max_send_packet_size,
              m_config.send_timeout_ms, m_config.recv_timeout_ms, m_config.connect_timeout_ms,
              m_config.source_addr.source_ip, m_config.source_addr.source_port,
              m_config.thread_pool_size);
#ifdef THREAD_POOL_MODE
    s_thread_pool_ = std::make_unique<ThreadPoolWrapper>(m_config.thread_pool_size);
    LOG_DEBUG("Created thread pool with size: {}", m_config.thread_pool_size);
#endif

    // 加载监听地址列表
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

    // 加载连接地址列表
    auto connect_list = cfg.getList<ConfigInterface::CommInfo>("connect_list");
    for (const auto &item : connect_list)
    {
        LOG_DEBUG("Adding connect address: {}:{}", item.IP, item.Port);
        if (!pimpl_->connectToServer(item.IP, item.Port))
        {
            LOG_ERROR("Failed to connect to {}:{}", item.IP, item.Port);
            return -1;
        }
    }

    LOG_INFO("TCP communication core initialized successfully");
    return 0;
}

bool TcpCommunicateCore::send(const std::string &dest_addr, int dest_port,
                              const void *data, size_t size)
{
    return pimpl_->sendData(dest_addr, dest_port, data, size);
}

int TcpCommunicateCore::addListenAddr(const char *addr, int port)
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

int TcpCommunicateCore::addSubscribe(const char *addr, int port,
                                     communicate::SubscribebBase *sub)
{
    std::string addr_str(addr ? addr : "");
    std::string key = TcpCommunicateCore::Impl::createSubKey(addr_str, port);
    LOG_DEBUG("Adding subscriber for key: {}", key);
    pimpl_->addSubscriber(key, sub);
    pimpl_->start();
    return 0;
}

void TcpCommunicateCore::shutdown()
{
    LOG_INFO("Shutting down TCP communication core");
    pimpl_->stop();
}

void TcpCommunicateCore::setDefSource(int port, std::string ip)
{
    LOG_DEBUG("Setting source addr to {}:{}", ip, port);
    m_config.source_addr.source_port = port;
    m_config.source_addr.source_ip= ip;
}