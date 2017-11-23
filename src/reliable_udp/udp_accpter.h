#pragma once

#include <iosfwd>
#include <asio.hpp>

#include "reliable_udp.h"

class UdpPassiveSocket
{
public:
    UdpPassiveSocket(asio::io_service& io_service, unsigned short port);
    void accept(ReliableUdp& reliable_udp);
private:
    asio::ip::udp::socket socket;
    asio::io_service& io_service;
};
