#include "filesystem.h"
#include "../libhttpc/http_client.h"

#include <string>
#include <iostream>
#include <iomanip>
#include <unordered_map>

#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <nlohmann/json.hpp>

#include <boost/thread/thread.hpp>
#include <thread>
#include <mutex>


static std::unordered_map<std::string, std::string> ext2mime;

static std::unordered_map<std::string, boost::shared_mutex> shared_mutex_list;
static std::mutex map_mutex;

boost::shared_mutex& fileMutex(const std::string& file_path)
{
    std::unique_lock<std::mutex>{map_mutex};
    boost::shared_mutex& ret = shared_mutex_list[file_path];
    return ret;
}

boost::filesystem::path createBoostDirPath(const std::string& dir_path)
{
    using namespace boost::filesystem;
    path p(dir_path);
    if (!exists(p))
    {
        //TODO
    }
    if (!is_directory(p))
    {
        //TODO
    }
    return p;
}

std::string textDirList(const std::string& dir_path)
{    
    using namespace boost::filesystem;
    std::string ret;
    path p = createBoostDirPath(dir_path);
    for (directory_entry& f : directory_iterator(p))
        ret += std::string(f.path().filename().string()) + "\n";
    return ret;
}

nlohmann::json jsonDirList(const boost::filesystem::path& p, int level)
{
    using namespace boost::filesystem;
    using json = nlohmann::json;

    nlohmann::json ret;
    for (directory_entry& f : directory_iterator(p))
    {
        bool directory = false;
        nlohmann::json object = json::object();
        object["name"] = f.path().filename().string();
        if (is_directory(f))
        {
            object["type"] = "directory";
            directory = true;
        }
        else if (is_regular_file(f))
            object["type"] = "file";
        else
            object["type"] = "unknown";
        if (directory && level > 0)
            object["children"] = jsonDirList(f, level - 1);
        ret.push_back(object);
    }
    return ret;
}

std::string jsonDirList(const std::string& dir_path, int level)
{
    using namespace boost::filesystem;
    std::string ret;
    path p = createBoostDirPath(dir_path);

    nlohmann::json jres = jsonDirList(p, level);
    ret = jres.dump(2);
    return ret;
}

boost::property_tree::ptree xmlDirList(const boost::filesystem::path& p, int level)
{
    namespace pt = boost::property_tree;
    namespace fs = boost::filesystem;

    pt::ptree root;
    for (fs::directory_entry& f : fs::directory_iterator(p))
    {
        std::string node_name = "unknown";
        bool directory = false;
        pt::ptree node;
        if (fs::is_directory(f))
        {
            node_name = "directory";
            directory = true;
        }
        else if (fs::is_regular_file(f))
            node_name = "file";
        node.put("name", f.path().filename().string());
        root.add_child(node_name, node);
        if (directory && level > 0)
        {
            auto child = xmlDirList(f.path(), level - 1);
            root.add_child(node_name + "." + "children", child);
        }
    }
    return root;
}

std::string xmlDirList(const std::string& dir_path, int level)
{
    namespace fs = boost::filesystem;
    namespace pt = boost::property_tree;

    fs::path path(dir_path);
    pt::ptree root;
    root.add_child("file_system", xmlDirList(path, level));
    std::stringstream ss;
    boost::property_tree::xml_writer_settings<std::string> settings(' ', 4);
    pt::write_xml(ss, root, settings);
    return ss.str();
}

std::string fileContent(const std::string& file_path, bool binary)
{
    boost::shared_lock<boost::shared_mutex> lock{fileMutex(file_path)};
    if (verbose)
        std::cout << "Got reader lock" << std::endl;
    std::ifstream file;
    using namespace std;

    file.exceptions(std::ifstream::badbit | std::ifstream::failbit);
    std::ostringstream os;
    ios_base::openmode mode = std::ios_base::in;
    if (binary)
        mode = ifstream::binary;
    file.open(file_path, mode);
    os << file.rdbuf();
    return os.str();
}

void write2File(const std::string& file_path, const std::string& content, bool binary)
{
    boost::unique_lock<boost::shared_mutex> lock{fileMutex(file_path)};
    if (verbose)
        std::cout << "Got writer lock" << std::endl;
//    std::cout << "sleep for 30s" << std::endl;
//    std::this_thread::sleep_for(std::chrono::seconds(30));
//    std::cout << "waking up" << std::endl;
    namespace fs = boost::filesystem;
    fs::path path(file_path);
    create_directories(path.parent_path());
    std::ofstream out;
    out.exceptions(std::ofstream::badbit | std::ofstream::failbit);
    if (!binary)
        out.open(path.string());
    else
        out.open(path.string(), std::ofstream::binary);
    out << content;
}

void initExt2Mime()
{
    using namespace std;
    ifstream fin;
    fin.exceptions(ifstream::badbit);
    fin.open(mime_file);
    string line;
    while (getline(fin, line))
    {
        if (line.empty())
            continue;
        unsigned i;
        for (i = 0; i < line.length() && isspace(line[i]); ++i);
        line = line.substr(i);
        if (line[0] == '#')
            continue;
        istringstream iss(line);
        string mime, extension;
        iss >> mime;
        while (iss >> extension)
            ext2mime[extension] = mime;
    }
}

std::string getFileMimeType(const std::string& file_path)
{
    using namespace boost::filesystem;
    path fp(file_path);
    auto extension_str = extension(fp);
    if (!extension_str.empty())
        extension_str = extension_str.substr(1);    //convert .ext -> ext
    return ext2mime[extension_str];
}

std::string human_readable_file_size(const unsigned long file_size)
{
    using std::string;
    using std::ostringstream;

    double size = file_size;
    string unit = "B";

    if (size > 1024)
    {
        unit = "K";
        size /= 1024;
    }
    if (size > 1024)
    {
        unit = "M";
        size /= 1024;
    }
    if (size > 1024)
    {
        unit = "G";
        size /= 1024;
    }
    ostringstream oss;
    if (std::floor(size - 0.5) < size)
        oss << std::setprecision(0);
    else
        oss << std::setprecision(1);
    oss  << std::fixed << size << unit;
    return oss.str();
}

//I copied url_encode from https://stackoverflow.com/questions/154536/encode-decode-urls-in-c
std::string url_encode(const std::string &value)
{
    using namespace std;
    ostringstream escaped;
    escaped.fill('0');
    escaped << hex;

    for (auto i = value.begin(), n = value.end(); i != n; ++i) {
        auto c = (*i);

        // Keep alphanumeric and other accepted characters intact
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }

        // Any other characters are percent-encoded
        escaped << uppercase;
        escaped << '%' << setw(2) << int((unsigned char) c);
        escaped << nouppercase;
    }

    return escaped.str();
}

std::string url_decode(const std::string &value)
{    
    std::string ret;    
    for (unsigned i = 0; i < value.length();)
    {
        if (value[i] == '%')
        {            
            std::string hex_str{value[i + 1], value[i + 2]};
            std::stringstream escaped;
            escaped  << std::hex << hex_str;
            int ch;
            escaped >> std::hex >> ch;
            ret.push_back(static_cast<char>(ch));
            i += 3;
        }
        else
        {
            ret.push_back(value[i]);
            ++i;
        }
    }
    return ret;
}

std::string modifiedTime2String(struct std::tm* tm)
{
    std::ostringstream oss;
    oss << std::put_time(tm, "%F %R");
    return oss.str();
}

//TODO Support UTF8 in Windows!
std::string htmlDirList(const std::string& root_dir_path, const std::string& relative_dir_path)
{
    using std::endl;
    namespace fs = boost::filesystem;
    fs::path dir_path(root_dir_path);
    fs::path file_path(relative_dir_path);
    fs::path full_path = dir_path / file_path;
    std::string relative_parent;
    if (full_path != root_dir_path)
        relative_parent = file_path.parent_path().string();

    std::ostringstream oss(R"(<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 3.2 Final//EN">)");
    oss << endl
        << "<html>" << endl
            << "<head>" << endl
                << "<title>Index of " << file_path.filename().string() << "</title>" << endl
                << R"(<meta charset="UTF-8">)" << endl
            << "</head>" << endl
            << "<body>" << endl
            << "<h1>Index of " << file_path.filename().string() << "</h1>" << endl
            << "<table>" << endl;
    oss << R"(<tr><th valign="top"><img src="/icons/blank.gif" alt="[ICO]"></th><th>Name</th><th>Last modified</th><th>Size</th><th>Description</th></tr>)" << endl;
    oss << R"(<tr><th colspan="5"><hr></th></tr>)" << endl;
    if (!relative_parent.empty())
        oss << R"(<tr><td valign="top"><img src="/icons/back.gif" alt="[PARENTDIR]"></td><td><a href=")"
            << relative_parent <<  R"(">Parent Directory</a>       </td><td>&nbsp;</td><td align="right">  - </td><td>&nbsp;</td></tr>)" << endl;
    for (fs::directory_entry& f : fs::directory_iterator(full_path))
    {
        std::string icon_path = "/icons/unknown.gif";
        std::string alt = "[   ]";
        std::string file_name = f.path().filename().string();
        fs::path f_name(url_encode(file_name));
        std::string link_name = (relative_dir_path / f_name).string();
        unsigned long file_size = 0;
        std::string file_size_str;
        auto mod_time_t = fs::last_write_time(f.path());
        auto modified_time = modifiedTime2String(std::localtime(&mod_time_t));
        if (is_directory(f))
        {
            icon_path = "/icons/folder.gif";
            alt = "[DIR]";
            file_size_str = "-";
        }
        else if (is_regular_file(f))
        {
            file_size = fs::file_size(f.path());
            file_size_str = human_readable_file_size(file_size);
            auto mime_type = getFileMimeType(f.path().filename().string());
            if (HttpClient::isTextBody(mime_type))
            {
                icon_path = "/icons/text.gif";
                alt = "[TXT]";
            }
            else if (mime_type.substr(0, 5) == "image")
            {
                icon_path = "/icons/image2.gif";
                alt = "[IMG]";
            }
            else if (mime_type.substr(0, 5) == "video")
            {
                icon_path = "/icons/movie.gif";
                alt = "[VID]";
            }
        }
//        if (file_name.length() > 23)
//            file_name = file_name.substr(0, 20) + "..>";
        oss << R"(<tr><td valign="top"><img src=")" << icon_path
            << R"(" alt=")" << alt << R"("></td><td><a href=")"
            << link_name << "\">" <<  file_name << R"(</a>               </td><td align="right">)" << modified_time
            << R"(</td><td align="right">)" << file_size_str << R"(</td><td>&nbsp;</td></tr>)" << endl;
    }
    oss << R"(<tr><th colspan="5"><hr></th></tr>)" << endl
        << "</table>" << endl
        << "</body></html>";
    return oss.str();
}

