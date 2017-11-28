#include "http_server.h"
#include "../libhttpc/http_client.h"
#include "../reliable_udp/reliable_udp.h"
#include "../reliable_udp/udp_packet.h"
#include "filesystem.h"
#include "request_handler.h"

#include <boost/filesystem.hpp>
#include <asio.hpp>
#include <iostream>
#include <thread>
#include <fstream>
#include <algorithm>

void readAndWrite(asio::ip::tcp::socket& active_socket)
{
    auto remote_address = active_socket.remote_endpoint().address();
    auto remote_port = active_socket.remote_endpoint().port();
    std::ostringstream oss;
    oss << remote_address << ':' << remote_port;
    auto remote_tcp_info = oss.str();

    if (verbose)
        std::cout << "Accept connection from " << remote_tcp_info << std::endl;

    asio::streambuf buffer;
    HttpMessage http_message = HttpClient::readHttpMessage(active_socket, buffer);
    if (verbose)
        std::cout << "Received header (" << remote_tcp_info << "): " << std::endl
                  << http_message.header << std::endl;
    http_message.resource_path = url_decode(http_message.resource_path);

    if (verbose && http_message.is_text_body)
        std::cout << "Received body(" << remote_tcp_info
                  << "): "<< std::endl << http_message.body << std::endl;
    auto reply_msg = prepareReplyMessage(http_message);
    if (verbose)
    {
        HttpMessage tmp = HttpClient::parseHttpMessage(reply_msg, false);
        std::cout << "Response message header (" << remote_tcp_info << "): " << tmp.header << std::endl;
        if (tmp.is_text_body)
        {
            tmp = HttpClient::parseHttpMessage(reply_msg, true);
            std::cout << "Response message body (" << remote_tcp_info
                      << "): " << std::endl << tmp.body << std::endl;
        }
    }
    asio::write(active_socket, asio::buffer(reply_msg));
    if (buffer.size())
    {
        std::cerr << remote_tcp_info << " sent " << buffer.size() << " extra bytes!"
                  << std::endl;
    }
}

void handleClientHttpRequest(asio::ip::tcp::socket active_socket) noexcept
{
    try
    {
        readAndWrite(active_socket);
    }
    catch(const std::system_error& ex)
    {
        std::cerr << ex.what() << std::endl;
    }
    catch(...)
    {
        std::cerr << "Unknown exception for active socket " << std::endl;
    }
}

void runHttpTcpServer(unsigned short port)
{
    using asio::ip::tcp;
    asio::io_service io_service;
    try
    {
        tcp::acceptor passive_socket(io_service, tcp::endpoint(tcp::v4(), port));
        while (true)
        {
            tcp::socket active_socket(io_service);
            passive_socket.accept(active_socket);
            std::thread(handleClientHttpRequest, std::move(active_socket)).detach();
        }
    }
    catch(const std::system_error& ex)
    {
        std::cerr << ex.what() << std::endl;
    }
    catch(...)
    {
        std::cerr << "Unknown exception for passive socket" << std::endl;
    }
}

[[noreturn]] void runHttpUdpServer(unsigned short port)
{
    //testMarshalling();
    using asio::ip::udp;
    asio::io_service io_service;
    udp::socket socket(io_service, udp::endpoint(udp::v4(), port));
    while (true)
    {
        char data[max_udp_packet_length];
        udp::endpoint sender_endpoint;
        auto length = socket.receive_from(asio::buffer(data, max_udp_packet_length), sender_endpoint);
        std::string message(data, length);
        UdpPacket packet;
        packet.unmarshall(message);
        using namespace std;
        cout << "Message Header: " << endl;
        cout << "peer ip: " << packet.peerIpV4() << endl;
        cout << "peer port: " << packet.peer_port << endl;
        cout << "sequence number: " << packet.seq_num << endl;
        cout << "payload:" << endl;
        cout << packet.data << endl;
        packet.data = "Hello There!";
//        std::string reply = packet.marshall();
//        socket.send_to(asio::buffer(reply, reply.length()), sender_endpoint);
    }
}

void runHttpServer(unsigned short port)
{
    std::string protocol;
    if (transport_protocol == TransportProtocol::TCP)
        protocol = "TCP";
    else
        protocol = "UDP";
    std::cout << "Listening on port " << port << " (" << protocol << ")" << std::endl;
    std::cout << "root directory is: " << root_dir_path << std::endl << std::endl;
    std::cout << "Loading MIME types..." << std::endl;
    initExt2Mime();
    if (transport_protocol == TransportProtocol::TCP)
        runHttpTcpServer(port);
    else
        runHttpUdpServer(port);
}

