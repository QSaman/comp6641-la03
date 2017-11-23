#include "reliable_udp.h"
#include "udp_packet.h"

#include <random>
#include <fstream>
#include <iostream>

using asio::ip::udp;

void testMarshalling()
{
    UdpPacket packet;
    packet.packet_type = PacketTypeMask::Rst;
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
    if (packet2.packet_type == PacketTypeMask::Rst)
        cout << "Yes" << endl;
    else
        cout << "No" << endl;
    std::cout << packet2.data << endl;
    cout << packet2.seq_num << ' ' << packet2.peerIpV4() << ' ' << packet2.peer_port << endl;
}

void ReliableUdp::srWrite()
{
    while (true)
    {
        std::unique_lock<std::mutex> lock(send_queue_mutex);
        send_cv.wait(lock, [this]{return !send_queue.empty() || !accept_request;});
        if (!accept_request)
            break;
        const auto& packet = send_queue.front();
        write(packet);
        //TODO
    }
}

void ReliableUdp::srRead()
{
    std::unique_ptr<char> buffer(new char[max_udp_packet_length]);
    while (accept_request)
    {
        auto length = socket.receive(asio::buffer(buffer.get(), max_udp_packet_length));
        auto message = std::string(buffer.get(), length);
        UdpPacket packet;
        packet.unmarshall(message);
        std::lock_guard<std::mutex> lock(receive_queue_mutex);
        receive_queue.push(packet);
        receive_cv.notify_all();
    }
}

void ReliableUdp::write(const UdpPacket& packet)
{
    auto message = packet.marshall();
    socket.send(asio::buffer(message, message.length()));
}

void ReliableUdp::completeThreewayHandshake(UdpPacket& packet)
{
    peer_sequence_number = packet.seq_num;    
    std::ostringstream oss;
    oss << packet.peer_port;
    peer_endpoint = *resolver.resolve({udp::v4(), packet.peerIpV4(), oss.str()});
    socket.connect(peer_endpoint);

    packet.ack_number = ++peer_sequence_number;
    packet.seq_num = initial_sequence_number;
    ++sequence_number;
    packet.setSynAck();
    packet.data = "";
    {
        std::lock_guard<std::mutex> lock(send_queue_mutex);
        send_queue.push(packet);
    }
    send_cv.notify_one();
    while (true)
    {
        std::unique_lock<std::mutex> lock(receive_queue_mutex);
        receive_cv.wait(lock, [this]{return !receive_queue.empty();});
        auto& packet = receive_queue.front();
        if (!packet.synPacket() && packet.ackPacket() && packet.ack_number == sequence_number &&
                packet.seq_num == peer_sequence_number)
        {
            ++peer_sequence_number;
            ++sequence_number;
            packet.resetAck();
            if (!packet.dataPacket())
                receive_queue.pop();
            break;
        }
        else
        {
            receive_queue.pop();
            lock.unlock();
            UdpPacket packet;
            packet.setAck();
            packet.ack_number = peer_sequence_number;
            packet.seq_num = sequence_number;
            packet.data = "";
            {
                std::lock_guard<std::mutex> lock(send_queue_mutex);
                send_queue.push(packet);
            }
        }
    }
}

ReliableUdp::ReliableUdp(asio::io_service& io_service) :
    io_service{io_service}, socket(udp::socket(io_service, udp::endpoint(udp::v4(), 0))),
    resolver(io_service), accept_request{true}
{
    init();
}

ReliableUdp::~ReliableUdp()
{
    {
        std::lock_guard<std::mutex> lock(send_queue_mutex);
        accept_request = false;
    }
    send_cv.notify_all();
    socket.cancel();
    //TODO
}

void ReliableUdp::write(std::string message)
{
    do
    {
        auto len = std::min(message.length(), static_cast<unsigned long>(max_udp_payload_length));
        UdpPacket packet;
        packet.seq_num = sequence_number++;
        packet.setPeerIpV4(peer_endpoint.address().to_string());
        packet.packet_type = PacketTypeMask::Data;
        packet.peer_port = peer_endpoint.port();
        packet.data = message.substr(0, len);
        send_queue.push(packet);
        //TODO
        message = message.substr(len);
    }while (message.length() > 0);
}

std::size_t ReliableUdp::read(asio::streambuf& buffer, int length)
{

}

void ReliableUdp::init()
{
    std::random_device rd;
    std::default_random_engine engine(rd());
    std::uniform_int_distribution<SeqNum> dis;
    initial_sequence_number = sequence_number = dis(engine);
    read_thread = std::thread(&ReliableUdp::srRead, this);
    write_thread = std::thread(&ReliableUdp::srWrite, this);
}
