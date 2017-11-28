#include "reliable_udp.h"
#include "udp_packet.h"

#include <random>
#include <fstream>
#include <iostream>
#include <vector>
#include <asio/deadline_timer.hpp>

using asio::ip::udp;

//void testMarshalling()
//{
//    UdpPacket packet;
//    packet.packet_type = PacketTypeMask::Rst;
//    packet.seq_num = 1;
//    packet.setPeerIpV4("192.168.0.1");
//    packet.peer_port = 45678;
//    packet.data = "GET /ip HTTP/1.0\r\nHost: httpbin.org\r\n\r\n";
//    //auto res = packet.marshall();
//    std::ofstream os("out.cereal", std::ios::binary);
//    os << res;
//    UdpPacket packet2;
//    packet2.unmarshall(res);
//    using namespace std;
//    if (packet2.packet_type == PacketTypeMask::Rst)
//        cout << "Yes" << endl;
//    else
//        cout << "No" << endl;
//    std::cout << packet2.data << endl;
//    cout << packet2.seq_num << ' ' << packet2.peerIpV4() << ' ' << packet2.peer_port << endl;
//}

void ReliableUdp::srWrite(bool hand_shake)
{
//    std::unique_lock<std::mutex> lock(send_queue_mutex);
//    send_cv.wait(lock, [this]{return !send_queue.empty() || !accept_request;});
//    if (!accept_request)
//        break;
    unsigned int send_base = 0, next = 0;
    std::unique_ptr<bool> timeout_list(new bool[send_queue.size()]);
    std::vector<bool> ack_list(send_queue.size(), false);
    std::vector<asio::steady_timer> timers;
    timers.reserve(send_queue.size());
    unsigned int io_opt_cnt = 0;
    while (send_base < send_queue.size())
    {
        for (; (next - send_base) < window_size && next < send_queue.size(); ++next)
        {
            auto& timeout = timeout_list.get()[next];
            timeout = false;
            auto& packet = send_queue[next];
            timers.push_back(asio::steady_timer(socket.get_io_service()));
            auto& timer = timers.back();
            timer.expires_from_now(std::chrono::milliseconds(timeout_limit));
            write(packet);
            timer.async_wait([&timeout](const asio::error_code& error)
            { if (error != asio::error::operation_aborted) timeout = true; });
            srRead();
            io_opt_cnt += 2;   // wait for 1 write and 1 (read or timeout)
        }

        for (unsigned i = 0; i < io_opt_cnt; ++i)
        {
            auto res = io_service.run_one();
            std::cout << "io_service.run_one: " << res << std::endl;
        }
        io_opt_cnt = 0;

        while (!receive_ack_queue.empty())  //Handle received ack packets
        {
            auto& packet = receive_ack_queue.front();
            if (packet.ack_number < send_queue[send_base].seq_num ||
                packet.ack_number >= send_queue[next].seq_num)
            {
                receive_ack_queue.pop();    //discard packet
                continue;
            }
            if (hand_shake && packet.seq_num != peer_sequence_number)
            {
                receive_ack_queue.pop();    //discard packet
                continue;
            }
            auto index = packet.ack_number - send_queue[0].seq_num;
            ack_list[index] = true;
            timers[index].cancel();
            receive_ack_queue.pop();
        }

        for (auto i = send_base; i < next; ++i) //Handle timeout packets
        {
            auto& timeout = timeout_list.get()[i];
            if (!timeout)
                continue;
            timeout = false;
            timers[i] = asio::steady_timer(socket.get_io_service());
            auto& timer = timers[i];
            timer.expires_from_now(std::chrono::milliseconds(timeout_limit));
            write(send_queue[i]);
            timer.async_wait([&timeout](const asio::error_code& error)
            { if (error != asio::error::operation_aborted) timeout = true; });
            srRead();
            io_opt_cnt += 2;
        }

        for (auto i = send_base; i < next && ack_list[i]; ++i)
            ++send_base;
    }

}

void ReliableUdp::srRead()
{
    socket.async_receive(asio::buffer(read_buffer, max_udp_packet_length),
                         [this](const asio::error_code& error, std::size_t bytes_transferred)
    {
        if (error)
        {
            std::cerr << "Error in receiving data" << std::endl;
            return;
        }
        if (bytes_transferred == 0)
            return;
        std::string message = std::string(read_buffer, bytes_transferred);
        //std::lock_guard<std::mutex> lock(receive_queue_mutex);
        UdpPacket packet_with_data;
        packet_with_data.unmarshall(message);
        if (packet_with_data.synPacket())
            return; //discard message
        UdpPacket packet_header;
        packet_header.unmarshall(message, true);
        packet_header.resetData();
        if (packet_with_data.dataPacket())
            receive_data_queue.push(packet_with_data);
        if (packet_header.ackPacket())
            receive_ack_queue.push(packet_header);
        //receive_cv.notify_all();
    });
}

void ReliableUdp::write(const UdpPacket& packet)
{
    socket.async_send(asio::buffer(packet.marshalled_message),
                      [](const asio::error_code& error_code, std::size_t bytes_transferred)
    {
        if (error_code)
            std::cerr << "Error in sending " << bytes_transferred << std::endl;
    });

}

void ReliableUdp::sendHandShakeResponse(UdpPacket& packet)
{
}

bool ReliableUdp::completeThreewayHandshake(UdpPacket& packet)
{
    peer_sequence_number = packet.seq_num;    
    std::ostringstream oss;
    oss << packet.peer_port;
    peer_endpoint = *resolver.resolve({udp::v4(), packet.peerIpV4(), oss.str()});
    socket.connect(peer_endpoint);

    packet.ack_number = ++peer_sequence_number;
    packet.seq_num = initial_sequence_number;
    ++sequence_number;
    packet.clearPacketType();
    packet.setSynAck();
    packet.data = "";
    send_queue.push_back(packet);
    srWrite();
//    while (true)
//    {
////        std::unique_lock<std::mutex> lock(receive_queue_mutex);
////        receive_cv.wait(lock, [this](){return !receive_queue.empty();});
//        auto& packet = receive_queue.front();
//        if (!packet.synPacket() && packet.ackPacket() && packet.ack_number == sequence_number &&
//                packet.seq_num == peer_sequence_number)
//        {
//            ++peer_sequence_number;
//            ++sequence_number;
//            packet.resetAck();
//            if (!packet.dataPacket())
//                receive_queue.pop();
//            break;
//        }
//        else
//        {
//            receive_queue.pop();
//            //lock.unlock();
//            packet.ack_number = peer_sequence_number;
//            packet.seq_num = sequence_number;
////            if (!sendHandShakeResponse(packet))
////            {
////                init();
////                //TODO
////                return false;
////            }
//        }
//    }
    return true;
}

ReliableUdp::ReliableUdp(asio::io_service& io_service) :
    socket(udp::socket(io_service, udp::endpoint(udp::v4(), 0))), /*work(io_service),*/
    io_service{io_service}, resolver(io_service), accept_request{true}
{
    init();
}

ReliableUdp::~ReliableUdp()
{
    io_service.stop();
    //background_thread.join();
    //TODO
}

void ReliableUdp::write(const std::string& message)
{
    const unsigned packet_num = static_cast<unsigned>(message.length()) / max_udp_payload_length +
            ((message.length() % max_udp_payload_length) == 0 ? 0 : 1);
//    std::vector<UdpPacket> packet_list;
//    packet_list.reserve(packet_num);
    std::string msg_buf;
    unsigned processed_len = 0;
    for (unsigned i = 0; i < packet_num; ++i)
    {
        auto len = (i < (packet_num - 1) ? max_udp_payload_length : message.length() - processed_len);
        msg_buf = message.substr(processed_len, len);
        processed_len += len;
        send_queue.push_back(UdpPacket());;
        UdpPacket& packet = send_queue.back();
        packet.seq_num = sequence_number++;
        packet.setPeerIpV4(peer_endpoint.address().to_string());
        packet.packet_type = PacketTypeMask::Data;
        packet.peer_port = peer_endpoint.port();
        packet.data = msg_buf;
        packet.marshall();        
    }
    srWrite();
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
//    read_thread = std::thread(&ReliableUdp::srRead, this);
//    write_thread = std::thread(&ReliableUdp::srWrite, this);
    //background_thread = std::thread([this](){io_service.run();});
}
