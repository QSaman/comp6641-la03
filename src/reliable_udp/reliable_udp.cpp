#include "reliable_udp.h"
#include "udp_packet.h"

#include <random>
#include <fstream>
#include <iostream>
#include <vector>
#include <asio/deadline_timer.hpp>

using asio::ip::udp;

#define print(x) #x << ": " << x

void ReliableUdp::srWrite(HandshakeStatus handshake_status)
{
    if (send_queue.empty())
        return;
    std::cout << std::endl << "sr write started - window size: " << window_size << std::endl << std::endl;
    IOModeWrapper wrapper(this, IOMode::Write);
    //next: current unsent packet index
    //send_base: the least-value index of unack packet
    unsigned int send_base = 0, next = 0;
    std::unique_ptr<bool> timeout_list(new bool[send_queue.size()]);
    std::vector<bool> ack_list(send_queue.size(), false);
    std::vector<asio::steady_timer> timers;
    timers.reserve(send_queue.size());
    int io_opt_cnt = 0;
    int percent = -1;
    while (send_base < send_queue.size())
    {
        double ratio = (send_base * 100.0) / send_queue.size();
        int integer = static_cast<int>(std::floor(ratio));
        if (integer > percent)
        {
            percent = integer;
            std::cout << "sr write: " << percent << "% completed." << std::endl;
        }
        for (; (next - send_base) < window_size && next < send_queue.size(); ++next)
        {
            auto& timeout = timeout_list.get()[next];
            timeout = false;
            auto& packet = send_queue[next];
            timers.push_back(asio::steady_timer(socket.get_io_service()));
            auto& timer = timers.back();
            timer.expires_from_now(std::chrono::milliseconds(timeout_limit));
            write(packet);
            timer.async_wait([&timeout,this](const asio::error_code& error)
            {
                if (error != asio::error::operation_aborted)
                {
                    if (verbose)
                        std::cout << "timer timeout for the first time!" << std::endl;
                    timeout = true;
                }
                else if (verbose)
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
                timeout = false;
                if (verbose)
                    std::cout << "sr write packet " << i << " timer is expired!" << std::endl;
                timers[i] = asio::steady_timer(socket.get_io_service());
                auto& timer = timers[i];
                timer.expires_from_now(std::chrono::milliseconds(timeout_limit));
                write(send_queue[i]);
                timer.async_wait([&timeout, this](const asio::error_code& error)
                {
                    if (error != asio::error::operation_aborted)
                    {
                        if (verbose)
                            std::cout << "timer timeout again!" << std::endl;
                        timeout = true;
                    }
                    else if (verbose)
                        std::cout << "Finally timer operation is aborted! I received the packet." << std::endl;
                });
                io_opt_cnt += 2;
            }
            io_service.run_one();
        }

        while (!receive_ack_queue.empty())  //Handle received ack packets
        {
            if (verbose)
                std::cout << "sr write: dequeue a packet from ack queue" << std::endl;
            auto& packet = receive_ack_queue.front();
            if (packet.ack_number < send_queue[send_base].seq_num ||
                (next < send_queue.size() && packet.ack_number > send_queue[next].seq_num))
            {
                if (verbose)
                    std::cout << "sr write: invalid ack number. I dropped the packet" << std::endl;
                receive_ack_queue.pop();    //discard packet
                read();
                ++io_opt_cnt;
                continue;
            }
            if (handshake_status == HandshakeStatus::Server && packet.seq_num != read_seq_num)
            {
                receive_ack_queue.pop();    //discard packet
                std::cerr << "sr write: Server received invalid seq num from client in handshake! I drop the packet." << std::endl;
                read();
                ++io_opt_cnt;
                continue;
            }
            else if (handshake_status == HandshakeStatus::Client && packet.synAckPacket())
            {                
                ++io_opt_cnt;
                if (!serverHandshakeResponse(packet))
                {
                    if (verbose)
                        std::cout << "sr write: Invalid handshake response from server. I drop it." << std::endl;
                    read();
                    continue;
                }
                if (verbose)
                    std::cout << "sr write: receive handshake response from server" << std::endl;
            }
            auto index = packet.ack_number - send_queue[0].seq_num;            
            if (!ack_list[index])
            {
                if (verbose)
                    std::cout << "sr write: receive ack for packet " << index << std::endl;
                ack_list[index] = true;
                timers[index].cancel();
                if (verbose)
                    std::cout << "sr write: cancel timer for packet " << index << std::endl;
                timeout_list.get()[index] = false;
            }
            else
            {
                if (verbose)
                    std::cout << "sr write: receive duplicate ack for packet " << index << ". I drop it." << std::endl;
                read();
                ++io_opt_cnt;
            }
            receive_ack_queue.pop();            
        }
        for (; send_base <= next && ack_list[send_base]; ++send_base);
    }
    if (verbose)
        std::cout << "sr write: running remaining io_service.run_one: " << print(io_opt_cnt) << std::endl;
    for (;io_opt_cnt > 0; --io_opt_cnt)
        io_service.run_one();
    send_queue.clear();
    std::cout << std::endl << "sr write finished" << std::endl << std::endl;
}

std::size_t ReliableUdp::read(std::string& buffer, unsigned int length)
{
    buffer.clear();
    const unsigned packet_num = length / max_udp_payload_length +
            (length % max_udp_payload_length ? 1 : 0);

    return srRead(buffer, packet_num);
}

std::size_t ReliableUdp::read_untill(std::string& buffer, std::string pattern)
{
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

void ReliableUdp::sendAckPacket(SeqNum seq_num)
{
    UdpPacket ack_packet;
    ack_packet.setAck();
    ack_packet.ack_number = seq_num;
    ack_packet.seq_num = write_seq_num;
    ack_packet.setPeerIpV4(peer_endpoint.address().to_string());
    ack_packet.peer_port = peer_endpoint.port();
    if (verbose)
        std::cout << "sr read: sending ack to " << ack_packet.peerIpV4() << ':' << ack_packet.peer_port << std::endl;
    write(ack_packet);
    io_service.run_one();
}

std::size_t ReliableUdp::srRead(std::string& buffer, const unsigned int packet_num)
{
    if (packet_num == 0)
        return 0;
    std::cout << std::endl << "sr read started - window size: " << window_size << std::endl << std::endl;
    IOModeWrapper wrapper(this, IOMode::Read);
    unsigned read_base = 0;
    std::deque<UdpPacket> window;
    window.resize(window_size);
    auto read_seq_num_orig = read_seq_num;
    unsigned int window_packets = 0;
    int percent = -1;
    while (read_base < packet_num)
    {
        double ratio = (read_base * 100.0) / packet_num;
        int integer = static_cast<int>(std::floor(ratio));
        if (integer > percent)
        {
            percent = integer;
            std::cout << "sr read: " << percent << "% completed." << std::endl;
        }
        while (receive_data_queue.empty())
        {            
            read();
            io_service.run_one();
        }
        auto packet = receive_data_queue.front();
        receive_data_queue.pop();
        if (readSeqIndex(packet.seq_num) < readSeqIndex(read_seq_num))
        {
            if (verbose)
                std::cout << "sr read: I received packet before" << std::endl;
            sendAckPacket(packet.seq_num);
            continue;
        }
        unsigned index = readSeqIndex(packet.seq_num) - readSeqIndex(read_seq_num);
        if (verbose)
            std::cout << "sr read: packet index: " << index << std::endl;
        if (index >= window_size || (readSeqIndex(packet.seq_num) - readSeqIndex(read_seq_num_orig)) >= packet_num)
        {
            if (verbose)
                std::cout << "sr read: packet is not in the read window frame or is not mine" << std::endl;
            continue;
        }
        if (!window[index].valid)
        {
            window[index] = std::move(packet);
            window[index].valid = true;
            ++window_packets;
            sendAckPacket(packet.seq_num);
        }
        else if (verbose)
        {
            std::cout << "sr read: received duplicate data packet for position " << index << ". I drop it" << std::endl;
        }
        unsigned ordered_cnt;
        for (ordered_cnt = 0; ordered_cnt < window.size() && window[ordered_cnt].valid; ++ordered_cnt)
        {
            buffer += window[ordered_cnt].data;
            ++read_base;
            ++read_seq_num;
            --window_packets;
            if (verbose)
                std::cout << "sr read: retreiving packet: " << print(read_seq_num) << std::endl;
        }
        for (unsigned i = 0; i < ordered_cnt; ++i)
        {
            window.push_back(UdpPacket());
            window.pop_front();            
        }
//        if (ordered_cnt > 0)
//            window.resize(window_size);
    }
    std::cout << std::endl << "sr read finished" << std::endl << std::endl;
    return buffer.length();
}


bool ReliableUdp::serverHandshakeResponse(UdpPacket& packet)
{
    if (handshake_status != HandshakeStatus::Client || packet.ack_number != init_write_seq_num)
    {
        std::cout << "Server sent invalid handshake" << std::endl;
        return false;
    }
    std::cout << "Received SYNACK packet. I'm trying to send an ack." << std::endl;
    if (connection_status != ConnectionStatus::Connected)
        init_read_seq_num = read_seq_num = packet.seq_num + 1;
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
            std::cerr << "read handler: error in receiving data" << std::endl;
            read();
            io_service.run_one();
            return;
        }
        if (bytes_transferred == 0)
        {
            std::cout << "read handler: 0 bytes received!" << std::endl;
            read();
            io_service.run_one();
            return;
        }
        bool data_packet = false, ack_packet = false;
        std::string message = std::string(read_buffer, bytes_transferred);       
        UdpPacket packet_with_data;
        packet_with_data.unmarshall(message);
        UdpPacket packet_without_data;
        packet_without_data.unmarshall(message, true);
        packet_without_data.resetData();

        if (verbose)
            std::cout << "read handler: received " << message.length() << " bytes" " from " << packet_with_data.peerIpV4() << ':' << packet_with_data.peer_port << std::endl;
        int tmp = packet_with_data.packet_type;
        if (verbose)
            std::cout << "read handler: recieved packet type " << tmp << std::endl;

        if (packet_with_data.dataPacket())
        {
            if (readSeqIndex(packet_with_data.seq_num) < readSeqIndex(read_seq_num))    //Sender didn't receive our ack
            {
                if (verbose)
                {
                    std::cout << "read handler: sender didn't receive our ack packet. I'm sending another ack." << std::endl;
                    std::cout << "read handler: sender seq num: " << packet_with_data.seq_num << std::endl;
                    std::cout << "I expect seq num should be >= " << read_seq_num << std::endl;
                }
                sendAckPacket(packet_with_data.seq_num);
                read();
                io_service.run_one();
                return;
            }
            else
            {
                if (verbose)
                    std::cout << "read handler: push received packet into data queue" << std::endl;
                receive_data_queue.push(packet_with_data);
                data_packet = true;
            }
        }
        if (packet_without_data.synAckPacket() && connection_status == ConnectionStatus::Connected)
        {
            if (verbose && handshake_status == HandshakeStatus::Client)
                std::cout << "Server didn't receive the third part of handshaking. I'm sending it again" << std::endl;
            if (serverHandshakeResponse(packet_without_data))
                io_service.run_one();
            read();
            io_service.run_one();
            return;
        }
        else if (packet_without_data.ackPacket())
        {
            if (verbose)
                std::cout << "read handler: push received packet into ack queue" << std::endl;
            receive_ack_queue.push(packet_without_data);
            ack_packet = true;
        }

        if (io_mode == IOMode::Write && !ack_packet)
        {
            if (verbose)
                std::cout << "read handler: I'm expecting an ack packet but I receive a data packet. Waiting for another packet" << std::endl;
            read();
            io_service.run_one();
        }
        else if (io_mode == IOMode::Read && !data_packet)
        {
            if (verbose)
                std::cout << "read handler: I'm expecting a data packet but I receive an ack packet. Waiting for another packet" << std::endl;
            read();
            io_service.run_one();
        }
    });
}

void ReliableUdp::write(UdpPacket& packet)
{
    if (packet.marshalled_message.empty())
        packet.marshall();
    int tmp = packet.packet_type;
    if (verbose)
        std::cout << "Writing packet type " << tmp << " to " << packet.peerIpV4() << ":" << packet.peer_port << std::endl;
    socket.async_send(asio::buffer(packet.marshalled_message),
                      [this](const asio::error_code& error_code, std::size_t bytes_transferred)
    {
        if (error_code)
            std::cerr << "write handler: Error in sending " << bytes_transferred << std::endl;
        else if (verbose)
            std::cout << "write handler: transmitted " << bytes_transferred << " bytes successfully!" << std::endl;
    });

}

void ReliableUdp::connect(const std::string& host, const std::string& port, const udp::endpoint& router_endpoint)
{
    std::cout << std::endl << "Sending connection request to " << host << ":" << port << std::endl << std::endl;
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
    std::cout << "connect - write sequence number: " << write_seq_num << std::endl;
    std::cout << "connect - read sequence number: " << read_seq_num << std::endl;
    std::cout << std::endl << "Connected to " << peer_endpoint.address().to_string()
              << ":" << peer_endpoint.port() << " successfully!" << std::endl << std::endl;
}

bool ReliableUdp::completeThreewayHandshake(UdpPacket& packet, const udp::endpoint& router_endpoint)
{    
    init_read_seq_num = read_seq_num = packet.seq_num;
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
    std::cout << "Sending SYN-ACK to " << packet.peerIpV4() << ":" << packet.peer_port;
    std::cout << " from " << socket.local_endpoint().address().to_string()
              << ":" << socket.local_endpoint().port() << std::endl;
    srWrite(HandshakeStatus::Server);
    connection_status = ConnectionStatus::Connected;
    ++write_seq_num;
    std::cout << "server-side handshake - write sequence number: " << write_seq_num << std::endl;
    std::cout << "server-side handshake - read sequence number: " << read_seq_num << std::endl << std::endl;
    std::cout << "Connection established successfully!" << std::endl;
    return true;
}

SeqNum ReliableUdp::readSeqIndex(SeqNum seq_num)
{
    long long int index = static_cast<long long int>(seq_num) - static_cast<long long int>(init_read_seq_num);
    while (index < 0)
        index += std::numeric_limits<SeqNum>::max();
    return static_cast<SeqNum>(index);
}

ReliableUdp::ReliableUdp(asio::io_service& io_service, unsigned int window_size, bool verbose) :
    handshake_status{HandshakeStatus::Unknown},
    connection_status{ConnectionStatus::Disconnect}, io_mode{IOMode::None},
    socket(udp::socket(io_service, udp::endpoint(udp::v4(), 0))),io_service{io_service},
    work{io_service}, resolver(io_service), window_size{window_size}, verbose{verbose}
{
    init();
}

ReliableUdp::~ReliableUdp()
{
}

void ReliableUdp::write(const std::string& message)
{
    const unsigned packet_num = static_cast<unsigned>(message.length()) / max_udp_payload_length +
            ((message.length() % max_udp_payload_length) == 0 ? 0 : 1);

    std::cout << "write: dividing the message into " << packet_num << " packets." << std::endl;
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
    srWrite();
}


void ReliableUdp::init()
{
    std::random_device rd;
    std::default_random_engine engine(rd());
    std::uniform_int_distribution<SeqNum> dis;
    init_write_seq_num = write_seq_num = dis(engine);
}
