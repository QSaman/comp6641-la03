#pragma once

#include <iosfwd>

extern std::string mime_file;
extern bool verbose;

void initExt2Mime();
std::string textDirList(const std::string& dir_path);
std::string jsonDirList(const std::string& dir_path, int level = 0);
std::string xmlDirList(const std::string& dir_path, int level);
std::string htmlDirList(const std::string& root_dir_path, const std::string& relative_dir_path);
std::string fileContent(const std::string& file_path, bool binary = false);
void write2File(const std::string& file_path, const std::string& content, bool binary = false);
std::string getFileMimeType(const std::string& file_path);
std::string url_encode(const std::string &value);
std::string url_decode(const std::string &value);


