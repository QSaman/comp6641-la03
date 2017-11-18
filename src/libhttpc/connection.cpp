#include <asio.hpp>
#include <sstream>
#include <string>
#include <iostream>

#include "connection.h"

asio::io_service TcpConnectionFactory::_io_service;

TcpConnection::TcpConnection(const std::string& server_address, const int port) :
    _port{port}, _server_address{server_address}
{
}

TcpConnection::~TcpConnection()
{
}

AsioTcpConnection::AsioTcpConnection(asio::io_service& io_service, const std::string& server_address, const int port) :
    TcpConnection(server_address, port), _socket{asio::ip::tcp::socket(io_service)}, _io_service{io_service}
{
}

void AsioTcpConnection::connect()
{
    std::ostringstream iss;
    asio::ip::tcp::resolver resolver(_io_service);
    iss << _port;
    asio::error_code ec;
    asio::connect(_socket, resolver.resolve({_server_address, iss.str()}), ec);
    if (ec)
    {
        std::cerr << "Connection error (" << ec.value() << "): " << ec.message() << std::endl;
        exit(ec.value());
    }
}

void AsioTcpConnection::write(const std::string& message)
{
    asio::error_code ec;
    asio::write(_socket, asio::buffer(message), ec);
    if (ec)
    {
        std::cerr << "Error in writing data (" << ec.value() << "): " << ec.message() << std::endl;
        exit(ec.value());
    }
}

std::string AsioTcpConnection::read()
{
    asio::streambuf sb;
    asio::error_code ec;
    auto n = asio::read(_socket, sb, ec);
    if (ec && ec != asio::error::eof)
    {
        std::cerr << "Error in reading data (" << ec.value() << "): " << ec.message() << std::endl;
        exit(ec.value());
    }
    auto bufs = sb.data();
    return std::string(asio::buffers_begin(bufs), asio::buffers_begin(bufs) + n);
}

AsioTcpConnection::~AsioTcpConnection()
{
    asio::error_code ec;
    _socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    if (ec)
    {
        std::cerr << "Error in shutting down the socket (" << ec.value() << "): " << ec.message() << std::endl;
        return;
    }
    _socket.close(ec);
    if (ec)
        std::cerr << "Erro in closing the socket (" << ec.value() << "): " << ec.message() << std::endl;
}


std::unique_ptr<TcpConnection> TcpConnectionFactory::createInstance(const std::string& server_address, const int port)
{
    return std::unique_ptr<TcpConnection>(new AsioTcpConnection(_io_service, server_address, port));
}
