#include "http_server.h"
#include "../libhttpc/http_client.h"
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

void runUdpServer(unsigned short port)
{
    using asio::ip::tcp;
    asio::io_service io_service;
    std::cout << "Listening on port " << port << std::endl;
    std::cout << "root directory is: " << root_dir_path << std::endl << std::endl;
    std::cout << "Loading MIME types..." << std::endl;
    initExt2Mime();
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

