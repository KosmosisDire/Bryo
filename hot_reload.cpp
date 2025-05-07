#include "hot_reload.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>

HotReload::HotReload(const std::vector<std::string> &initial_paths, StringFileReloadCallback string_callback)
    : _string_callback(string_callback), _binary_callback(nullptr), _use_string_callback(true)
{
    for (const auto &path : initial_paths)
    {
        add_file_to_watch(path);
    }
    initial_scan();
}

HotReload::HotReload(const std::vector<std::string> &initial_paths, BinaryFileReloadCallback binary_callback)
    : _string_callback(nullptr), _binary_callback(binary_callback), _use_string_callback(false)
{
    for (const auto &path : initial_paths)
    {
        add_file_to_watch(path);
    }
    initial_scan();
}

bool HotReload::add_file_to_watch(const std::string &path_str)
{
    std::filesystem::path fs_path(path_str);
    if (!std::filesystem::exists(fs_path) || !std::filesystem::is_regular_file(fs_path))
    {
        std::cerr << "HotReload: Cannot watch '" << path_str << "'. Not a regular file or does not exist." << std::endl;
        return false;
    }

    if (_watched_files_info.find(path_str) == _watched_files_info.end())
    {
        _watched_file_paths.push_back(path_str);
        try
        {
            _watched_files_info[path_str] = {std::filesystem::last_write_time(fs_path)};
        }
        catch (const std::filesystem::filesystem_error &e)
        {
            std::cerr << "HotReload: Error getting initial last write time for '" << path_str << "': " << e.what() << std::endl;
            // Remove if we couldn't get info
            _watched_file_paths.erase(std::remove(_watched_file_paths.begin(), _watched_file_paths.end(), path_str), _watched_file_paths.end());
            _watched_files_info.erase(path_str);
            return false;
        }
        std::cout << "HotReload: Watching file '" << path_str << "'" << std::endl;
        return true;
    }
    return false; // Already watching
}

bool HotReload::remove_file_from_watch(const std::string &path_str)
{
    auto it_info = _watched_files_info.find(path_str);
    if (it_info != _watched_files_info.end())
    {
        _watched_files_info.erase(it_info);

        auto it_path = std::find(_watched_file_paths.begin(), _watched_file_paths.end(), path_str);
        if (it_path != _watched_file_paths.end())
        {
            _watched_file_paths.erase(it_path);
        }
        std::cout << "HotReload: Stopped watching file '" << path_str << "'" << std::endl;
        return true;
    }
    return false; // Not found
}

void HotReload::initial_scan()
{
    std::cout << "HotReload: Initial scan complete. " << _watched_file_paths.size() << " files tracked." << std::endl;
}

void HotReload::poll_changes()
{
    for (const auto &path_str : _watched_file_paths)
    {
        std::filesystem::path fs_path(path_str);
        if (!std::filesystem::exists(fs_path))
        {
            std::cerr << "HotReload: Warning - file '" << path_str << "' does not exist. Skipping." << std::endl;
            continue;
        }

        try
        {
            auto current_mod_time = std::filesystem::last_write_time(fs_path);
            auto it = _watched_files_info.find(path_str);

            if (it != _watched_files_info.end())
            {
                if (current_mod_time > it->second.last_modified_time)
                {
                    std::cout << "HotReload: File '" << path_str << "' changed. Reloading." << std::endl;
                    process_file_change(path_str);
                    it->second.last_modified_time = current_mod_time; // Update time after processing
                }
            }
            else
            {
                std::cerr << "HotReload: Warning - untracked file '" << path_str << "' detected in poll. Adding it." << std::endl;
                _watched_files_info[path_str] = {current_mod_time};
                process_file_change(path_str); // Process it as it's "new" to us
            }
        }
        catch (const std::filesystem::filesystem_error &e)
        {
            std::cerr << "HotReload: Error accessing file '" << path_str << "': " << e.what() << std::endl;
        }
    }
}

void HotReload::process_file_change(const std::string &path)
{
    if (_use_string_callback && _string_callback)
    {
        std::string content_str;
        if (read_file_content_string(path, content_str))
        {
            _string_callback(path, content_str);
        }
        else
        {
            std::cerr << "HotReload: Failed to read string content for '" << path << "'" << std::endl;
        }
    }
    else if (!_use_string_callback && _binary_callback)
    {
        std::vector<char> content_buffer;
        if (read_file_content(path, content_buffer))
        {
            _binary_callback(path, content_buffer);
        }
        else
        {
            std::cerr << "HotReload: Failed to read binary content for '" << path << "'" << std::endl;
        }
    }
}

bool HotReload::read_file_content(const std::string &path, std::vector<char> &content_buffer)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate); // ate: open at end to get size
    if (!file.is_open())
    {
        return false;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg); // go back to beginning

    content_buffer.resize(static_cast<size_t>(size));
    if (size > 0)
    {
        if (!file.read(content_buffer.data(), size))
        {
            content_buffer.clear(); // Clear on failure
            return false;
        }
    }
    return true;
}

bool HotReload::read_file_content_string(const std::string &path, std::string &content_str)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        return false;
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    content_str.resize(static_cast<size_t>(size));
    if (size > 0)
    {
        if (!file.read(&content_str[0], size))
        {
            content_str.clear();
            return false;
        }
    }
    return true;
}