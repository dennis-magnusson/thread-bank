#include "logger.h"
#include <sys/time.h>
#include <time.h>
#include <stdio.h>

void log_with_timestamp(FILE* log_file, const char* message) {
  struct timeval tv;
  char timestamp[64];
  gettimeofday(&tv, NULL);
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&tv.tv_sec));
  fprintf(log_file, "[%s] %s\n", timestamp, message);
  fflush(log_file);
}
