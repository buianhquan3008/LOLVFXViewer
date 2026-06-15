#pragma once

#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace lol
{
enum class LogLevel
{
    Info,
    Warning,
    Error
};

struct LogEntry
{
    std::chrono::system_clock::time_point timestamp;
    LogLevel level = LogLevel::Info;
    std::string message;
    std::optional<std::string> path;
    std::optional<int> line;
    std::optional<int> column;
};

class Log
{
public:
    static void Initialize();
    static const std::filesystem::path& LogFilePath();
    static void Push(LogLevel level, std::string message,
                     std::optional<std::string> path = std::nullopt,
                     std::optional<int> line = std::nullopt,
                     std::optional<int> column = std::nullopt);
    static std::vector<LogEntry> Entries();

private:
    static std::deque<LogEntry>& Buffer();
    static std::ofstream& FileStream();
    static std::mutex& Mutex();
};
}
