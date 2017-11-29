#pragma once

#include <iosfwd>
#include <asio.hpp>
#include <queue>
#include <deque>
#include <memory>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "udp_packet.h"

enum { max_udp_packet_length = 1024, max_udp_payload_length = 1009, timeout_limit = 500 };

extern unsigned int window_size;

void testMarshalling();

class ReliableUdp
{
    ReliableUdp(asio::io_service& io_service);
    ~ReliableUdp();
    void write(const std::string& message);
    std::size_t read(char* buffer, unsigned int length);
private:
    void init();
    void srWrite(bool hand_shake = false);
    std::size_t srRead(char* buffer, unsigned int packet_num);
    void read();
    //The following method should be thread-safe
    void write(UdpPacket& packet);
    bool completeThreewayHandshake(UdpPacket& packet);
    friend class UdpPassiveSocket;
private:
    asio::ip::udp::endpoint peer_endpoint;
    SeqNum write_seq_num, init_write_seq_num;
    SeqNum read_seq_num;
    std::queue<UdpPacket> receive_ack_queue, receive_data_queue;
    std::deque<UdpPacket> send_queue;
    std::mutex send_queue_mutex, receive_queue_mutex;
    std::string received_message;
    std::condition_variable send_cv, receive_cv;
    asio::ip::udp::socket socket;
    //See examples/cpp11/futures/daytime_client.cpp:
    //asio::io_service::work work;
    asio::io_service& io_service;  
    asio::ip::udp::resolver resolver;
    bool accept_request;
    //std::thread write_thread, read_thread;
    //std::thread background_thread;
    char read_buffer[max_udp_packet_length];
};
