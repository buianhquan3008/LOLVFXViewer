#include "ui/ConsolePanel.h"

#include "core/Log.h"

#include <imgui.h>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace lol
{
void ConsolePanel::Draw()
{
    ImGui::Begin("Console");

    for (const LogEntry& entry : Log::Entries())
    {
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
        ImGui::TextWrapped("%s", stream.str().c_str());
    }

    ImGui::End();
}
}
