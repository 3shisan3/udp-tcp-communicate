#ifndef UDP_COMMUNICATE_H
#define UDP_COMMUNICATE_H

#include <string>
#include <netinet/in.h>

class UdpCommunicate
{
public:
    UdpCommunicate();
    ~UdpCommunicate();

    // 初始化UDP套接字
    bool initializeSocket(int port);

    // 向指定地址和端口发送数据
    bool sendData(const std::string &data, const std::string &address, int port);

    // 从套接字接收数据
    std::string receiveData();

private:
    int m_udpSocket_;                 // UDP套接字
    struct sockaddr_in m_localAddr_;  // 本地地址
    struct sockaddr_in m_remoteAddr_; // 远程地址
};

#endif // UDP_COMMUNICATE_H