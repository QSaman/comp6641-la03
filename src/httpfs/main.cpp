#include <iostream>
#include <string>
#include <exception>
#include <thread>
#include <cxxopts.hpp>
#include <vector>

#include "../libhttpfs/http_server.h"
#include "../libhttpfs/filesystem.h"
#include "../libhttpc/http_client.h"
#include <boost/filesystem.hpp>
#include <asio.hpp>

extern std::string root_dir_path;
extern std::string mime_file;
extern std::string icons_dir_path;
extern bool no_cache;
extern int children_level;
extern std::vector<std::string> mime_filter_list;
extern TransportProtocol transport_protocol;
extern unsigned int window_size;

std::string root_dir_path = "./resources/httpfs_root";
std::string icons_dir_path = "./resources";
std::string mime_file = "./resources/mime.types";
std::vector<std::string> mime_filter_list;
static cxxopts::Options options("httpfs", "httpfs is a simple file server.");
bool verbose = false;
bool no_cache = false;
int children_level = 0;
static unsigned short port = 8080;
TransportProtocol transport_protocol = TransportProtocol::UDP;
unsigned int window_size = 1;

[[noreturn]] void printHelp()
{
    std::cout << options.help({""}) << std::endl;
    std::exit(0);
}

void cli(int argc, char* argv[])
{
    options.add_options()
            ("v,verbose", " Prints debugging messages")
            ("p,port", " Specifies the port number that the server will listen and serve at."
                       " Default is 8080", cxxopts::value<int>(), "num")
            ("t,protocol", "The underlying protocol. The default is udp",
             cxxopts::value<std::string>(), "tcp/udp")
            ("w,window-size", "Window size. See Selective Repeat aglorithm for more information."
                         " The default is 1.", cxxopts::value<unsigned int>(), "num")
            ("d,dir", " Specifies the directory that the server will use to read/write requested"
                      " files. Default is resources/https_root in the current directory when"
                      " launching the application", cxxopts::value<std::string>(), "dir_path")
            ("h,help", "Show this page")
            ("m,mime-file", "A file containing the mapping between file extension and MIME types"
                            " in Apache format. The default is resources/mime.types",
             cxxopts::value<std::string>(), "mime_file")
            ("c,no-cache", "Tell the client to not cache any data. It's useful for debugging"
                           " purposes (e.g. testing concurrency).")
            ("l,level", "The level of directory tree. This is meaningful when client request"
             "for JSON or XML output. The default is 0", cxxopts::value<int>(), "num")
            ("C,content-disposal", "A mime type which server suggest to client to not show them inline"
                                   " by using Content-Disposition in response header. You can use this "
                                   "parameter multiple times for different filters. For example if you use"
                                   "\"video\" as filter, you suggest to save all type of video files",
             cxxopts::value<std::vector<std::string>>(), "mime_type");
    options.parse(argc, argv);


    if (options.count("help"))
        printHelp();
    if (options.count("protocol"))
    {
        auto tmp = options["protocol"].as<std::string>();
        if (tmp == "tcp" || tmp == "TCP")
            transport_protocol = TransportProtocol::TCP;
    }
    if (options.count("window-size"))
    {
        if (transport_protocol != TransportProtocol::UDP)
        {
            std::cerr << "Option window-size is only available for UDP" << std::endl;
            exit(1);
        }
        window_size = options["window-size"].as<unsigned int>();
    }
    if (options.count("verbose"))
        verbose = true;
    if (options.count("port"))
        port = options["port"].as<unsigned short>();
    if (options.count("dir"))
        root_dir_path = options["dir"].as<std::string>();
    if (options.count("mime-file"))
        mime_file = options["mime-file"].as<std::string>();
    if (options.count("no-cache"))
        no_cache = true;
    if (options.count("level"))
        children_level = options["level"].as<int>();
    if (options.count("content-disposal"))
        mime_filter_list = options["content-disposal"].as<std::vector<std::string>>();
}

int main(int argc, char* argv[])
{
    cli(argc, argv);
    runHttpServer(port);
}
 
