#include "cath/platform/log.hpp"

#include <cstdarg>
#include <cstdio>

namespace cath {

void log_message(LogLevel level, const char* fmt, ...) {
  const char* tag = "I";
  FILE* out = stdout;
  if (level == LogLevel::Warn) {
    tag = "W";
  } else if (level == LogLevel::Error) {
    tag = "E";
    out = stderr;
  }
  std::fprintf(out, "[cath:%s] ", tag);
  va_list ap;
  va_start(ap, fmt);
  std::vfprintf(out, fmt, ap);
  va_end(ap);
  std::fputc('\n', out);
}

}  // namespace cath
