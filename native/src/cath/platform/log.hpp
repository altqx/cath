#pragma once

namespace cath {

enum class LogLevel { Info, Warn, Error };

void log_message(LogLevel level, const char* fmt, ...);

inline void log_info(const char* fmt, ...) {
  // implemented via log_message in .cpp through macros below
}

}  // namespace cath

#define CATH_LOG_INFO(...) ::cath::log_message(::cath::LogLevel::Info, __VA_ARGS__)
#define CATH_LOG_WARN(...) ::cath::log_message(::cath::LogLevel::Warn, __VA_ARGS__)
#define CATH_LOG_ERROR(...) ::cath::log_message(::cath::LogLevel::Error, __VA_ARGS__)
