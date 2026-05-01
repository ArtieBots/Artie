/**
 * @file err.h
 * @brief Error handling and codes for Artie CAN library.
 *
 */

#pragma once

/**
 * @brief Enumeration of error codes for Artie CAN library.
 * Positive values indicate a failure, while ARTIE_CAN_ERR_NONE (0) indicates success.
 * These error codes can be viewed as bit flags within a uint32_t,
 * so they can be combined together if multiple errors occur at once,
 * though typically only one error code will be returned at a time.
 *
 */
typedef enum {
    ARTIE_CAN_ERR_NONE = 0,                 /**< No error */
    ARTIE_CAN_ERR_INVALID_ARG = (1 << 0),   /**< Invalid argument provided */
    ARTIE_CAN_ERR_TIMEOUT = (1 << 1),       /**< Operation timed out */
    ARTIE_CAN_ERR_NO_DATA = (1 << 2),       /**< No data available to receive */
    ARTIE_CAN_ERR_SEND_FAIL = (1 << 3),     /**< Failed to send frame */
    ARTIE_CAN_ERR_RECEIVE_FAIL = (1 << 4),  /**< Failed to receive frame */
    ARTIE_CAN_ERR_INIT_FAIL = (1 << 5),     /**< Failed to initialize backend */
    ARTIE_CAN_ERR_CLOSE_FAIL = (1 << 6),    /**< Failed to close backend */
    ARTIE_CAN_ERR_CLOSED = (1 << 7),        /**< Backend is closed */
    ARTIE_CAN_ERR_SEND_BUSY = (1 << 8),     /**< Bus is busy, cannot send frame */
    ARTIE_CAN_ERR_NO_SPACE = (1 << 9),      /**< Cannot write to a buffer because there is no more space in it */
    ARTIE_CAN_ERR_INTERNAL = (1 << 10),     /**< An internal error occurred. This is a catch-all for errors that don't fit into the other categories, and typically indicates a bug in the library. */
    ARTIE_CAN_ERR_DRIVER = (1 << 11),       /**< An error occurred in the backend driver. This indicates an error in the communication with underlying hardware. */
} artie_can_error_t;
