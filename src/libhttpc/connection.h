#pragma once

#include <string>
#include <asio.hpp>
#include <memory>

class TcpConnection
{
public:
    TcpConnection(const std::string& server_address, const int port);
    virtual void connect() = 0;
    virtual void write(const std::string& message) = 0;
    virtual std::string read() = 0;
    virtual ~TcpConnection();
protected:
    int _port;
    std::string _server_address;
};

class AsioTcpConnection : public TcpConnection
{
public:
    AsioTcpConnection(asio::io_service& io_service, const std::string& server_address, const int port);
    virtual void connect() override;
    virtual void write(const std::string& message) override;
    virtual std::string read() override;
    virtual ~AsioTcpConnection();
private:
    asio::ip::tcp::socket _socket;
    asio::io_service& _io_service;
};

class TcpConnectionFactory
{
public:
    static std::unique_ptr<TcpConnection> createInstance(const std::string& server_address, const int port);
private:
    static asio::io_service _io_service;
};


