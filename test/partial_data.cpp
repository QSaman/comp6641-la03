#include <asio.hpp>
#include <iostream>
#include <string>
#include <thread>

void writeAndRead()
{
    using asio::ip::tcp;
    using std::cout;
    using std::endl;

    asio::io_service io_service;
    tcp::socket socket(io_service);
    tcp::resolver resolver(io_service);
    asio::connect(socket, resolver.resolve({"localhost", "7777"}));
    std::string msg1 = "POST /saman.txt HTTP/1.0\r\nContent-Length: 11\r\n\r\nsaman\n";
    cout << "Sending the first part of message" << endl;
    asio::write(socket, asio::buffer(msg1));
    cout << "Waiting for 1 second" << endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::string msg2 = "saadi";
    cout << "Sending the second part of message" << endl;
    asio::write(socket, asio::buffer(msg2));
    asio::streambuf buffer;
    asio::error_code ec;
    auto n = asio::read(socket, buffer, ec);
    if (ec && ec != asio::error::eof)
        throw std::system_error(ec);
    auto iter = asio::buffers_begin(buffer.data());
    auto reply = std::string(iter, iter + n);
    buffer.consume(n);
    cout << reply << endl;
}

int main()
{
    try
    {
        writeAndRead();
    }
    catch(const std::system_error& ex)
    {
        std::cerr << ex.what() << std::endl;
    }
}
