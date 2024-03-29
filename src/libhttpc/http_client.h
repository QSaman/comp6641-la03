#pragma once

#include <string>
#include <sstream>
#include <map>
#include <asio.hpp>

#include "../reliable_udp/reliable_udp.h"

namespace LUrlParser{ class clParseURL; }
using HttpHeader = std::map<std::string, std::string>;

enum class ClientTransportProtocol
{
    TCP,
    UDP
};

extern ClientTransportProtocol client_transport_protocol;
extern std::string router_address;
extern unsigned short router_port;
extern unsigned int client_window_size;

enum class HttpMessageType
{
    Reply,
    Get,
    Post
};

struct HttpMessage
{
    HttpMessageType message_type;
    std::string header, body, protocol, protocol_version;
    int status_code;
    std::string resource_path;
    std::string status_message;
    HttpHeader http_header;
    bool is_text_body;  //Weather or not the body is text or binary
};

class HttpClient
{
public:
    HttpMessage sendGetCommand(const std::string& url, const std::string& header = "", bool verbose = false);
    HttpMessage sendPostCommand(const std::string& url, const std::string& data = "",
                                const std::string& header = "", bool verbose = false);
    static HttpMessage parseHttpMessage(const std::string& message, bool read_body = true);
    static bool isTextBody(const std::string& mime);
    static HttpMessage readHttpMessage(asio::ip::tcp::socket& socket, asio::streambuf& buffer);
    static HttpMessage readUdpHttpMessage(ReliableUdp& reliable_udp, std::string& buffer);
private:
    HttpMessage requestAndReply(const std::string& host, const std::string& port, const std::string& message);
    HttpMessage tcpRequestAndReply(const std::string& host, const std::string& port, const std::string& message);
    HttpMessage udpRequestAndReply(const std::string& host, const std::string& port, const std::string& message);
    void constructMessage(std::ostringstream& oss, LUrlParser::clParseURL& lurl, const std::string& header, const std::string& data);
};
