#include "core/Log.h"

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace lol
{
void Log::Initialize()
{
    {
        std::scoped_lock lock(Mutex());
        std::filesystem::create_directories(LogFilePath().parent_path());
        auto& stream = FileStream();
        if (!stream.is_open())
        {
            stream.open(LogFilePath(), std::ios::out | std::ios::trunc);
        }

        stream << "=== LoL Asset Studio startup ===\n";
        stream.flush();
    }

    Push(LogLevel::Info, "LoL Asset Studio logging initialized");
}

void Log::Push(LogLevel level, std::string message,
               std::optional<std::string> path,
               std::optional<int> line,
               std::optional<int> column)
{
    LogEntry entry{
        .timestamp = std::chrono::system_clock::now(),
        .level = level,
        .message = std::move(message),
        .path = std::move(path),
        .line = line,
        .column = column,
    };

    std::scoped_lock lock(Mutex());
    auto& buffer = Buffer();
    buffer.push_back(entry);

    constexpr std::size_t maxEntries = 500;
    while (buffer.size() > maxEntries)
    {
        buffer.pop_front();
    }

    std::time_t time = std::chrono::system_clock::to_time_t(entry.timestamp);
    std::tm localTime{};
    localtime_s(&localTime, &time);

    std::ostringstream stream;
    stream << std::put_time(&localTime, "%H:%M:%S") << " ";
    stream << (entry.level == LogLevel::Info ? "[Info] " : entry.level == LogLevel::Warning ? "[Warn] " : "[Error] ");
    stream << entry.message;
    if (entry.path)
    {
        stream << " | " << *entry.path;
    }
    if (entry.line)
    {
        stream << ":" << *entry.line;
        if (entry.column)
        {
            stream << ":" << *entry.column;
        }
    }

    std::cout << stream.str() << '\n';
    auto& file = FileStream();
    if (file.is_open())
    {
        file << stream.str() << '\n';
        file.flush();
    }
}

std::vector<LogEntry> Log::Entries()
{
    std::scoped_lock lock(Mutex());
    return {Buffer().begin(), Buffer().end()};
}

std::deque<LogEntry>& Log::Buffer()
{
    static std::deque<LogEntry> entries;
    return entries;
}

const std::filesystem::path& Log::LogFilePath()
{
    static const std::filesystem::path path = std::filesystem::current_path() / "logs" / "LoLAssetStudio.log";
    return path;
}

std::ofstream& Log::FileStream()
{
    static std::ofstream stream;
    return stream;
}

std::mutex& Log::Mutex()
{
    static std::mutex mutex;
    return mutex;
}
}
