/**
 * @file log.h
 * @brief The logging subsystem for CAN library. This provides a simple logging interface
 * for debugging - NOT for logging during production. For that, there is a library
 * that sits on top of this library.
 *
 */

#pragma once

/** Whether we should turn logging on or off. */
#ifndef ARTIE_CAN_LOGGING_ENABLED
    #define ARTIE_CAN_LOGGING_ENABLED 0
#endif


#if ARTIE_CAN_LOGGING_ENABLED
    void artie_can_log(const artie_can_context_t *context, const char *fmt, ...);
    #define ARTIE_CAN_LOG(context, fmt, ...) artie_can_log(context, fmt, ##__VA_ARGS__)
#else
    #define ARTIE_CAN_LOG(context, fmt, ...) do {} while(0)
#endif // ARTIE_CAN_LOGGING_ENABLED
