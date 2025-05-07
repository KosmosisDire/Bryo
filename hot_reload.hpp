#pragma once

#include <string>
#include <vector>
#include <functional>
#include <filesystem>
#include <map>
#include <chrono>

using StringFileReloadCallback = std::function<void(const std::string & /*path*/, const std::string & /*content*/)>;
using BinaryFileReloadCallback = std::function<void(const std::string & /*path*/, const std::vector<char> & /*content*/)>;

class HotReload
{
public:
    HotReload(const std::vector<std::string> &initial_paths, StringFileReloadCallback string_callback);
    HotReload(const std::vector<std::string> &initial_paths, BinaryFileReloadCallback binary_callback);

    bool add_file_to_watch(const std::string &path);
    bool remove_file_from_watch(const std::string &path);
    void poll_changes();

private:
    struct FileInfo
    {
        std::filesystem::file_time_type last_modified_time;
    };

    std::map<std::string, FileInfo> _watched_files_info;
    std::vector<std::string> _watched_file_paths;

    StringFileReloadCallback _string_callback;
    BinaryFileReloadCallback _binary_callback;

    bool _use_string_callback;

    bool read_file_content(const std::string &path, std::vector<char> &content_buffer);
    bool read_file_content_string(const std::string &path, std::string &content_str);

    void initial_scan();
    void process_file_change(const std::string &path);
};