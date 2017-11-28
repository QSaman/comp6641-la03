#include "udp_packet.h"
#include "binary_stream.h"

#include <iostream>
#include <boost/endian/conversion.hpp>

void UdpPacket::marshall()
{
    std::ostringstream oss;
    saman::binary_write(oss, packet_type);
    auto b_seq_num = boost::endian::native_to_big(seq_num);
    saman::binary_write(oss, b_seq_num);
    auto b_peer_ipv4 = boost::endian::native_to_big(peer_ipv4);
    saman::binary_write(oss, b_peer_ipv4);
    auto b_peer_port = boost::endian::native_to_big(peer_port);
    saman::binary_write(oss, b_peer_port);
    saman::binary_write(oss, boost::endian::native_to_big(ack_number));
    saman::binary_write(oss, data);

    data.clear();
    marshalled_message = oss.str();
}

void UdpPacket::unmarshall(const std::string& network_message, bool ignore_data)
{
    unsigned int header_len = 0;
    std::istringstream iss(network_message);
    saman::binary_read(iss, packet_type);
    header_len += sizeof(packet_type);
    saman::binary_read(iss, seq_num);
    header_len += sizeof(seq_num);
    boost::endian::big_to_native_inplace(seq_num);
    saman::binary_read(iss, peer_ipv4);
    header_len += sizeof(peer_ipv4);
    boost::endian::big_to_native_inplace(peer_ipv4);
    saman::binary_read(iss, peer_port);
    header_len += sizeof(peer_port);
    boost::endian::big_to_native_inplace(peer_port);
    saman::binary_read(iss, ack_number);
    header_len += sizeof(ack_number);
    boost::endian::big_to_native_inplace(ack_number);
    if (!ignore_data)
    {
        unsigned int msg_len = network_message.length() - header_len;
        saman::binary_read_string(iss, data, msg_len);
    }
}

void UdpPacket::setPeerIpV4(const std::string& ipv4_address)
{
    Ipv4 segment[4];
    char dot;
    std::istringstream iss(ipv4_address);
    iss.exceptions(std::istringstream::badbit | std::istringstream::failbit);
    try
    {
        iss >> segment[3] >> dot >> segment[2] >> dot >> segment[1] >> dot >> segment[0];
    }
    catch (...)
    {
        throw std::invalid_argument("Invalid IP addresss: " + ipv4_address);
    }
    peer_ipv4 = 0;
    for (unsigned int i = 0; i < 4; ++i)
    {
        if (segment[i] > 255)
            throw std::invalid_argument("Invalid IP addresss: " + ipv4_address);
        peer_ipv4 |= segment[i] << (8 * i);
    }
}

std::string UdpPacket::peerIpV4()
{
    std::ostringstream oss;
    for (int i = 3; i >= 0; --i)
    {
        oss << ((peer_ipv4 >> (8 * i)) & 0xff);
        if (i > 0)
            oss << '.';
    }
    return oss.str();
}

//std::ostream&operator<<(std::ostream& out, PacketType packet_type)
//{
//    return out << std::underlying_type<PacketType>::type(packet_type);
//}
