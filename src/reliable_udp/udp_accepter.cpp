#include "udp_accpter.h"
#include <string>

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
    udp::endpoint sender_endpoint;
    auto length = socket.receive_from(asio::buffer(data, max_udp_packet_length), sender_endpoint);
    UdpPacket packet;
    std::string message(data, length);
    packet.unmarshall(message);
    if (!packet.synPacket())
        return;
    reliable_udp.completeThreewayHandshake(packet);
}

