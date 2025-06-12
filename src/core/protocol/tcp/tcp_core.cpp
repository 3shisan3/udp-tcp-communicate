/* #include "tcp_core.h"
#include <cstring>
#include <iostream>
#include <functional>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

class TcpCommunicateCore::Impl
{
public:
    Impl(CoreConfig& config) : is_running_(false), config_(config)
    {
        LOG_TRACE("TCP Core Impl constructor");
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
        LOG_TRACE("TCP Core Impl destructor");
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
            LOG_INFO("Starting TCP listener threads");
            for (auto& listener : listeners_)
            {
                listener.thread = std::thread(&Impl::listenerLoop, this, listener.fd);
            }
        }
    }

    void stop()
    {
        if (is_running_.exchange(false))
        {
            LOG_INFO("Stopping TCP communication");
            
            // Close all connections
            {
                std::lock_guard<std::mutex> lock(conn_mutex_);
                for (auto& conn : connections_)
                {
#ifdef _WIN32
                    closesocket(conn.fd);
#else
                    close(conn.fd);
#endif
                }
                connections_.clear();
            }
            
            // Close all listener sockets
            {
                std::lock_guard<std::mutex> lock(listener_mutex_);
                for (auto& listener : listeners_)
                {
#ifdef _WIN32
                    closesocket(listener.fd);
#else
                    close(listener.fd);
#endif
                    if (listener.thread.joinable())
                    {
                        listener.thread.join();
                    }
                }
                listeners_.clear();
            }
        }
    }

    bool addListeningSocket(const std::string &addr, int port)
    {
        std::lock_guard<std::mutex> lock(listener_mutex_);

        // Check if port is already being listened on
        for (const auto &listener : listeners_)
        {
            if (listener.port == port)
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

        // Start listening
        if (listen(sockfd, config_.max_connections) == SOCKET_ERROR)
        {
            LOG_ERROR("Failed to listen on socket: {}", strerror(errno));
#ifdef _WIN32
            closesocket(sockfd);
#else
            close(sockfd);
#endif
            return false;
        }

        listeners_.push_back({sockfd, port, std::thread()});
        LOG_INFO("Added listening socket for {}:{}", addr, port);
        
        if (is_running_.load())
        {
            // Start listener thread if we're already running
            listeners_.back().thread = std::thread(&Impl::listenerLoop, this, sockfd);
        }
        
        return true;
    }

    bool sendData(const std::string &dest_addr, int dest_port,
                  const void *data, size_t size)
    {
        LOG_TRACE("Attempting to send {} bytes to {}:{}", size, dest_addr, dest_port);
        std::lock_guard<std::mutex> lock(send_mutex_);

        // Check for existing connection
        std::string conn_key = createConnKey(dest_addr, dest_port);
        SocketType sockfd = INVALID_SOCKET;
        
        {
            std::lock_guard<std::mutex> lock(conn_mutex_);
            auto it = connections_.find(conn_key);
            if (it != connections_.end())
            {
                sockfd = it->second.fd;
            }
        }

        // Create new connection if needed
        if (sockfd == INVALID_SOCKET)
        {
            sockfd = createAndConnectSocket(dest_addr, dest_port);
            if (sockfd == INVALID_SOCKET)
            {
                LOG_ERROR("Failed to create/connect socket for {}:{}", dest_addr, dest_port);
                return false;
            }
            
            // Add to connections map
            std::lock_guard<std::mutex> lock(conn_mutex_);
            connections_[conn_key] = {sockfd, dest_addr, dest_port};
        }

        // Send data
        const char *data_ptr = reinterpret_cast<const char *>(data);
        size_t remaining = size;
        bool success = true;
        
        while (remaining > 0)
        {
            size_t chunk_size = (remaining > config_.max_send_packet_size) ? 
                               config_.max_send_packet_size : remaining;

            ssize_t sent_bytes = send(sockfd, data_ptr, chunk_size, 0);
            if (sent_bytes <= 0)
            {
                LOG_ERROR("Failed to send data to {}:{} - {}", dest_addr, dest_port, strerror(errno));
                success = false;
                break;
            }
            
            data_ptr += sent_bytes;
            remaining -= sent_bytes;
        }

        if (success)
        {
            LOG_DEBUG("Successfully sent {} bytes to {}:{}", size, dest_addr, dest_port);
        }
        else
        {
            // Remove failed connection
            std::lock_guard<std::mutex> lock(conn_mutex_);
            connections_.erase(conn_key);
#ifdef _WIN32
            closesocket(sockfd);
#else
            close(sockfd);
#endif
        }
        
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

    static std::string createConnKey(const std::string &addr, int port)
    {
        return addr + ":" + std::to_string(port);
    }

private:
    struct Connection
    {
        SocketType fd;
        std::string addr;
        int port;
    };

    struct Listener
    {
        SocketType fd;
        int port;
        std::thread thread;
    };

    void listenerLoop(SocketType listen_fd)
    {
        LOG_INFO("Listener thread started for socket {}", listen_fd);
        
        while (is_running_.load())
        {
            sockaddr_in client_addr = {};
            socklen_t client_len = sizeof(client_addr);
            
            SocketType client_fd = accept(listen_fd, 
                                        reinterpret_cast<sockaddr*>(&client_addr), 
                                        &client_len);
            
            if (client_fd == INVALID_SOCKET)
            {
                if (is_running_.load())
                {
                    LOG_ERROR("Accept failed: {}", strerror(errno));
                }
                continue;
            }
            
            // Get client info
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            int client_port = ntohs(client_addr.sin_port);
            
            // Get local port
            sockaddr_in local_addr = {};
            socklen_t local_len = sizeof(local_addr);
            getsockname(client_fd, reinterpret_cast<sockaddr*>(&local_addr), &local_len);
            int local_port = ntohs(local_addr.sin_port);
            
            LOG_INFO("Accepted connection from {}:{} on port {}", 
                     client_ip, client_port, local_port);
            
            // Set socket options
            setSocketOptions(client_fd);
            
            // Add to connections map
            std::string conn_key = createConnKey(client_ip, client_port);
            {
                std::lock_guard<std::mutex> lock(conn_mutex_);
                connections_[conn_key] = {client_fd, client_ip, client_port};
            }
            
            // Start receiver thread for this connection
#ifdef THREAD_POOL_MODE
            thread_pool_->enqueue([this, client_fd, client_ip, client_port, local_port]() {
                receiveLoop(client_fd, client_ip, client_port, local_port);
            });
#else
            std::thread([this, client_fd, client_ip, client_port, local_port]() {
                receiveLoop(client_fd, client_ip, client_port, local_port);
            }).detach();
#endif
        }
        
        LOG_INFO("Listener thread exiting for socket {}", listen_fd);
    }

    void receiveLoop(SocketType sockfd, const std::string& client_ip, 
                    int client_port, int local_port)
    {
        LOG_DEBUG("Receiver thread started for {}:{} on port {}", 
                 client_ip, client_port, local_port);
        
        std::vector<char> buffer(config_.max_receive_packet_size);
        
        while (is_running_.load())
        {
            ssize_t recv_len = recv(sockfd, buffer.data(), buffer.size(), 0);
            
            if (recv_len <= 0)
            {
                if (recv_len == 0)
                {
                    LOG_INFO("Connection closed by {}:{}", client_ip, client_port);
                }
                else
                {
                    LOG_ERROR("recv failed: {}", strerror(errno));
                }
                break;
            }
            
            LOG_DEBUG("Received {} bytes from {}:{}", recv_len, client_ip, client_port);
            
            // Copy data to shared memory
            auto msg_data = std::shared_ptr<void>(malloc(recv_len), free);
            memcpy(msg_data.get(), buffer.data(), recv_len);
            
            // Create context
            struct MatchContext
            {
                std::string sender_key;
                std::string local_key;
                std::string wildcard_key;
                std::string any_key;
            };
            
            auto context = std::make_shared<MatchContext>();
            context->sender_key = createConnKey(client_ip, client_port);
            context->local_key = createConnKey("", local_port);
            context->wildcard_key = createConnKey("localhost", local_port);
            context->any_key = createConnKey("", 0);
            
            // Process message
            auto process_msg = [this, context, msg_data] {
                if (auto sub = getSubscriber(context->sender_key) ?:
                               getSubscriber(context->local_key) ?:
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
        
        // Clean up
        {
            std::lock_guard<std::mutex> lock(conn_mutex_);
            connections_.erase(createConnKey(client_ip, client_port));
        }
        
#ifdef _WIN32
        closesocket(sockfd);
#else
        close(sockfd);
#endif
        
        LOG_DEBUG("Receiver thread exiting for {}:{}", client_ip, client_port);
    }

    SocketType createAndBindSocket(const std::string &addr, int port)
    {
        SocketType sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sockfd == INVALID_SOCKET)
        {
            LOG_ERROR("Failed to create socket: {}", strerror(errno));
            return INVALID_SOCKET;
        }
        
        // Set SO_REUSEADDR
        int opt = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
                      reinterpret_cast<const char*>(&opt), sizeof(opt)) == SOCKET_ERROR)
        {
            LOG_ERROR("Failed to set SO_REUSEADDR: {}", strerror(errno));
#ifdef _WIN32
            closesocket(sockfd);
#else
            close(sockfd);
#endif
            return INVALID_SOCKET;
        }
        
        // Bind
        sockaddr_in serv_addr = {};
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port);
        serv_addr.sin_addr.s_addr = addr.empty() ? INADDR_ANY : inet_addr(addr.c_str());
        
        if (bind(sockfd, reinterpret_cast<const sockaddr*>(&serv_addr), 
                sizeof(serv_addr)) == SOCKET_ERROR)
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

    SocketType createAndConnectSocket(const std::string &dest_addr, int dest_port)
    {
        SocketType sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sockfd == INVALID_SOCKET)
        {
            LOG_ERROR("Failed to create socket: {}", strerror(errno));
            return INVALID_SOCKET;
        }
        
        // Set socket options
        setSocketOptions(sockfd);
        
        // Bind to source port if specified
        if (config_.source_port > 0)
        {
            sockaddr_in local_addr = {};
            local_addr.sin_family = AF_INET;
            local_addr.sin_port = htons(config_.source_port);
            local_addr.sin_addr.s_addr = INADDR_ANY;
            
            if (bind(sockfd, reinterpret_cast<const sockaddr*>(&local_addr),
                    sizeof(local_addr)) == SOCKET_ERROR)
            {
                LOG_ERROR("Bind failed: {}", strerror(errno));
#ifdef _WIN32
                closesocket(sockfd);
#else
                close(sockfd);
#endif
                return INVALID_SOCKET;
            }
        }
        
        // Connect
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
            return INVALID_SOCKET;
        }
        
        // Set non-blocking for connect with timeout
#ifdef _WIN32
        u_long mode = 1;
        ioctlsocket(sockfd, FIONBIO, &mode);
#else
        fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) | O_NONBLOCK);
#endif
        
        // Start non-blocking connect
        int ret = connect(sockfd, reinterpret_cast<const sockaddr*>(&dest_addr_in),
                         sizeof(dest_addr_in));
        
        if (ret == SOCKET_ERROR)
        {
#ifdef _WIN32
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK)
            {
                LOG_ERROR("Connect failed immediately: {}", error);
                closesocket(sockfd);
                return INVALID_SOCKET;
            }
#else
            if (errno != EINPROGRESS)
            {
                LOG_ERROR("Connect failed immediately: {}", strerror(errno));
                close(sockfd);
                return INVALID_SOCKET;
            }
#endif
            
            // Wait for connection with timeout
            fd_set writefds;
            FD_ZERO(&writefds);
            FD_SET(sockfd, &writefds);
            
            timeval tv;
            tv.tv_sec = config_.connect_timeout_ms / 1000;
            tv.tv_usec = (config_.connect_timeout_ms % 1000) * 1000;
            
            ret = select(static_cast<int>(sockfd + 1), nullptr, &writefds, nullptr, &tv);
            if (ret <= 0)
            {
                LOG_ERROR("Connect timeout or error");
#ifdef _WIN32
                closesocket(sockfd);
#else
                close(sockfd);
#endif
                return INVALID_SOCKET;
            }
            
            // Check for errors
            int error = 0;
            socklen_t len = sizeof(error);
            getsockopt(sockfd, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &len);
            
            if (error != 0)
            {
                LOG_ERROR("Connect failed: {}", strerror(error));
#ifdef _WIN32
                closesocket(sockfd);
#else
                close(sockfd);
#endif
                return INVALID_SOCKET;
            }
        }
        
        // Set back to blocking
#ifdef _WIN32
        mode = 0;
        ioctlsocket(sockfd, FIONBIO, &mode);
#else
        fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL, 0) & ~O_NONBLOCK);
#endif
        
        LOG_DEBUG("Successfully connected to {}:{}", dest_addr, dest_port);
        return sockfd;
    }

    void setSocketOptions(SocketType sockfd)
    {
        // Set send timeout
#ifdef _WIN32
        DWORD send_timeout = config_.send_timeout_ms;
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, 
                  reinterpret_cast<const char*>(&send_timeout), sizeof(send_timeout));
#else
        timeval tv;
        tv.tv_sec = config_.send_timeout_ms / 1000;
        tv.tv_usec = (config_.send_timeout_ms % 1000) * 1000;
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, 
                  reinterpret_cast<const char*>(&tv), sizeof(tv));
#endif
        
        // Set receive timeout
#ifdef _WIN32
        DWORD recv_timeout = config_.recv_timeout_ms;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, 
                  reinterpret_cast<const char*>(&recv_timeout), sizeof(recv_timeout));
#else
        tv.tv_sec = config_.recv_timeout_ms / 1000;
        tv.tv_usec = (config_.recv_timeout_ms % 1000) * 1000;
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, 
                  reinterpret_cast<const char*>(&tv), sizeof(tv));
#endif
        
        // Enable keepalive
        int opt = 1;
        setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, 
                  reinterpret_cast<const char*>(&opt), sizeof(opt));
        
#ifdef __linux__
        // Linux-specific keepalive settings
        opt = config_.keepalive_time;
        setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPIDLE, &opt, sizeof(opt));
        opt = 1;
        setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPINTVL, &opt, sizeof(opt));
        opt = 3;
        setsockopt(sockfd, IPPROTO_TCP, TCP_KEEPCNT, &opt, sizeof(opt));
#endif
    }

    std::atomic<bool> is_running_;
    CoreConfig& config_;
#ifdef THREAD_POOL_MODE
    std::unique_ptr<ThreadPoolWrapper> thread_pool_;
#endif
    std::mutex send_mutex_;
    std::shared_mutex sub_mutex_;
    std::mutex listener_mutex_;
    std::mutex conn_mutex_;
    std::vector<Listener> listeners_;
    std::unordered_map<std::string, Connection> connections_;
    std::unordered_map<std::string, communicate::SubscribebBase*> subscribers_;
};

// TcpCommunicateCore method implementations
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
    
    m_config.max_send_packet_size = cfg.getValue("max_send_packet_size", 1024);
    m_config.max_receive_packet_size = cfg.getValue("max_receive_packet_size", 65535);
    m_config.recv_timeout_ms = cfg.getValue("recv_timeout_ms", 100);
    m_config.send_timeout_ms = cfg.getValue("send_timeout_ms", 100);
    m_config.connect_timeout_ms = cfg.getValue("connect_timeout_ms", 3000);
    m_config.source_port = cfg.getValue("source_port", 0);
    m_config.thread_pool_size = cfg.getValue("thread_pool_size", 3);
    m_config.max_connections = cfg.getValue("max_connections", 10);
    m_config.keepalive_time = cfg.getValue("keepalive_time", 60);
    
    LOG_DEBUG("Configuration loaded - max_send: {}, max_recv: {}, send_timeout: {}ms, recv_timeout: {}ms, connect_timeout: {}ms, source_port: {}, thread_pool: {}, max_connections: {}, keepalive: {}s",
              m_config.max_send_packet_size, m_config.max_receive_packet_size,
              m_config.send_timeout_ms, m_config.recv_timeout_ms,
              m_config.connect_timeout_ms, m_config.source_port,
              m_config.thread_pool_size, m_config.max_connections,
              m_config.keepalive_time);
    
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
    
    pimpl_->start();
    LOG_INFO("TCP communication core initialized successfully");
    return 0;
}

bool TcpCommunicateCore::send(const std::string &dest_addr, int dest_port,
                             const void *data, size_t size)
{
    LOG_DEBUG("Sending data to {}:{} (size: {})", dest_addr, dest_port, size);
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

int TcpCommunicateCore::addSubscribe(const char *addr, int port, communicate::SubscribebBase *sub)
{
    std::string addr_str(addr ? addr : "");
    std::string key = TcpCommunicateCore::Impl::createConnKey(addr_str, port);
    LOG_DEBUG("Adding subscriber for key: {}", key);
    pimpl_->addSubscriber(key, sub);
    return 0;
}

void TcpCommunicateCore::shutdown()
{
    LOG_INFO("Shutting down TCP communication core");
    pimpl_->stop();
}

void TcpCommunicateCore::setSendPort(int port)
{
    LOG_DEBUG("Setting source port to {}", port);
    m_config.source_port = port;
} */