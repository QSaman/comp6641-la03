#pragma once

#include <iosfwd>
#include <asio.hpp>
#include <queue>
#include <memory>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "udp_packet.h"

enum { max_udp_packet_length = 1024, max_udp_payload_length = 1009 };

extern int window_size;

void testMarshalling();

class ReliableUdp
{
    ReliableUdp(asio::io_service& io_service);
    ~ReliableUdp();
    void write(std::string message);
    std::size_t read(asio::streambuf& buffer, int length);
private:
    void init();
    void srWrite();
    void srRead();
    void write(const UdpPacket& packet);
    void completeThreewayHandshake(UdpPacket& packet);
    friend class UdpPassiveSocket;
private:
    asio::ip::udp::endpoint peer_endpoint;
    SeqNum sequence_number, initial_sequence_number;
    SeqNum peer_sequence_number;
    std::queue<UdpPacket> send_queue, receive_queue;
    std::mutex send_queue_mutex, receive_queue_mutex;
    std::condition_variable send_cv, receive_cv;
    asio::ip::udp::socket socket;
    asio::io_service& io_service;
    asio::ip::udp::resolver resolver;
    bool accept_request;
    std::thread write_thread, read_thread;
};
