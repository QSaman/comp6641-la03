#pragma once

#include <iosfwd>
#include <string>
#include <cstdint>

using SeqNum = std::uint32_t;
using Ipv4 = std::uint32_t;
using PortNo = std::uint16_t;

enum class PacketType : std::uint8_t
{
    Data = 0x00,
    Syn = 0x01,
    Fin = 0x02,
    Rst = 0x04
};

std::ostream& operator<<(std::ostream& out, PacketType packet_type);

class UdpPacket
{
public:
    std::string marshall() const;
    void unmarshall(const std::string& network_message);
    void setPeerIpV4(const std::string& ipv4_address);
    std::string peerIpV4();
public:
    PacketType packet_type;
    SeqNum seq_num;
    Ipv4 peer_ipv4;
    PortNo peer_port;
    std::string data;
};
