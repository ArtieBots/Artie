#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "context.h"
#include "log.h"

// Platform-specific includes for timestamps and thread safety
#ifdef _WIN32
    #include <windows.h>
#else
    #include <sys/time.h>
    #include <time.h>
    #include <pthread.h>
#endif

void artie_can_log(const artie_can_context_t *context, const char *fmt, ...)
{
    // Create a stack-allocated buffer for the timestamp, since we need to ensure
    // thread-safety.
    char timestamp_buffer[32];
#ifdef _WIN32
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(timestamp_buffer, sizeof(timestamp_buffer), "[%02d:%02d:%02d.%03d]", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
#else
    struct timeval tv;
    struct tm *tm_info;
    gettimeofday(&tv, NULL);
    tm_info = localtime(&tv.tv_sec);
    snprintf(timestamp_buffer, sizeof(timestamp_buffer), "[%02d:%02d:%02d.%03ld]", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec, tv.tv_usec / 1000);
#endif

    // Print the log message with the timestamp and node address from context
    if (context) {
        printf("%s[ARTIE CAN][Node %d] ", timestamp_buffer, context->node_address);
    } else {
        printf("%s[ARTIE CAN][Node unknown] ", timestamp_buffer);
    }

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    fflush(stdout);
}
