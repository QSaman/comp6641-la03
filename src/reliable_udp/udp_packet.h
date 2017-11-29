#pragma once

#include <iosfwd>
#include <string>
#include <cstdint>
#include <sstream>

using SeqNum = std::uint32_t;
using Ipv4 = std::uint32_t;
using PortNo = std::uint16_t;
using PacketType = std::uint8_t;

enum  PacketTypeMask
{
    Syn = 0x01,
    Fin = 0x02,
    Ack = 0x04,
    Rst = 0x08,
    Data = 0x10
};

//std::ostream& operator<<(std::ostream& out, PacketType packet_type);

//1 byte for packet type, 4 bytes for sequence number, 4 bytes for peer address, 2 bytes for peer port
//4 bytes for ack number, maximum 1009 bytes for payload
class UdpPacket
{
public:
    UdpPacket() {valid = false;}
    void marshall();
    void unmarshall(const std::string& network_message, bool ignore_data = false);
    void setPeerIpV4(const std::string& ipv4_address);
    std::string peerIpV4();
    inline void setSynAck() {packet_type |= (PacketTypeMask::Ack | PacketTypeMask::Syn);}
    inline void setAck() {packet_type |= PacketTypeMask::Ack;}
    inline void setSyn() {packet_type |= PacketTypeMask::Syn;}
    inline bool synPacket() {return (packet_type & PacketTypeMask::Syn) != 0;}
    inline bool ackPacket() {return (packet_type & PacketTypeMask::Ack) != 0;}
    inline bool dataPacket() {return (packet_type & PacketTypeMask::Data) != 0;}
    inline bool synAckPacket() {return (packet_type & (PacketTypeMask::Ack | PacketTypeMask::Syn)) != 0;}
    inline void resetAck() {packet_type &= ~PacketTypeMask::Ack;}
    inline void resetData() {packet_type &= ~PacketTypeMask::Data;}
    inline void clearPacketType() {packet_type = 0x00;}
public:
    PacketType packet_type;
    SeqNum seq_num, ack_number;
    Ipv4 peer_ipv4;
    PortNo peer_port;
    std::string data;
public:
    //The following fileds are not part of marshalling and unmarshalling
    std::string marshalled_message;
    bool valid;
};
