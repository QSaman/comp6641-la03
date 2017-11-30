#include "http_server.h"
#include "../libhttpc/http_client.h"
#include "../reliable_udp/reliable_udp.h"
#include "../reliable_udp/udp_packet.h"
#include "../reliable_udp/udp_accpter.h"
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

void udpReadAndWrite(ReliableUdp& reliable_udp)
{
    auto remote_address = reliable_udp.remoteAddress();
    auto remote_port = reliable_udp.remotePort();
    std::ostringstream oss;
    oss << remote_address << ':' << remote_port;
    auto remote_tcp_info = oss.str();

    if (verbose)
        std::cout << "Accept connection from " << remote_tcp_info << std::endl;

    std::string buffer;
    HttpMessage http_message = HttpClient::readUdpHttpMessage(reliable_udp, buffer);
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
    std::cout << std::endl << "Writing http message to client" << std::endl << std::endl;
    reliable_udp.write(reply_msg);
    std::cout << std::endl << "Successfully writing http message to client" << std::endl << std::endl;
    if (buffer.size())
    {
        std::cerr << remote_tcp_info << " sent " << buffer.size() << " extra bytes!"
                  << std::endl;
    }
}

void handleUdpClientHttpRequest(ReliableUdp& active_socket) noexcept
{
    try
    {
        udpReadAndWrite(active_socket);
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

void runUdpHttpServer(unsigned short port)
{
    using asio::ip::udp;
    asio::io_service io_service;

    UdpPassiveSocket passive_socket(io_service, port);
    try
    {
        while (true)
        {
            ReliableUdp reliable_udp(io_service);
            passive_socket.accept(reliable_udp);
            //std::thread(handleUdpClientHttpRequest, std::move(reliable_udp)).detach();
            handleUdpClientHttpRequest(reliable_udp);
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
        runUdpHttpServer(port);
}

