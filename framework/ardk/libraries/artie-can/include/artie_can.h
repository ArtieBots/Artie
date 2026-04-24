/**
 * @file artie_can.h
 * @brief Public header file for Artie CAN library (C implementation).
 * Mostly just a bunch of includes for the various components of the library, and a few public definitions.
 *
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "backend.h"
#include "backend_mcp2515.h"
#include "backend_tcp.h"
#include "context.h"
#include "err.h"
#include "frame.h"
#include "rtacp.h"

/**
 * @brief Initialize the library with the given backend type and configuration.
 * Either this function or artie_can_init_custom() must be called before using any other functions in the library.
 *
 * @param context Pointer to context data.
 * We copy the pointer into the backend handle struct, so the caller must ensure the context data's lifetime
 * matches the lifetime of the backend handle.
 * @param handle Pointer to an artie_can_backend_t struct that will be populated with the function pointers
 * and context for the initialized backend. This handle can then be used with the other functions in the library.
 * @param backend_type The type of backend to initialize.
 * @param rx_callback Callback function that will be called when a CAN frame is received. Any time a CAN frame
 * is received by the backend, so long as it matches one of the filters configured in the backend's context,
 * this callback will be called with the received frame. The frame will be raw. In an embedded system,
 * this callback will probably be called from an interrupt context, so it should be designed with that in mind.
 * @param get_ms_fn Function pointer to a function that returns the current time in milliseconds.

 * @return ARTIE_CAN_ERR_NONE on success, or an appropriate error code on failure.
 *
 */
artie_can_error_t artie_can_init(artie_can_context_t *context, artie_can_backend_t *handle, artie_can_backend_type_t backend_type, artie_can_rx_callback_t rx_callback, artie_can_get_ms_t get_ms_fn);

/**
 * @brief Initialize the library with a custom backend.
 * This can be used to initialize the library with a backend that is not one of the built-in types, or to provide
 * a custom implementation of a built-in type.
 * Either this function or artie_can_init() must be called before using any other functions in the library.
 *
 * Simply calls the init function pointer in the provided handle with the provided context.
 *
 * @param context Pointer to context data.
 * We copy the pointer into the backend handle struct, so the caller must ensure the context data's lifetime
 * matches the lifetime of the backend handle.
 * @param handle Pointer to an artie_can_backend_t struct that will be populated with the function pointers
 * and context for the initialized backend. This handle can then be used with the other functions in the library.
 * @param rx_callback Callback function that will be called when a CAN frame is received. Any time a CAN frame
 * is received by the backend, so long as it matches one of the filters configured in the backend's context,
 * this callback will be called with the received frame. The frame will be raw. In an embedded system,
 * this callback will probably be called from an interrupt context, so it should be designed with that in mind.
 * @param get_ms_fn Function pointer to a function that returns the current time in milliseconds.
 *
 * @return The return value of the backend's init function, which should be ARTIE_CAN_ERR_NONE
 * on success or an appropriate error code on failure.
 *
 */
artie_can_error_t artie_can_init_custom(artie_can_context_t *context, artie_can_backend_t *handle, artie_can_rx_callback_t rx_callback, artie_can_get_ms_t get_ms_fn);

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
 * @brief Run the event loop for the specified backend. This should be called periodically to allow the backend to process
 * any pending events, such as handling received frames or timeouts. The latency of
 * the Artie CAN Library depends on how often this function is called, so it should be called as frequently as possible
 * for best performance.
 *
 * In an environment running an OS, you can use the convenience function artie_can_start_event_loop() to start
 * a dedicated thread that runs this event loop for you.
 *
 * @param handle Pointer to the artie_can_backend_t struct representing the backend to run the event loop for.
 * @return Error code indicating the result of the operation. If the event loop ran successfully,
 * returns ARTIE_CAN_ERR_NONE. If there was an error, returns an appropriate error code.
 */
artie_can_error_t artie_can_tick(artie_can_backend_t *handle);

/**
 * @brief Start the event loop for the specified backend in a separate thread.
 * This function creates a new thread to run the event loop, allowing the backend to process
 * events concurrently with the main program. The event loop will continue running until
 * artie_can_close() is called on the backend.
 *
 * Important! This function is not supported in an embedded context. It is only intended for use
 * in a full-blown OS environment. In an embedded context, you should call artie_can_tick() periodically
 * from your main loop, a timer interrupt, or an RTOS thread to allow the backend to process events.
 *
 * @param handle Pointer to the artie_can_backend_t struct representing the backend to start the event loop for.
 * @param tick_interval_us The interval in microseconds at which to call artie_can_tick() in the event loop thread.
 * This determines how often the backend processes events, so a lower value will result in lower latency but
 * higher CPU usage. A reasonable default might be 100 (0.1 ms). Must be greater than 0.
 * @return Error code indicating the result of the operation. If the event loop was successfully started,
 * returns ARTIE_CAN_ERR_NONE. If there was an error, returns an appropriate error code.
 */
artie_can_error_t artie_can_start_event_loop(artie_can_backend_t *handle, uint32_t tick_interval_us);
