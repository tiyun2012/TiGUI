#include "Log.h"

#include <windows.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>

namespace
{
    // ------------------------------------------------------------------------
    // Logger state.
    //
    // Kept private to this .cpp file so no other part of the application can
    // directly manipulate the logger's internal file/mutex state.
    // ------------------------------------------------------------------------
    std::ofstream gLogFile;

    std::mutex gLogMutex;

    bool gInitialized = false;

    // ------------------------------------------------------------------------
    // Convert UTF-8/std::string into UTF-16.
    //
    // OutputDebugStringW is a wide-character Windows API, so we convert our
    // UTF-8/std::string log message before sending it to Visual Studio.
    // ------------------------------------------------------------------------
    std::wstring ToWide(
        std::string_view text)
    {
        if (text.empty())
        {
            return {};
        }

        const int required =
            MultiByteToWideChar(
                CP_UTF8,
                0,
                text.data(),
                static_cast<int>(text.size()),
                nullptr,
                0);

        if (required <= 0)
        {
            return L"<UTF-8 conversion failed>";
        }

        std::wstring result(
            required,
            L'\0');

        MultiByteToWideChar(
            CP_UTF8,
            0,
            text.data(),
            static_cast<int>(text.size()),
            result.data(),
            required);

        return result;
    }
}

bool Logger::Initialize(
    std::string_view filePath)
{
    std::scoped_lock lock(gLogMutex);

    // Close an existing file if Initialize() is called again.
    if (gLogFile.is_open())
    {
        gLogFile.close();
    }

    try
    {
        const std::filesystem::path path(filePath);

        // Create the parent directory automatically.
        //
        // Example:
        //
        //     logs/MyUI.log
        //
        // creates:
        //
        //     logs\
        //
        if (path.has_parent_path())
        {
            std::filesystem::create_directories(
                path.parent_path());
        }

        // Append instead of overwriting previous runs.
        gLogFile.open(
            path,
            std::ios::out | std::ios::app);

        if (!gLogFile.is_open())
        {
            gInitialized = false;
            return false;
        }

        gInitialized = true;

        return true;
    }
    catch (...)
    {
        gInitialized = false;
        return false;
    }
}

void Logger::Shutdown()
{
    std::scoped_lock lock(gLogMutex);

    if (gLogFile.is_open())
    {
        gLogFile.flush();
        gLogFile.close();
    }

    gInitialized = false;
}

void Logger::Write(
    LogLevel level,
    std::string_view message)
{
    std::scoped_lock lock(gLogMutex);

    const std::string line =
        std::format(
            "[{}] [{}] {}",
            MakeTimestamp(),
            ToString(level),
            message);

    // ========================================================================
    // Sink 1: File
    //
    // The full message is appended to:
    //
    //     logs/MyUI.log
    // ========================================================================
    if (gInitialized &&
        gLogFile.is_open())
    {
        gLogFile << line << '\n';
        gLogFile.flush();
    }

    // ========================================================================
    // Sink 2: Visual Studio debugger
    //
    // This appears in:
    //
    //     Debug
    //       -> Windows
    //          -> Output
    //
    // when running under Visual Studio.
    // ========================================================================
    OutputDebugStringW(
        (ToWide(line) + L"\n").c_str());
}

std::string Logger::MakeTimestamp()
{
    const auto now =
        std::chrono::system_clock::now();

    const auto time =
        std::chrono::system_clock::to_time_t(now);

    std::tm localTime{};

    // Thread-safe Windows version of localtime().
    localtime_s(
        &localTime,
        &time);

    // Add milliseconds so events occurring within the same second can still
    // be distinguished.
    const auto milliseconds =
        std::chrono::duration_cast<
            std::chrono::milliseconds>(
                now.time_since_epoch()) %
        1000;

    std::ostringstream stream;

    stream
        << std::put_time(
               &localTime,
               "%Y-%m-%d %H:%M:%S")
        << '.'
        << std::setfill('0')
        << std::setw(3)
        << milliseconds.count();

    return stream.str();
}

const char* Logger::ToString(
    LogLevel level)
{
    switch (level)
    {
        case LogLevel::Info:
            return "INFO ";

        case LogLevel::Warning:
            return "WARN ";

        case LogLevel::Error:
            return "ERROR";
    }

    return "?????";
}