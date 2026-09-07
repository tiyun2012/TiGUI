#pragma once

#include <format>
#include <string_view>
#include <utility>

// ============================================================================
// LogLevel
//
// Three basic levels are enough for Milestone 1.
//
// Logger belongs to Core rather than Platform or Renderer because every
// subsystem should be able to use logging:
//
//     Core
//       ↑
//       ├── Platform
//       ├── Renderer
//       └── future UI
//
// Later we can add categories, filtering, console sinks, asynchronous logging,
// profiling output, and a GUI Output Log panel.
// ============================================================================
enum class LogLevel
{
    Info,
    Warning,
    Error
};

// ============================================================================
// Logger
//
// Current output destinations:
//
//     1. logs/MyUI.log
//     2. Visual Studio Output window through OutputDebugStringW
//
// We intentionally do NOT create a visual Output Log panel yet.
// That will be part of the future UI framework.
// ============================================================================
class Logger
{
public:
    // Create/open the log file.
    //
    // The default path is relative to the application's working directory:
    //
    //     logs/MyUI.log
    //
    static bool Initialize(
        std::string_view filePath = "logs/MyUI.log");

    // Flush and close the log file.
    static void Shutdown();

    // Write an already formatted message.
    static void Write(
        LogLevel level,
        std::string_view message);

    // ------------------------------------------------------------------------
    // Formatted helpers.
    //
    // Example:
    //
    //     Logger::Info("Window size: {}x{}", width, height);
    //
    //     Logger::Error(
    //         "CreateDevice failed. HRESULT=0x{:08X}",
    //         hr);
    // ------------------------------------------------------------------------

    template <typename... Args>
    static void Info(
        std::format_string<Args...> format,
        Args&&... args)
    {
        Write(
            LogLevel::Info,
            std::format(
                format,
                std::forward<Args>(args)...));
    }

    template <typename... Args>
    static void Warning(
        std::format_string<Args...> format,
        Args&&... args)
    {
        Write(
            LogLevel::Warning,
            std::format(
                format,
                std::forward<Args>(args)...));
    }

    template <typename... Args>
    static void Error(
        std::format_string<Args...> format,
        Args&&... args)
    {
        Write(
            LogLevel::Error,
            std::format(
                format,
                std::forward<Args>(args)...));
    }

private:
    static std::string MakeTimestamp();

    static const char* ToString(
        LogLevel level);
};

// ============================================================================
// Convenience macros
//
// These keep subsystem code clean:
//
//     LOG_INFO("Renderer initialized");
//     LOG_WARNING("Something unusual happened");
//     LOG_ERROR("Could not create device");
// ============================================================================
#define LOG_INFO(...)    Logger::Info(__VA_ARGS__)
#define LOG_WARNING(...) Logger::Warning(__VA_ARGS__)
#define LOG_ERROR(...)   Logger::Error(__VA_ARGS__)