#include "udp_communicate.h"

#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>

UdpCommunicate::UdpCommunicate() : m_udpSocket_(-1)
{
    memset(&m_localAddr_, 0, sizeof(m_localAddr_));
    memset(&m_remoteAddr_, 0, sizeof(m_remoteAddr_));
}

UdpCommunicate::~UdpCommunicate()
{
    if (m_udpSocket_ != -1)
    {
        close(m_udpSocket_);
    }
}

bool UdpCommunicate::initializeSocket(int port)
{
    m_udpSocket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (m_udpSocket_ < 0)
    {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }

    m_localAddr_.sin_family = AF_INET;
    m_localAddr_.sin_addr.s_addr = INADDR_ANY;
    m_localAddr_.sin_port = htons(port);

    if (bind(m_udpSocket_, (struct sockaddr *)&m_localAddr_, sizeof(m_localAddr_)) < 0)
    {
        std::cerr << "Failed to bind socket" << std::endl;
        return false;
    }

    return true;
}

bool UdpCommunicate::sendData(const std::string &data, const std::string &address, int port)
{
    m_remoteAddr_.sin_family = AF_INET;
    m_remoteAddr_.sin_port = htons(port);
    if (inet_pton(AF_INET, address.c_str(), &m_remoteAddr_.sin_addr) <= 0)
    {
        std::cerr << "Invalid address" << std::endl;
        return false;
    }

    if (sendto(m_udpSocket_, data.c_str(), data.size(), 0, (struct sockaddr *)&m_remoteAddr_, sizeof(m_remoteAddr_)) < 0)
    {
        std::cerr << "Failed to send data" << std::endl;
        return false;
    }

    return true;
}

std::string UdpCommunicate::receiveData()
{
    char buffer[1024];
    socklen_t addrLen = sizeof(m_remoteAddr_);
    ssize_t len = recvfrom(m_udpSocket_, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&m_remoteAddr_, &addrLen);
    if (len < 0)
    {
        std::cerr << "Failed to receive data" << std::endl;
        return "";
    }

    buffer[len] = '\0';
    return std::string(buffer);
}