#pragma once

#include <iosfwd>
#include <asio.hpp>
#include <queue>
#include <memory>
#include <deque>

#include "udp_packet.h"

enum { max_udp_packet_length = 1024, max_udp_payload_length = 1009, timeout_limit = 500 };

void testMarshalling();

class ReliableUdp
{
public:
    explicit ReliableUdp(asio::io_service& io_service, unsigned int window_size = 1, bool verbose = false);
    ~ReliableUdp();
    void write(const std::string& message);
    std::size_t read(std::string& buffer, unsigned int length);
    std::size_t read_untill(std::string& buffer, std::string pattern);
    void connect(const std::string& host, const std::string& port, const asio::ip::udp::endpoint& router_endpoint);
    std::string remoteAddress()
    {
        return peer_endpoint.address().to_string();
    }
    PortNo remotePort()
    {
        return peer_endpoint.port();
    }
private:
    enum class ConnectionStatus
    {
        Disconnect,
        Connected
    };

    enum class HandshakeStatus
    {
        Unknown,
        Server,
        Client
    };

    enum class IOMode
    {
        None,
        Read,
        Write
    };

    struct IOModeWrapper
    {
    public:
        IOModeWrapper(ReliableUdp* reliable_udp, IOMode io_mode)
        {
            this->reliable_udp = reliable_udp;
            this->reliable_udp->io_mode = io_mode;
        }
        ~IOModeWrapper()
        {
            this->reliable_udp->io_mode = IOMode::None;
        }
    private:
        ReliableUdp* reliable_udp;
    };
private:
    bool serverHandshakeResponse(UdpPacket& packet);
    void init();
    void srWrite(HandshakeStatus handshake_status = HandshakeStatus::Unknown);
    void sendAckPacket(SeqNum seq_num);
    std::size_t srRead(std::string& buffer, const unsigned int packet_num);
    void read();
    void write(UdpPacket& packet);
    bool completeThreewayHandshake(UdpPacket& packet, const asio::ip::udp::endpoint& router_endpoint);
    friend class UdpPassiveSocket;
private:
    HandshakeStatus handshake_status;
    ConnectionStatus connection_status;
    IOMode io_mode;
    asio::ip::udp::endpoint peer_endpoint, router_endpoint;
    SeqNum write_seq_num, init_write_seq_num;
    SeqNum read_seq_num;
    std::queue<UdpPacket> receive_ack_queue, receive_data_queue;
    std::deque<UdpPacket> send_queue;
    asio::ip::udp::socket socket;
    asio::io_service& io_service;
    //See examples/cpp11/futures/daytime_client.cpp:
    //If there is one ready task and we call io_service::run_one, after that io_service::stopped is true.
    //So we need to use io_service::work
    asio::io_service::work work;
    asio::ip::udp::resolver resolver;
    char read_buffer[max_udp_packet_length];
    unsigned int window_size;
    bool verbose;
};
