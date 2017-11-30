#include "reliable_udp.h"
#include "udp_packet.h"

#include <random>
#include <fstream>
#include <iostream>
#include <vector>
#include <asio/deadline_timer.hpp>

using asio::ip::udp;

#define print(x) #x << ": " << x

unsigned int window_size = 1;

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

void ReliableUdp::srWrite(HandshakeStatus handshake_status)
{
    //next: current unsent packet index
    //send_base: the least-value index of unack packet
    unsigned int send_base = 0, next = 0;
    std::unique_ptr<bool> timeout_list(new bool[send_queue.size()]);
    std::vector<bool> ack_list(send_queue.size(), false);
    std::vector<asio::steady_timer> timers;
    timers.reserve(send_queue.size());
    int io_opt_cnt = 0;
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
            {
                if (error != asio::error::operation_aborted)
                {
                    std::cout << "timer timeout for the first!" << std::endl;
                    timeout = true;
                }
                else
                    std::cout << "timer operation is aborted in the first run!" << std::endl;
            });
            read();
            io_opt_cnt += 3;   // wait for 1 write and 1 read and 1 timer
        }        
        for (; io_opt_cnt > 0 && receive_ack_queue.empty(); --io_opt_cnt)
        {
            for (auto i = send_base; i < next; ++i) //Handle timeout packets
            {
                auto& timeout = timeout_list.get()[i];
                if (!timeout)
                    continue;
                //std::cout << "Revan: timeout!" << std::endl;
                timeout = false;
                std::cout << "sr write: packet " << i << " timer is expired!" << std::endl;
                timers[i] = asio::steady_timer(socket.get_io_service());
                auto& timer = timers[i];
                timer.expires_from_now(std::chrono::milliseconds(timeout_limit));
                write(send_queue[i]);
                timer.async_wait([&timeout](const asio::error_code& error)
                {
                    if (error != asio::error::operation_aborted)
                    {
                        std::cout << "timer timeout again!" << std::endl;
                        timeout = true;
                    }
                    else
                        std::cout << "timer operation is aborted!" << std::endl;
                });
                //read();
                io_opt_cnt += 2;
            }
            std::cout << "Revan: Check" << std::endl;
//            if (io_service.stopped())
//                io_service.restart();
            io_service.run_one();
            std::cout << "Revan: " << print(io_service.stopped()) << std::endl;
        }

        while (!receive_ack_queue.empty())  //Handle received ack packets
        {
            std::cout << "sr write: dequeue a packet from queue" << std::endl;
            auto& packet = receive_ack_queue.front();
            if (packet.ack_number < send_queue[send_base].seq_num ||
                (next < send_queue.size() && packet.ack_number > send_queue[next].seq_num))
            {
                //std::cout << "Revan: First continue" << std::endl;
                std::cout <<"Revan: " << print(packet.ack_number) << std::endl;
                std::cout <<"Revan: " << print(send_queue[send_base].seq_num) << std::endl;
                std::cout <<"Revan: " << print(send_queue[next].seq_num) << std::endl;
                std::cout << "sr write: invalid ack number. I drop the packet" << std::endl;
                receive_ack_queue.pop();    //discard packet
                read();
                ++io_opt_cnt;
                continue;
            }
            if (handshake_status == HandshakeStatus::Server && packet.seq_num != read_seq_num)
            {
                //std::cout << "Revan: Second continue" << std::endl;
                receive_ack_queue.pop();    //discard packet
                std::cout << "Revan: " << print(packet.seq_num) << std::endl;
                std::cout << "Revan: " << print(read_seq_num) << std::endl;
                std::cerr << "Invalid sequene number in handshake (server)! I ignore it." << std::endl;
                read();
                ++io_opt_cnt;
                continue;
            }
            else if (handshake_status == HandshakeStatus::Client && packet.synAckPacket())
            {
                std::cout << "sr write: receive handshake response from server" << std::endl;
                ++io_opt_cnt;
                if (!serverHandshakeResponse(packet))
                {
                    read();
                    continue;
                }
            }
            //std::cout << "Revan: No Continue" << std::endl;
            auto index = packet.ack_number - send_queue[0].seq_num;
            std::cout << "sr write: receive ack for packet " << index << std::endl;
            ack_list[index] = true;
            timers[index].cancel();
            std::cout << "sr write: cancel timer for packet " << index << std::endl;
            timeout_list.get()[index] = false;
            receive_ack_queue.pop();
        }

        for (; send_base <= next && ack_list[send_base]; ++send_base);
        //std::cout << "Revan: " << print(send_base) << std::endl;
        //std::cout << "Revan: " << print(next) << std::endl;
    }
    std::cout << "sr write: running remaining io_service.run_one: " << print(io_opt_cnt) << std::endl;
    for (;io_opt_cnt > 0; --io_opt_cnt)
    {
        io_service.run_one();
    }
    std::cout << "sr write: After running remaining io_service.run_one: " << print(io_service.stopped()) << std::endl;
    send_queue.clear();
    if (io_service.stopped())
        io_service.restart();
    std::cout << "sr write: After restarting io_service: " << print(io_service.stopped()) << std::endl;
}

std::size_t ReliableUdp::read(std::string& buffer, unsigned int length)
{
    std::cout << "ReliableUdp::read" << std::endl;
    buffer.clear();
    const unsigned packet_num = length / max_udp_payload_length +
            (length % max_udp_payload_length ? 1 : 0);

    return srRead(buffer, packet_num);
}

std::size_t ReliableUdp::read_untill(std::string& buffer, std::string pattern)
{
    std::cout << "ReliableUdp::read_untill" << std::endl;
    buffer.clear();
    std::size_t ret;
    while (true)
    {
        std::string message;
        srRead(message, 1);
        buffer += message;
        ret = message.find(pattern);
        if (ret != message.npos)
            break;
    }
    return (ret + pattern.length());
}

std::size_t ReliableUdp::srRead(std::string& buffer, unsigned int packet_num)
{
    unsigned read_base = 0;
    std::deque<UdpPacket> window;
    window.resize(window_size);
    auto read_seq_num_orig = read_seq_num;

    while (read_base < packet_num)
    {
        while (receive_data_queue.empty())
        {
            read();
            io_service.run_one();
        }
        auto packet = receive_data_queue.front();
        receive_data_queue.pop();
        if (packet.seq_num < read_seq_num)
        {
            std::cout << "packet sequence number is old" << std::endl;
            continue;
        }
        unsigned index = packet.seq_num - read_seq_num;
        std::cout << "sr read: packet index: " << index << std::endl;
        if (index >= window_size || (packet.seq_num - read_seq_num_orig) > packet_num)
        {
            std::cout << "sr read: packet is not in the read window frame" << std::endl;
            continue;
        }
        window[index] = std::move(packet);
        window[index].valid = true;
        UdpPacket ack_packet;
        ack_packet.setAck();
        ack_packet.ack_number = packet.seq_num;
        ack_packet.seq_num = write_seq_num;
        ack_packet.setPeerIpV4(peer_endpoint.address().to_string());
        ack_packet.peer_port = peer_endpoint.port();
        std::cout << "sr read sending ack to " << ack_packet.peerIpV4() << ':' << ack_packet.peer_port << std::endl;
        write(ack_packet);
        std::cout << "Revan: " << print(io_service.run_one()) << std::endl;
        unsigned ordered_cnt;
        for (ordered_cnt = 0; ordered_cnt < window.size() && window[ordered_cnt].valid; ++ordered_cnt)
        {
            buffer += window[ordered_cnt].data;
            ++read_base;
            ++read_seq_num;
            std::cout << "sr read: retreiving packet: " << print(read_seq_num) << std::endl;
        }
        for (unsigned i = 0; i < ordered_cnt; ++i)
            window.pop_front();
        if (ordered_cnt > 0)
            window.resize(window_size);
    }
    return buffer.length();
}


bool ReliableUdp::serverHandshakeResponse(UdpPacket& packet)
{
    if (handshake_status != HandshakeStatus::Client || packet.ack_number != init_write_seq_num)
    {
        std::cout << "Server sent invalid handshake" << std::endl;
        std::cout << "Revan: " << print(packet.ack_number) << std::endl;
        std::cout << "Revan: " << print(init_write_seq_num) << std::endl;
        return false;
    }
    std::cout << "Received SYNACK packet. I'm trying to send an ack." << std::endl;
    if (connection_status != ConnectionStatus::Connected)
        read_seq_num = packet.seq_num + 1;
    packet.resetSyn();
    UdpPacket ack_packet;
    std::ostringstream oss;
    oss << packet.peer_port;
    peer_endpoint = *resolver.resolve({udp::v4(), packet.peerIpV4(), oss.str()});
    ack_packet.setAck();
    ack_packet.seq_num = init_write_seq_num + 1;
    ack_packet.ack_number = packet.seq_num;
    ack_packet.setPeerIpV4(peer_endpoint.address().to_string());
    ack_packet.peer_port = peer_endpoint.port();
    write(ack_packet);
    return true;
}

void ReliableUdp::read()
{
    socket.async_receive(asio::buffer(read_buffer, max_udp_packet_length),
                         [this](const asio::error_code& error, std::size_t bytes_transferred)
    {
        if (error)
        {
            std::cerr << "Error in receiving data" << std::endl;
            read();
            return;
        }
        if (bytes_transferred == 0)
        {
            std::cout << "0 bytes received!" << std::endl;
            read();
            return;
        }
        std::string message = std::string(read_buffer, bytes_transferred);
        UdpPacket packet_with_data;
        packet_with_data.unmarshall(message);
        UdpPacket packet_header;
        packet_header.unmarshall(message, true);
        packet_header.resetData();

        int tmp = packet_header.packet_type;
        std::cout << "read packet type " << tmp << " from " << packet_header.peerIpV4() << ':' << packet_header.peer_port << std::endl;
        if (packet_with_data.dataPacket())
        {
            if (packet_with_data.seq_num < read_seq_num)    //Sender didn't receive our ack
            {
                std::cout << "Sender did not receive our ack packet. I'm trying to resend" << std::endl;
                std::thread thread([this](unsigned seq_num, unsigned ack_num)
                {
                    UdpPacket ack_packet;
                    ack_packet.setAck();
                    ack_packet.ack_number = ack_num;
                    ack_packet.seq_num = seq_num;
                    ack_packet.setPeerIpV4(peer_endpoint.address().to_string());
                    ack_packet.peer_port = peer_endpoint.port();
                    write(ack_packet);
                    io_service.run_one();
                }, write_seq_num, packet_with_data.seq_num);
                thread.detach();
                read();
            }
            else
            {
                std::cout << "push a data packet into queue" << std::endl;
                receive_data_queue.push(packet_with_data);
                std::cout << "Revan: " << print(receive_data_queue.size()) << std::endl;
            }
        }
        //std::cout << "Revan: " << print(packet_with_data.synAckPacket()) << std::endl;
        if (packet_header.synAckPacket() && connection_status == ConnectionStatus::Connected)
        {
            serverHandshakeResponse(packet_header);
            read();
            return;
        }        
        if (packet_header.ackPacket())
        {
            std::cout << "Pushing an ack packet into queue" << std::endl;
            receive_ack_queue.push(packet_header);
        }
    });
}

//The following method should be thread-safe
void ReliableUdp::write(UdpPacket& packet)
{
    packet.marshall();
    int tmp = packet.packet_type;
    std::cout << "Writing packet type " << tmp << " to " << packet.peerIpV4() << ":" << packet.peer_port << std::endl;
    socket.async_send(asio::buffer(packet.marshalled_message),
                      [](const asio::error_code& error_code, std::size_t bytes_transferred)
    {
        if (error_code)
            std::cerr << "Error in sending " << bytes_transferred << std::endl;
        else
            std::cout << "write packet handler" << std::endl;
    });

}

void ReliableUdp::connect(const std::string& host, const std::string& port, const udp::endpoint& router_endpoint)
{
    peer_endpoint = *resolver.resolve({udp::v4(), host, port});
    this->router_endpoint = router_endpoint;
    socket.connect(router_endpoint);
    UdpPacket packet;
    packet.setSyn();
    packet.setPeerIpV4(peer_endpoint.address().to_string());
    packet.peer_port = peer_endpoint.port();
    packet.ack_number = 0;
    packet.seq_num = init_write_seq_num;
    packet.data = "";
    send_queue.push_back(packet);
    handshake_status = HandshakeStatus::Client;
    srWrite(HandshakeStatus::Client);
    ++write_seq_num;
    connection_status = ConnectionStatus::Connected;
}

bool ReliableUdp::completeThreewayHandshake(UdpPacket& packet, const udp::endpoint& router_endpoint)
{
    read_seq_num = packet.seq_num;
    std::ostringstream oss;
    oss << packet.peer_port;
    peer_endpoint = *resolver.resolve({udp::v4(), packet.peerIpV4(), oss.str()});
    this->router_endpoint = router_endpoint;
    socket.connect(this->router_endpoint);

    packet.ack_number = read_seq_num;
    packet.seq_num = init_write_seq_num;
    ++read_seq_num;
    packet.clearPacketType();
    packet.setSynAck();
    packet.data = "";
    packet.setPeerIpV4(peer_endpoint.address().to_string());
    packet.peer_port = peer_endpoint.port();
    send_queue.push_back(packet);
    handshake_status = HandshakeStatus::Server;
    srWrite(HandshakeStatus::Server);
    connection_status = ConnectionStatus::Connected;
    ++write_seq_num;
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
    io_service{io_service}, resolver(io_service), accept_request{true},
    handshake_status{HandshakeStatus::Unknown}, connection_status{ConnectionStatus::Disconnect}
{
    init();
}

//ReliableUdp::ReliableUdp(ReliableUdp&& r) : socket{std::move(r.socket)}, io_service{io_service}, resolver(io_service)
//{

//}

ReliableUdp::~ReliableUdp()
{
    io_service.stop();
    //background_thread.join();
    //TODO
}

void ReliableUdp::write(const std::string& message)
{
    std::cout << "ReliableUdp::write" << std::endl;
    const unsigned packet_num = static_cast<unsigned>(message.length()) / max_udp_payload_length +
            ((message.length() % max_udp_payload_length) == 0 ? 0 : 1);

    std::cout << "Revan: " << print(packet_num) << std::endl;
//    std::vector<UdpPacket> packet_list;
//    packet_list.reserve(packet_num);
    std::string msg_buf;
    unsigned processed_len = 0;
    for (unsigned i = 0; i < packet_num; ++i)
    {
        auto len = (i < (packet_num - 1) ? max_udp_payload_length : message.length() - processed_len);
        msg_buf = message.substr(processed_len, len);
        processed_len += len;

        UdpPacket packet;
        packet.seq_num = write_seq_num++;
        packet.setPeerIpV4(peer_endpoint.address().to_string());
        packet.packet_type = PacketTypeMask::Data;
        packet.peer_port = peer_endpoint.port();
        packet.data = msg_buf;
        send_queue.push_back(packet);
    }
    std::cout << "Revan: Before srWrite()" << std::endl;
    srWrite();
    std::cout << "Revan: After srWrite()" << std::endl;
}


void ReliableUdp::init()
{
    std::random_device rd;
    std::default_random_engine engine(rd());
    std::uniform_int_distribution<SeqNum> dis;
    init_write_seq_num = write_seq_num = dis(engine);
//    read_thread = std::thread(&ReliableUdp::srRead, this);
//    write_thread = std::thread(&ReliableUdp::srWrite, this);
    //background_thread = std::thread([this](){io_service.run();});
}
