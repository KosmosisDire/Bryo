#ifndef FILE_HOT_RELOADER_HPP
#define FILE_HOT_RELOADER_HPP

#include <string>
#include <vector>
#include <functional>
#include <filesystem> // Requires C++17
#include <map>
#include <chrono> // For file_time_type comparison

// Forward declaration
class HotReload;

// Callback types
// Callback for when a file is reloaded (content as string)
using StringFileReloadCallback = std::function<void(const std::string& /*path*/, const std::string& /*content*/)>;
// Callback for when a file is reloaded (content as binary)
using BinaryFileReloadCallback = std::function<void(const std::string& /*path*/, const std::vector<char>& /*content*/)>;


class HotReload {
public:
    // Constructor: Initialize with paths and a string-based callback
    HotReload(const std::vector<std::string>& initial_paths, StringFileReloadCallback string_callback);

    // Constructor: Initialize with paths and a binary-based callback
    HotReload(const std::vector<std::string>& initial_paths, BinaryFileReloadCallback binary_callback);

    // Add a file path to be watched
    // Returns true if added, false if already watched or path is invalid
    bool add_file_to_watch(const std::string& path);

    // Remove a file path from being watched
    // Returns true if removed, false if not found
    bool remove_file_from_watch(const std::string& path);

    // Call this regularly to check for file changes
    void poll_changes();

private:
    struct FileInfo {
        std::filesystem::file_time_type last_modified_time;
        // Add other info if needed, e.g., size, hash
    };

    std::map<std::string, FileInfo> _watched_files_info;
    std::vector<std::string> _watched_file_paths; // To maintain order and easy iteration

    StringFileReloadCallback _string_callback;
    BinaryFileReloadCallback _binary_callback;

    bool _use_string_callback; // To know which callback to use

    // Helper to read file content
    bool read_file_content(const std::string& path, std::vector<char>& content_buffer);
    bool read_file_content_string(const std::string& path, std::string& content_str);

    void initial_scan(); // Scan files on startup to get initial state
    void process_file_change(const std::string& path);
};

#endif // FILE_HOT_RELOADER_HPP