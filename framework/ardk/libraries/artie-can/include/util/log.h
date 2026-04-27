/**
 * @file log.h
 * @brief The logging subsystem for CAN library. This provides a simple logging interface
 * for debugging - NOT for logging during production. For that, there is a library
 * that sits on top of this library.
 *
 */

#pragma once

#include <stdio.h>

/** Whether we should turn logging on or off. */
#ifndef ARTIE_CAN_LOGGING_ENABLED
    #define ARTIE_CAN_LOGGING_ENABLED 0
#endif

#if ARTIE_CAN_LOGGING_ENABLED
    /** Printf functionality when logging is enabled. Includes node address from context. */
    #define ARTIE_CAN_LOG(context, fmt, ...) \
        do { \
            if (context) { \
                printf("[ARTIE CAN][Node %d] " fmt "\n", (context)->rtacp_context.node_address, ##__VA_ARGS__); \
            } else { \
                printf("[ARTIE CAN][Node unknown] " fmt "\n", ##__VA_ARGS__); \
            } \
        } while(0)
#else
    /** Printf functionality when logging is enabled. */
    #define ARTIE_CAN_LOG(context, fmt, ...) do { } while(0)
#endif
