#pragma once
#include <string>
#include <vector>
#include <functional>
#include <mutex>

namespace RTBEngine {
    namespace Core {

        enum class LogLevel {
            Info,
            Warning,
            Error
        };

        struct LogMessage {
            LogLevel level;
            std::string message;
            std::string timestamp;
        };

        class Logger {
        public:
            static Logger& GetInstance();

            void Log(LogLevel level, const std::string& message);
            void Info(const std::string& message) { Log(LogLevel::Info, message); }
            void Warning(const std::string& message) { Log(LogLevel::Warning, message); }
            void Error(const std::string& message) { Log(LogLevel::Error, message); }

            using LogCallback = std::function<void(const LogMessage&)>;
            void AddCallback(LogCallback callback);

            const std::vector<LogMessage>& GetLogs() const { return logs; }
            void Clear() { logs.clear(); }

        private:
            Logger() = default;
            ~Logger() = default;
            Logger(const Logger&) = delete;
            Logger& operator=(const Logger&) = delete;

            std::vector<LogMessage> logs;
            std::vector<LogCallback> callbacks;
            mutable std::mutex logMutex;
        };

    }
}

// Macros for easier logging
#define RTB_INFO(msg) RTBEngine::Core::Logger::GetInstance().Info(msg)
#define RTB_WARN(msg) RTBEngine::Core::Logger::GetInstance().Warning(msg)
#define RTB_ERROR(msg) RTBEngine::Core::Logger::GetInstance().Error(msg)
