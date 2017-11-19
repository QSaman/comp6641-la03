#pragma once

#include <iosfwd>

extern bool verbose;

enum class TransportProtocol
{
    TCP,
    UDP
};

extern TransportProtocol transport_protocol;

void runHttpServer(unsigned short port);
std::string constructServerMessage(const std::string partial_header, const std::string body);
