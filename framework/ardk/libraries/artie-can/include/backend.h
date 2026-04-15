/**
 * @file artie_can.h
 * @brief Public header file for Artie CAN library (C implementation).
 * @date 2026-04-12
 *
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "err.h"
#include "frame.h"

/**
 * @brief A typedef representing a callback for the non-blocking receive function in the CAN backend.
 * (See artie_can_backend_t.receive_nonblocking for more details on how this callback is used).
 */
typedef void (*artie_can_receive_callback_t)(void *ctx, artie_can_error_t error, artie_can_frame_t *frame);

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
 * continue trying to send the previous message.
 * @param receive Function pointer for receiving a CAN frame.
 * The backend should handle the actual reception of the CAN frame. The frame data will be populated by the backend.
 * If timeout_ms is non-zero, the backend should block up to that many milliseconds for a frame to be received before
 * returning with an ARTIE_CAN_ERROR_TIMEOUT. If timeout_ms is zero, the backend should block indefinitely until a frame
 * is received.
 * @param receive_nonblocking Function pointer for non-blocking reception of a CAN frame.
 * This function attempts to receive a CAN frame without blocking. Unless there is an error,
 * this function returns immediately with ARTIE_CAN_ERR_NONE. Later, when a frame is received,
 * the backend will call the provided callback with ARTIE_CAN_ERR_NONE as the error argument and the received frame as its third argument.
 * If an error occurs, the backend will call the callback with the appropriate error code as its second argument and
 * NULL as its third argument.
 * In embeded systems, the callback will typically be called from an interrupt context.
 * @param close Function pointer for closing the backend.
 * The backend should handle any necessary cleanup and resource deallocation. This function will be called
 * when the backend is no longer needed, and the context should be considered invalid after this call.
 * After this function is called, the backend should not call any callbacks or attempt to send or receive any more messages.
 * @param context Pointer to backend-specific context data.
 *
 */
typedef struct {
    artie_can_error_t (*init)(void *ctx);
    artie_can_error_t (*send)(void *ctx, const artie_can_frame_t *frame);
    artie_can_error_t (*receive)(void *ctx, artie_can_frame_t *frame, uint32_t timeout_ms);
    artie_can_error_t (*receive_nonblocking)(void *ctx, artie_can_frame_t *frame, artie_can_receive_callback_t callback);
    artie_can_error_t (*close)(void *ctx);
    void *context;
} artie_can_backend_t;

/**
 * @brief Enumeration for the different types of CAN backends directly supported by the library.
 *
 */
typedef enum {
    ARTIE_CAN_BACKEND_MCP2515,
    ARTIE_CAN_BACKEND_TCP,
} artie_can_backend_type_t;
