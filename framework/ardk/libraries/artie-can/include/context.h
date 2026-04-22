/**
 * @file context.h
 * @brief Context definitions for Artie CAN library (C implementation).
 *
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "rtacp_context.h"
#include "translationlayer.h"

/** The callback function that gets executed whenever a non-filtered CAN frame is received. */
typedef void artie_can_rx_callback_t(const artie_can_frame_t *frame);

/**
 * @brief Enumeration of supported protocols in the Artie CAN library.
 */
typedef enum {
    ARTIE_CAN_PROTOCOL_FLAG_RTACP = 1 << 0,     ///< RTACP protocol
    ARTIE_CAN_PROTOCOL_FLAG_RPCACP = 1 << 1,    ///< RPCACP protocol
    ARTIE_CAN_PROTOCOL_FLAG_PSACP = 1 << 2,     ///< PSAP protocol
    ARTIE_CAN_PROTOCOL_FLAG_BWACP = 1 << 3,     ///< BWACP protocol
} artie_can_protocol_t;

/** Number of items in the artie_can_protocol_t enum */
#define ARTIE_CAN_PROTOCOL_COUNT 4U

/**
 * @brief Struct to hold event loop thread management data.
 */
typedef struct {
    thread_handle_t thread;          ///< Handle to the event loop thread.
    volatile bool running;           ///< Flag indicating whether the event loop thread should continue running.
    uint32_t tick_interval_us;       ///< Interval in microseconds between tick calls in the event loop thread.
} event_loop_data_t;

/**
 * @brief Struct for Artie CAN library's state and configuration.
 *
 */
typedef struct {
    void *backend_context;                  ///< Pointer to backend-specific context data (which could be custom)
    rtacp_context_t rtacp_context;          ///< Context for RTACP protocol handling.
    uint16_t protocol_flags;                ///< Mask of protocols that we are interested in. Use OR'd members of the artie_can_protocol_t enum.
    event_loop_data_t event_loop;           ///< Event loop thread management (only used if artie_can_start_event_loop is called)
    artie_can_rx_callback_t *rx_callback;   ///< Callback function for received CAN frames
} artie_can_context_t;
