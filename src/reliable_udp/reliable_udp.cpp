#include "reliable_udp.h"
#include "udp_packet.h"

//#include <cereal/archives/portable_binary.hpp>
////#include <cereal/archives/binary.hpp>
#include <fstream>
#include <iostream>

void testMarshalling()
{
    UdpPacket packet;
    packet.packet_type = PacketType::Rst;
    packet.seq_num = 1;
    packet.setPeerIpV4("192.168.0.1");
    packet.peer_port = 45678;
    packet.data = "GET /ip HTTP/1.0\r\nHost: httpbin.org\r\n\r\n";
    auto res = packet.marshall();
    std::ofstream os("out.cereal", std::ios::binary);
    os << res;
    UdpPacket packet2;
    packet2.unmarshall(res);
    using namespace std;
    if (packet2.packet_type == PacketType::Rst)
        cout << "Yes" << endl;
    else
        cout << "No" << endl;
    std::cout << packet2.data << endl;
    cout << packet2.seq_num << ' ' << packet2.peerIpV4() << ' ' << packet2.peer_port << endl;
}

