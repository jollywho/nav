#ifndef FN_CORE_LOG_H
#define FN_CORE_LOG_H

void log_msg(const char* obj, const char* msg, ...) \
  __attribute__((format (printf, 2, 3)));

#endif