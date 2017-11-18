#include "request_handler.h"
#include "../libhttpc/http_client.h"
#include "filesystem.h"

#include <string>
#include <boost/filesystem.hpp>
#include <asio.hpp>
#include <iostream>
#include <thread>
#include <fstream>
#include <algorithm>

extern std::string root_dir_path;

std::string constructServerMessage(const std::string partial_header, const std::string body)
{
    std::ostringstream oss;
    auto tmp_len = partial_header.length() - 2;
    std::string header;
    if (tmp_len > 0 && partial_header.substr(tmp_len) == "\r\n")
        header = partial_header.substr(0, tmp_len);
    else
        header = partial_header;
    oss << header;
    oss << "\r\nConnection: close" << "\r\nServer: httpfs/0.0.1";
    oss << "\r\nDate: " << now();
    if (no_cache)
        oss << "\r\nPragma: no-cache\r\nExpires: 0";
    if (!body.empty())
    {
        oss << "\r\nContent-Length: " << body.length();
        oss << "\r\n\r\n" << body;
    }
    return oss.str();
}

std::string generateHtmlMessage(const char* msg, const std::string& title, const std::string& header)
{
    std::ostringstream oss;
    using std::endl;

    oss << "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\">" << endl;
    oss << "<title>" << title << "</title>" << endl;
    oss << "<h1>" << header << "</h1>" << endl;
    oss << "<p>" << msg << "</p>" << endl;
    return oss.str();
}

bool matchMimeFilterList(const std::string& mime_type)
{
    for (const auto& val : mime_filter_list)
        if (mime_type.find(val) != mime_type.npos)
            return true;
    return false;
}

std::string httpGetMessage(const HttpMessage& client_msg) noexcept
{
    using namespace boost::filesystem;
    path dir_path;
    if (client_msg.resource_path.substr(0, 6) == "/icons")
        dir_path = icons_dir_path;
    else
        dir_path = root_dir_path;
    path file_path(client_msg.resource_path);
    path full_path = dir_path / file_path;

    if (client_msg.resource_path == "/" || is_directory(full_path))
    {
        try
        {
            std::string body;
            std::string partial_header;
            auto iter = client_msg.http_header.find("Accept");
            std::string accept;
            if (iter != client_msg.http_header.end())
                accept = iter->second;
            bool send_html = false;
            if (accept.find("text/html") != accept.npos)
                send_html = true;
            else if (accept.find("application/json") != accept.npos)
            {
                body = jsonDirList(full_path.c_str(), children_level);
                partial_header = "HTTP/1.1 200 OK\r\nContent-Type: application/json";
            }
            else if (accept.find("application/xml") != accept.npos)
            {
                body = xmlDirList(full_path.c_str(), children_level);
                partial_header = "HTTP/1.1 200 OK\r\nContent-Type: application/xml";
            }
            else if (accept.find("text/plain") != accept.npos)
            {
                body = textDirList(full_path.c_str());
                partial_header = "HTTP/1.1 200 OK\r\nContent-Type: text/plain";
            }
            else
                send_html = true;
            if (send_html)
            {
                body = htmlDirList(root_dir_path, client_msg.resource_path);
                partial_header = "HTTP/1.1 200 OK\r\nContent-Type: text/html";
            }
            return constructServerMessage(partial_header, body);
        }
        catch(const boost::filesystem::filesystem_error& ex)
        {
            std::string body(generateHtmlMessage(ex.what(), "500 Internal Server Error",
                                                      "Default directory does not exists on the server"));
            std::string partial_header = "HTTP/1.0 500 Internal Server Error\r\n"
                                         "Content-Type: text/html\r\n";
            return constructServerMessage(partial_header, body);
        }
    }
    try
    {        
        std::string path = full_path.c_str();
        std::string body = fileContent(path, !client_msg.is_text_body);
        auto mime_type = getFileMimeType(path);
        if (mime_type.empty())
            mime_type = "text/plain";
        std::string partial_header = "HTTP/1.1 200 OK\r\nContent-Type: " + mime_type;
        if (matchMimeFilterList(mime_type))
        {
            partial_header += "\r\nContent-Disposition: attachment; filename=\"";
            partial_header += std::string(full_path.filename().c_str()) + "\"";
        }
        return constructServerMessage(partial_header, body);
    }
    catch (const filesystem_error& ex)
    {
        std::string body(generateHtmlMessage(ex.what(), "404 Not Found",
                                                  "File does not exist"));

        std::string partial_header = "HTTP/1.0 404 NOT FOUND\r\n"
                                     "Content-Type: text/html";
        return constructServerMessage(partial_header, body);
    }
    catch (const std::ifstream::failure&)
    {
        std::string what = client_msg.resource_path + " doesn't exist!";
        std::string body(generateHtmlMessage(what.c_str(), "404 Not Found",
                                                  "File does not exist"));

        std::string partial_header = "HTTP/1.0 404 NOT FOUND\r\n"
                                     "Content-Type: text/html";
        return constructServerMessage(partial_header, body);
    }
}

std::string httpPostMessage(const HttpMessage& client_msg) noexcept
{
    using namespace boost::filesystem;
    if (client_msg.resource_path.substr(0, 6) == "/icons")
    {
        std::string what = "You are not allowed to change in /icons directory!";
        std::string body = generateHtmlMessage(what.c_str(), "403 Forbiddenr", "Unauthorized operation");
        std::string partial_header = "HTTP/1.0 403 Forbidden\r\n"
                                     "Content-Type: text/html";
        return constructServerMessage(partial_header, body);
    }
    path dir_path(root_dir_path);
    path file_path(client_msg.resource_path);
    try
    {
        path full_path = dir_path / file_path;
        bool is_exists = exists(full_path);
        if (is_exists && !is_regular_file(full_path))
        {
            std::string partial_header = "HTTP/1.0 400 Bad Request\r\n"
                                         "Content-Type: text/html";
            std::string msg = std::string(full_path.c_str()) + " is not a regular file";
            std::string body = generateHtmlMessage(msg.c_str(), "400 Bad Request", "No regular file");
            return constructServerMessage(partial_header, body);
        }
        write2File(full_path.c_str(), client_msg.body, !client_msg.is_text_body);
//        create_directories(full_path.parent_path());
//        std::ofstream out;
//        out.exceptions(std::ofstream::badbit | std::ofstream::failbit);
//        if (client_msg.is_text_body)
//            out.open(full_path.c_str());
//        else
//            out.open(full_path.c_str(), std::ofstream::binary);
//        out << client_msg.body;
        std::string partial_header = "HTTP/1.1 200 OK\r\nContent-Type: text/html";
        std::string body = generateHtmlMessage("File created/updated successfully!",
                                   "Operation Successfull", "Operation Successfull");
        return constructServerMessage(partial_header, body);
    }
    catch(const filesystem_error& ex)
    {
        std::string body = generateHtmlMessage(ex.what(), "500 Internal Server Error", "File Operation Failed");
        std::string partial_header = "HTTP/1.0 500 Internal Server Error\r\n"
                                     "Content-Type: text/html";
        return constructServerMessage(partial_header, body);
    }
    catch(const std::ofstream::failure& ex)
    {
        std::string body = generateHtmlMessage(ex.what(), "500 Internal Server Error", "Error in File IO Operations");
        std::string partial_header = "HTTP/1.0 500 Internal Server Error\r\n"
                                     "Content-Type: text/html";
        return constructServerMessage(partial_header, body);
    }
}

std::string prepareReplyMessage(const HttpMessage& client_msg) noexcept
{
    switch (client_msg.message_type)
    {
    case HttpMessageType::Get:
        return httpGetMessage(client_msg);
    case HttpMessageType::Post:
        return httpPostMessage(client_msg);
    default:
        std::string body = generateHtmlMessage("Operation is not implemented",
                                               "501 Not Implemented", "501 Not Implemented");
        std::string partial_header = "HTTP/1.0 501 Not Implemented\r\n"
                                     "Content-Type: text/html";
        return constructServerMessage(partial_header, body);
    }
}

std::string now()
{
    const char* fmt = "%a, %d %b %Y %H:%M:%S %Z";
    std::time_t t = std::time(NULL);
    char mbstr[100];
    if (std::strftime(mbstr, sizeof(mbstr), fmt, std::gmtime(&t)))
        return std::string(mbstr);

    auto start = std::chrono::system_clock::now();
    std::time_t time = std::chrono::system_clock::to_time_t(start);
    std::string ret(std::ctime(&time));
    return ret;
}
