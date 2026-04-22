/**
 * @file backend.h
 * @brief Backend interface definitions for Artie CAN library.
 *
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "context.h"
#include "err.h"
#include "frame.h"

/** The callback function that gets executed whenever a non-filtered CAN frame is received. */
typedef void artie_can_rx_callback_t(artie_can_frame_t *frame);

/**
 * @brief The callback function type for getting the current time in milliseconds.
 * We don't actually care what the time is, we just use this for timeouts, so we just
 * need a monotonically increasing value that represents the passage of time in milliseconds.
 *
 * @return The current time in milliseconds.
 */
typedef uint64_t artie_can_get_ms_t(void);

/**
 * @brief Struct for CAN backend function pointers and context.
 *
 * @param init Function pointer for backend initialization.
 * It is expected that this function will set up the backend and populate the context as needed.
 * Backends can assume this function will be called before any other functions in the backend are used.
 * @param send Function pointer for sending a CAN frame.
 * The backend should handle the actual transmission of the CAN frame. The frame data is provided as an argument.
 * This function is expected to be non-blocking, but even though it returns quickly, the message may not be sent until
 * the bus is free. If send is called again before the previous message has been sent,
 * the backend should return ARTIE_CAN_ERR_SEND_BUSY to indicate that this message cannot be sent, and it should
 * continue trying to send the previous message until some backend-specific timeout mechanism triggers
 * that frees up the backend for sending new messages.
 * @param receive_callback Function pointer to a user-supplied callback that the backend should call
 * whenever a CAN frame is received that matches the filters configured in the backend's context. The backend should call this callback
 * from the appropriate context, which may be an interrupt context in embedded systems.
 * @param close Function pointer for closing the backend.
 * The backend should handle any necessary cleanup and resource deallocation. This function will be called
 * when the backend is no longer needed, and the context should be considered invalid after this call.
 * After this function is called, the backend should not call any callbacks or attempt to send or receive any more messages.
 * @param get_ms Function pointer to a function that returns the current time in milliseconds.
 * The backend can use this function for any timing-related needs, such as implementing timeouts for sending messages or for timestamping received messages. This function is provided by the user of the library to allow the backend to access a time source without depending on any specific platform's timing APIs.
 * @param context Pointer to context data.
 *
 */
typedef struct {
    artie_can_error_t (*init)(artie_can_context_t *ctx);
    artie_can_error_t (*send)(artie_can_context_t *ctx, const artie_can_frame_t *frame);
    artie_can_rx_callback_t *receive_callback;
    artie_can_error_t (*close)(artie_can_context_t *ctx);
    artie_can_get_ms_t *get_ms;
    artie_can_context_t *context;
} artie_can_backend_t;

/**
 * @brief Enumeration for the different types of CAN backends directly supported by the library.
 *
 */
typedef enum {
    ARTIE_CAN_BACKEND_MCP2515,
    ARTIE_CAN_BACKEND_TCP,
} artie_can_backend_type_t;
