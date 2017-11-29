#include "udp_accpter.h"
#include <string>

#include <iostream>

//void UdpActiveSocket::read(std::string& message)
//{
//    std::unique_ptr<char> buffer(new char[packet_length]);
//    auto bytes = socket.receive(asio::buffer(buffer.get(), packet_length));
//    message = std::string(buffer.get(), bytes);
//}

using asio::ip::udp;

UdpPassiveSocket::UdpPassiveSocket(asio::io_service& io_service, unsigned short port) :
    socket(io_service, udp::endpoint(udp::v4(), port)), io_service(io_service)
{
}

void UdpPassiveSocket::accept(ReliableUdp& reliable_udp)
{
    char data[max_udp_packet_length];
    UdpPacket packet;
    udp::endpoint sender_endpoint;
    while (true)
    {
        auto length = socket.receive_from(asio::buffer(data, max_udp_packet_length), sender_endpoint);
        std::string message(data, length);
        packet.unmarshall(message);
        if (!packet.synPacket())
            continue;
        std::cout << "Before threeway handshaking on the server" << std::endl;
        if (reliable_udp.completeThreewayHandshake(packet, sender_endpoint))
            break;
    }
    std::cout << "After threeway handshaking on the server" << std::endl;
}

