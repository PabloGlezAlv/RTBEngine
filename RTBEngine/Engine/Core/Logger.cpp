#include "Logger.h"
#include <iostream>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace RTBEngine {
    namespace Core {

        Logger& Logger::GetInstance() {
            static Logger instance;
            return instance;
        }

        void Logger::Log(LogLevel level, const std::string& message) {
            std::lock_guard<std::mutex> lock(logMutex);

            // Get current time
            auto now = std::time(nullptr);
            std::tm tm;
            localtime_s(&tm, &now);
            std::ostringstream oss;
            oss << std::put_time(&tm, "%H:%M:%S");
            std::string timestamp = oss.str();

            LogMessage logMsg = { level, message, timestamp };
            logs.push_back(logMsg);

            // Print to console for debug
            const char* levelStr = "";
            switch (level) {
                case LogLevel::Info:    levelStr = "[INFO]"; break;
                case LogLevel::Warning: levelStr = "[WARN]"; break;
                case LogLevel::Error:   levelStr = "[ERR ]"; break;
            }
            std::cout << "[" << timestamp << "]" << levelStr << " " << message << std::endl;

            // Trigger callbacks
            for (auto& callback : callbacks) {
                callback(logMsg);
            }
        }

        void Logger::AddCallback(LogCallback callback) {
            std::lock_guard<std::mutex> lock(logMutex);
            callbacks.push_back(callback);
        }

    }
}
