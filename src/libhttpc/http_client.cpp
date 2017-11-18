#include "http_client.h"
#include "connection.h"

#include <LUrlParser.h>
#include <iostream>
#include <sstream>
#include <memory>

HttpMessage HttpClient::sendGetCommand(const std::string& url, const std::string& header, bool verbose)
{
    using LUrlParser::clParseURL;
    clParseURL lurl = clParseURL::ParseURL(url);
    if (!lurl.IsValid())
    {
        std::cerr << "Cannot parse url " << url << std::endl;
        exit(1);
    }
    std::ostringstream oss;
    oss << "GET /";
    constructMessage(oss, lurl, header, "");
    auto message = oss.str();
    if (verbose)
        std::cout << "GET message:" << std::endl << message << "------------" << std::endl;
    std::string port = "80";
    if (!lurl.m_Port.empty())
        port = lurl.m_Port;
    auto reply = requestAndReply(lurl.m_Host, port, message);
    if (verbose)
        std::cout << "Reply Header:" << std::endl << reply.header << "------------" << std::endl;
    return reply;
}

HttpMessage HttpClient::sendPostCommand(const std::string& url, const std::string& data, const std::string& header, bool verbose)
{
    using LUrlParser::clParseURL;
    clParseURL lurl = clParseURL::ParseURL(url);
    if (!lurl.IsValid())
    {
        std::cerr << "Cannot parse url " << url << std::endl;
        exit(1);
    }
    std::ostringstream oss;
    oss << "POST /";
    constructMessage(oss, lurl, header, data);
    auto message = oss.str();
    if (verbose)
        std::cout << "POST message:" << std::endl << message << "------------" << std::endl;
    std::string port = "80";
    if (!lurl.m_Port.empty())
        port = lurl.m_Port;
    auto reply = requestAndReply(lurl.m_Host, port, message);
    if (verbose)
        std::cout << "Reply Header:" << std::endl << reply.header << "------------" << std::endl;
    return reply;
}

//TODO this list is incomplete
bool HttpClient::isTextBody(const std::string& mime)
{    
   if (mime.substr(0, 16) == "application/json" || mime.substr(0, 4) == "text")
       return true;
   if (mime.substr(0, 22) == "application/javascript" || mime.substr(0, 15) == "application/xml")
       return true;
   return false;
}

HttpMessage HttpClient::readHttpMessage(asio::ip::tcp::socket& socket, asio::streambuf& buffer)
{
    auto n = asio::read_until(socket, buffer, "\r\n\r\n");
    auto iter = asio::buffers_begin(buffer.data());
    auto http_header = std::string(iter, iter + static_cast<long>(n));
    HttpMessage http_message = parseHttpMessage(http_header, false);
    std::string length_str = http_message.http_header["Content-Length"];
    unsigned int length = 0;
    if (!length_str.empty())
    {
        std::istringstream iss(length_str);
        iss >> length;
    }
    //We need to use the same buffer after consuming the header data:
    //It is important to remember that there may be additional data after the delimiter.
    //For more information: http://think-async.com/Asio/asio-1.11.0/doc/asio/overview/core/line_based.html
    buffer.consume(n);
    auto partial_len = std::min(static_cast<unsigned int>(buffer.size()), length);
    std::string body;
    if (partial_len > 0)
    {
        iter = asio::buffers_begin(buffer.data());
        body = std::string(iter, iter + partial_len);
        buffer.consume(partial_len);
    }
    auto num_bytes = length - body.length();
    if (num_bytes > 0)
    {
        n = asio::read(socket, buffer, asio::transfer_exactly(num_bytes));
        iter = asio::buffers_begin(buffer.data());
        body += std::string(iter, iter + static_cast<long>(num_bytes));
        buffer.consume(num_bytes);
    }
    http_message.body = body;
    return http_message;
}

HttpMessage HttpClient::requestAndReply(const std::string& host, const std::string& port, const std::string& message)
{
    asio::io_service io_service;
    using asio::ip::tcp;
    tcp::socket socket(io_service);
    tcp::resolver resolver(io_service);
    asio::connect(socket, resolver.resolve({host, port}));
    asio::write(socket, asio::buffer(message));
    asio::streambuf buffer;
    return readHttpMessage(socket, buffer);
}

HttpMessage HttpClient::parseHttpMessage(const std::string& message, bool read_body)
{
    using namespace std;
    stringstream ss(message);
    string line;
    HttpMessage http_msg;
    bool read_message_type = false;
    //End-of-line in HTTP protocol is "\r\n" no matter what OS we are using
    //There is an empty line between header and body of message
    while (getline(ss, line) && line != "\r")
    {
        if (line.back() == '\r')
            line.pop_back();
        if (!read_message_type)
        {
            read_message_type = true;
            bool request_msg = false;
            unsigned offset = 0;
            if (line.substr(0, 4) == "HTTP")
            {
                http_msg.message_type = HttpMessageType::Reply;
                http_msg.protocol = "HTTP";
                std::stringstream ss(line.substr(5));
                ss >> http_msg.protocol_version >> http_msg.status_code;
                ss.get(); //Remove space between status code and status message (200 OK)
                std::getline(ss, http_msg.status_message);
            }
            else if (line.substr(0, 3) == "GET")
            {
                http_msg.message_type = HttpMessageType::Get;
                offset = 4;
                request_msg = true;
            }
            else if (line.substr(0, 4) == "POST")
            {
                http_msg.message_type = HttpMessageType::Post;
                offset = 5;
                request_msg = true;
            }
            else
            {
                //TODO
            }
            if (request_msg)
            {
                //TODO
                std::stringstream ss(line.substr(offset));
                std::string tmp;
                ss >> http_msg.resource_path >> tmp; //tmp = HTTP/1.0
                auto index = tmp.find('/');
                if (index == tmp.npos)
                {
                    //TODO
                    continue;
                }
                http_msg.protocol = tmp.substr(0, index);
                http_msg.protocol_version = tmp.substr(index + 1);
            }
        }
        else
        {
            auto index = line.find(':');
            if (index != line.npos)
            {
                std::string key, value;
                key = line.substr(0, index);
                if ((index + 1) < line.length() && line[index + 1] == ' ')
                    ++index;
                value = line.substr(index + 1);
                http_msg.http_header[key] = value;
            }
        }
        line.push_back('\n');
        http_msg.header += line;
    }
    //Reading body of message
    http_msg.is_text_body = isTextBody(http_msg.http_header["Content-Type"]);
    if (!read_body)
        return http_msg;

    if (http_msg.is_text_body)
    {
        while (std::getline(ss, line))
        {
//If we notify server (by using User-Agent) that we are using Linux, it's possible the end-of-line of body will be in Linux Format (\n)
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            line.push_back('\n');
            http_msg.body += line;
        }
    }
    else
    {
        //Since body is binary, we don't modify end-of-line characters
        char ch;
        while (ss.get(ch))
            http_msg.body += ch;
    }

    return http_msg;
}


void HttpClient::constructMessage(std::ostringstream& oss, LUrlParser::clParseURL& lurl, const std::string& header, const std::string& data)
{
    oss << lurl.m_Path;
    if (!lurl.m_Query.empty())
        oss << '?' << lurl.m_Query;
    if (!lurl.m_Fragment.empty())
        oss << '#' << lurl.m_Fragment;
    oss << " HTTP/1.0\r\nHost: " << lurl.m_Host;
    if (!header.empty())
        oss << "\r\n" << header;
    if (!data.empty())
        oss << "\r\nContent-Length: " << data.length() << "\r\n\r\n" << data;
    oss << "\r\n\r\n";
}
