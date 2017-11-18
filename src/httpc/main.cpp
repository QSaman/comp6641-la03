#include <iostream>
#include <vector>
#include <string>
#include <exception>
#include <fstream>
#include <sstream>
#include <cxxopts.hpp>
#include <LUrlParser.h>
#include <memory>
#include <iterator>

#include "../libhttpc/http_client.h"

enum class CommandType {None, Get, Post, Help};

static cxxopts::Options options("httpc", "httpc is a curl-like application but supports HTTP only.");
static bool verbose = false;
static std::string header;
static CommandType command_type = CommandType::None;
static std::string url;
static std::string post_data;
static std::string output_file;
static bool redirect = false;
static int max_redirection = 50;

[[noreturn]] void print_all_helps()
{
    std::cout << options.help({"", "get", "post"});
    exit(0);
}

[[noreturn]] void print_help(std::string& help_str)
{
    std::cout << options.help({help_str});
    exit(0);
}

void handle_post_data()
{
    bool has_data = false;
    if (options.count("data"))
    {
        if (command_type != CommandType::Post)
        {
            std::cerr << "You cannot use --data for a command other than POST" << std::endl;
            exit(1);
        }
        post_data = options["data"].as<std::string>();
        has_data = true;
    }
    if (options.count("file"))
    {
        if (has_data)
        {
            std::cerr << "You cannot use both --data and --file" << std::endl;
            exit(1);
        }
        if (command_type != CommandType::Post)
        {
            std::cerr << "You cannot use --file for a command other than POST" << std::endl;
            exit(1);
        }
        auto file_path = options["file"].as<std::string>();
        std::ifstream fin;
        fin.open(file_path, std::ifstream::binary);
        fin.exceptions(std::ifstream::badbit | std::ifstream::failbit);
        try
        {
            fin.seekg(0, fin.end);
            auto length = fin.tellg();
            fin.seekg(0, fin.beg);
            std::unique_ptr<char> buffer(new char[length]);
            fin.read(buffer.get(), length);
            post_data.reserve(static_cast<unsigned>(length));
            std::copy(buffer.get(), buffer.get() + length, std::back_inserter(post_data));
        }
        catch(std::ifstream::failure e)
        {
            std::cerr << "Error in reading file " << file_path << std::endl;
            exit(1);
        }
    }
}

void handle_http_header()
{
    using namespace std;

    bool has_data = false;
    if (options.count("header"))
    {
        auto& headers = options["header"].as<std::vector<std::string>>();
        bool first = true;
        for (auto& val : headers)
        {
            if (first)
                first = false;
            else
                header += "\r\n";
            header += val;
        }
        has_data = true;
    }
    if (options.count("header-file"))
    {
        if (has_data)
        {
            std::cerr << "You cannot use both --header and --header-file" << std::endl;
            exit(1);
        }
        auto file_path = options["header-file"].as<std::string>();
        std::ifstream fin;
        std::stringstream ss;
        fin.exceptions(std::ifstream::badbit | std::ifstream::failbit);
        try
        {
            fin.open(file_path);
            ss << fin.rdbuf();
            std::string line;
            bool first = true;
            //fstream handles different end-of-line in different platforms so we don't need to use: line != '\r'
            //After the last line of header there should be an empty line, so it's invalid we have an empty line in header
            while (std::getline(ss, line) && !line.empty())
            {
                if (first)
                    first = false;
                else
                    line = "\r\n" + line;   //HTTP line break
                header += line;
            }
        }
        catch(std::ifstream::failure e)
        {
            std::cerr << "Error in reading file " << file_path << std::endl;
            exit(1);
        }
    }
}

void process_input_args(int argc, char* argv[])
{
    using namespace std;    
    options.positional_help("\n  httpc [Option...] [get|post] [URL]\n  httpc [help] [get|post]");
    options.add_options()
            ("help", "Show all help menus")
            ("v,verbose", "Prints the detail of the response such as protocol, status, and headers")
            ("h,header", "Associate headers to HTTP Request with the format 'key:value'", cxxopts::value<std::vector<std::string>>(), "key:value")
            ("F,header-file", "Associate the content of a file as headers to HTTP Request", cxxopts::value<string>(), "file")
            ("o,output-file", "Writing the body of response to file", cxxopts::value<string>(), "file")
            ("r,redirect", "Redirect to a new address when server sends a status code 3xx")
            ("m,max-redirection", "Maximum number of redirection. If you use -1 it means infinity. The default is 50.", cxxopts::value<int>(), "N");
    //The following options are invisible to the user we only use them for positional arguments
    options.add_options("positional")
            ("c, command", "get is a HTTP GET and post is HTTP POST and help is this text", cxxopts::value<string>(), "get|post|help")
            ("u,url", "A valid URL address", cxxopts::value<string>());
    options.add_options("get")
            ("g,get", "executes a HTTP GET request for a given url", cxxopts::value<string>(), "URL");
    options.add_options("post")
            ("p,post", "executes a HTTP POST request for a given URL with inline data or from file", cxxopts::value<string>(), "URL")
            ("d,data", "Associate an inline data to the body HTTP POST request", cxxopts::value<string>(), "string")
            ("f,file", "Associate the content of a file to the body HTTP POST request", cxxopts::value<string>(), "file");

    options.parse_positional(vector<string>{"command", "url"});
    options.parse(argc, argv);

    if (options.count("help"))
        print_all_helps();
    if (options.count("redirect"))
        redirect = true;
    if (options.count("max-redirection"))
        max_redirection = options["max-redirection"].as<int>();
    if (options.count("verbose"))
        verbose = true;
    if (options.count("output-file"))
        output_file = options["output-file"].as<string>();
    if (options.count("command"))
    {
        auto cmd_str = options["command"].as<string>();
        if (cmd_str == "get")
            command_type = CommandType::Get;
        else if (cmd_str == "post")
            command_type = CommandType::Post;
        else if (cmd_str == "help")
            command_type = CommandType::Help;
        else
        {
            cerr << "Invalid command " << cmd_str << endl;
            exit(1);
        }
        if (options.count("url"))
            url = options["url"].as<string>();
        else
        {
            cerr << "You forgot to provide the second argument" << endl;
            exit(1);
        }
    }
    if (options.count("get"))
    {
        if (command_type != CommandType::None)
        {
            cerr << "You need to provide at most one command" << endl;
            exit(1);
        }
        command_type = CommandType::Get;
        url = options["get"].as<string>();
    }
    if (options.count("post"))
    {
        if (command_type != CommandType::None)
        {
            cerr << "You need to provide at most one command" << endl;
            exit(1);
        }
        command_type = CommandType::Post;
        url = options["post"].as<string>();
    }
    handle_http_header();
    handle_post_data();
}

void test_get()
{
    HttpClient client;
    auto reply = client.sendGetCommand("http://httpbin.org/get?course=networking&assignment=2", "", true);
    std::cout << "reply: \n" << reply.body << std::endl;
}

void test_post()
{
    HttpClient client;
    auto reply = client.sendPostCommand("http://httpbin.org/post", "{\"Assignment\": 1}", "Content-Type:application/json", true);
    std::cout << "reply: \n" << reply.body << std::endl;
}

void print_reply(HttpMessage& reply)
{
    if (output_file.empty())
    {
        std::cout << "replied message:\n" << reply.body << std::endl;
        return;
    }
    std::ofstream out;
    out.exceptions(std::ifstream::badbit | std::ifstream::failbit);
    try
    {
        if (reply.is_text_body)
            out.open(output_file);
        else
            out.open(output_file, std::ofstream::binary);
        out << reply.body;
    } catch (std::ofstream::failure e)
    {
        std::cerr << "Error in writing into " << output_file << std::endl;
        exit(1);
    }
}

bool check_redirect(HttpMessage& reply)
{
    if (reply.status_code < 300 || reply.status_code > 399)
        return false;
    static int redirect_num = 0;
    if (!redirect || (redirect_num >= max_redirection && max_redirection != -1))
        return false;
    ++redirect_num;
    auto str = reply.http_header["Location"];
    if (str[0] != '/')
        url = str;
    else
    {
        using LUrlParser::clParseURL;
        clParseURL lurl = clParseURL::ParseURL(url);
        auto new_url = "http://" + lurl.m_Host + str;
        if (!lurl.m_Query.empty())
        {
            new_url.push_back('?');
            new_url += lurl.m_Query;
        }
        if (!lurl.m_Fragment.empty())
        {
            new_url.push_back('#');
            new_url += lurl.m_Fragment;
        }
        url = new_url;
    }
    std::cout << redirect_num << ": Redirecting to " << url << std::endl << "************" << std::endl;
    return true;
}

void execute_user_request()
{
    HttpClient client;
    HttpMessage reply;
    switch (command_type)
    {
    case CommandType::Get:
        do
        {
            reply = client.sendGetCommand(url, header, verbose);
        } while(check_redirect(reply));
        print_reply(reply);
        break;
    case CommandType::Post:
        reply = client.sendPostCommand(url, post_data, header, verbose);
        print_reply(reply);
        break;
    case CommandType::Help:
        print_help(url);
    default:
        print_all_helps();
    }
}

int main(int argc, char* argv[])
{
    process_input_args(argc, argv);
    execute_user_request();
    //test_get();
    //test_post();
}
