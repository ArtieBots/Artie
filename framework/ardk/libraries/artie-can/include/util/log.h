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
    /**
     * @brief Print a debug log message. Only compiled in when ARTIE_CAN_LOGGING_ENABLED is set.
     * Prefer the ARTIE_CAN_LOG() macro over calling this directly, since it compiles away entirely
     * (including its arguments) when logging is disabled.
     *
     * @param context Pointer to the artie_can_context_t struct, in case logging wants to include node-specific info. May be NULL.
     * @param fmt printf-style format string.
     * @param ... Arguments for the format string.
     */
    void artie_can_log(const artie_can_context_t *context, const char *fmt, ...);
    /** Log a debug message via artie_can_log() if ARTIE_CAN_LOGGING_ENABLED is set, otherwise a no-op. */
    #define ARTIE_CAN_LOG(context, fmt, ...) artie_can_log(context, fmt, ##__VA_ARGS__)
#else
    /** Log a debug message via artie_can_log() if ARTIE_CAN_LOGGING_ENABLED is set, otherwise a no-op. */
    #define ARTIE_CAN_LOG(context, fmt, ...) do {} while(0)
#endif // ARTIE_CAN_LOGGING_ENABLED
