/**
 * @file artie_can.h
 * @brief Public header file for Artie CAN library (C implementation).
 * Mostly just a bunch of includes for the various components of the library, and a few public definitions.
 * @date 2026-04-12
 *
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "err.h"

#include "backend.h"
#include "backend_mcp2515.h"
#include "backend_tcp.h"
#include "frame.h"
#include "rtacp.h"

/**
 * @brief Initialize the library with the given backend type and configuration.
 * Either this function or artie_can_init_custom() must be called before using any other functions in the library.
 *
 * @param context Pointer to backend-specific context data. The expected structure of this data depends on the backend
 * type being initialized.
 * For ARTIE_CAN_BACKEND_MCP2515, this should be a pointer to an artie_can_mcp2515_context_t struct that
 * has been initialized with artie_can_initialize_context_mcp2515() with the desired configuration for the MCP2515 backend.
 * For ARTIE_CAN_BACKEND_TCP, this should be a pointer to an artie_can_backend_tcp_context_t struct that
 * has been initialized with artie_can_initialize_context_tcp() with the desired configuration for the TCP backend.
 * We copy the pointer into the backend handle struct, so the caller must ensure the context data's lifetime
 * matches the lifetime of the backend handle.
 * @param handle Pointer to an artie_can_backend_t struct that will be populated with the function pointers
 * and context for the initialized backend. This handle can then be used with the other functions in the library.
 * @return ARTIE_CAN_ERR_NONE on success, or an appropriate error code on failure.
 *
 */
artie_can_error_t artie_can_init(void *context, artie_can_backend_t *handle, artie_can_backend_type_t backend_type);

/**
 * @brief Initialize the library with a custom backend.
 * This can be used to initialize the library with a backend that is not one of the built-in types, or to provide
 * a custom implementation of a built-in type.
 * Either this function or artie_can_init() must be called before using any other functions in the library.
 *
 * Simply calls the init function pointer in the provided handle with the provided context.
 *
 * @param handle Pointer to an artie_can_backend_t struct that has been initialized with the function pointers and context
 * for the desired backend implementation.
 * @return The return value of the backend's init function, which should be ARTIE_CAN_ERR_NONE
 * on success or an appropriate error code on failure.
 *
 */
artie_can_error_t artie_can_init_custom(artie_can_backend_t *handle);

/**
 * @brief Close the backend and free any associated resources.
 *
 * @param handle Pointer to the artie_can_backend_t struct representing the backend to close.
 * @return Error code indicating the result of the operation. If the backend was successfully closed,
 * returns ARTIE_CAN_ERR_NONE, in which case the handle has been zeroed out and can be safely
 * reused or freed by the caller. If the backend was already closed, returns ARTIE_CAN_ERR_CLOSED.
 */
artie_can_error_t artie_can_close(artie_can_backend_t *handle);

/**
 * @brief Send the given CAN frame using the specified backend.
 *
 * This function assumes you already have a prepared frame. To prepare the frame for your backend
 * and protocol, use the appropriate frame initialization function for your protocol.
 * For example, if you are using RTACP, you can use artie_can_rtacp_init_frame()
 * to prepare your frame before sending it with this function.
 *
 * @param handle Pointer to the artie_can_backend_t struct representing the backend to use for sending the frame.
 * @param frame Pointer to the artie_can_frame_t struct representing the frame to send.
 * @return Error code indicating the result of the operation. If the frame was successfully sent,
 * returns ARTIE_CAN_ERR_NONE. If there was an error sending the frame, returns an appropriate error code.
 */
artie_can_error_t artie_can_send(artie_can_backend_t *handle, const artie_can_frame_t *frame);

/**
 * @brief Receive a CAN frame using the specified backend.
 * If timeout_ms is non-zero, this function will block up to that many milliseconds for a frame to be received before
 * returning with an ARTIE_CAN_ERROR_TIMEOUT. If timeout_ms is zero, this function will block forever until
 * a frame is received.
 *
 * This function's frame output is raw. To deconstruct it into useful data according to your protocol,
 * use the appropriate frame parsing functions. For example, if you are using RTACP,
 * you can use artie_can_rtacp_parse_frame() to get a more useful representation of the received data.
 *
 * @param handle Pointer to the artie_can_backend_t struct representing the backend to use for receiving the frame.
 * @param frame Pointer to the artie_can_frame_t struct where the received frame will be stored.
 * @param timeout_ms Maximum time to wait for a frame in milliseconds. If zero, wait indefinitely.
 * @return Error code indicating the result of the operation. If a frame was successfully received,
 * returns ARTIE_CAN_ERR_NONE. If there was a timeout, returns ARTIE_CAN_ERROR_TIMEOUT. If there was another error,
 * returns an appropriate error code.
 */
artie_can_error_t artie_can_receive(artie_can_backend_t *handle, artie_can_frame_t *frame, uint32_t timeout_ms);

/**
 * @brief Attempt to receive a CAN frame without blocking.
 *
 * This function attempts to receive a CAN frame without blocking. Unless there is an error,
 * this function returns immediately with ARTIE_CAN_ERR_NONE. If timeout_ms is zero, the backend will asynchronously attempt
 * to receive a frame forever. If timeout_ms is non-zero, the backend will attempt to receive a frame for up to that many
 * milliseconds before calling the callback with a timeout error as its second argument. If a frame is received,
 * the backend will call the callback with ARTIE_CAN_ERR_NONE as its second argument and the received frame as its third argument.
 * If an error occurs, the backend will call the callback with the appropriate error code as its second argument
 * and NULL as its third argument. Unless the backend has been closed, the first argument should always be a valid
 * pointer to the backend context. If the backend has been closed by the time the callback is called, the first argument should be NULL,
 * and the error should be ARTIE_CAN_ERR_CLOSED.
 *
 * This function's frame output is raw. To deconstruct it into useful data according to your protocol,
 * use the appropriate frame parsing functions. For example, if you are using RTACP,
 * you can use artie_can_rtacp_parse_frame() to get a more useful representation of the received data.
 *
 * @param handle Pointer to the artie_can_backend_t struct representing the backend to use for receiving the frame.
 * @param frame Pointer to the artie_can_frame_t struct where the received frame will be stored once it has been received.
 * Until the callback is called, this frame should be considered uninitialized and should not be accessed by the caller
 * (the backend owns this memory until the callback is called).
 * @param timeout_ms Maximum time to attempt to receive a frame in milliseconds. If zero, attempt to receive indefinitely until a frame is received or an error occurs.
 * @param callback Callback function that the backend will call when a frame is received, an error occurs, or the backend is closed. The callback will be called with a pointer to the backend context as its first argument (or NULL if the backend is closed), an artie_can_error_t indicating the result of the receive attempt as its second argument, and a pointer to the received frame as its third argument (or NULL if no frame was received).
 * @return Error code indicating the result of the operation. If the receive attempt was successfully initiated, returns ARTIE_CAN_ERR_NONE. If there was an error initiating the receive attempt, returns an appropriate error code.
 */
artie_can_error_t artie_can_receive_nonblocking(artie_can_backend_t *handle, artie_can_frame_t *frame, uint32_t timeout_ms, artie_can_receive_callback_t callback);
