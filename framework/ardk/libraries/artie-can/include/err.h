/**
 * @file err.h
 * @brief Error handling and codes for Artie CAN library.
 *
 */

#pragma once

typedef enum {
    ARTIE_CAN_ERR_NONE = 0,           /**< No error */
    ARTIE_CAN_ERR_INVALID_ARG = -1,   /**< Invalid argument provided */
    ARTIE_CAN_ERR_TIMEOUT = -2,       /**< Operation timed out */
    ARTIE_CAN_ERR_NO_DATA = -3,       /**< No data available to receive */
    ARTIE_CAN_ERR_SEND_FAIL = -4,     /**< Failed to send frame */
    ARTIE_CAN_ERR_RECEIVE_FAIL = -5,  /**< Failed to receive frame */
    ARTIE_CAN_ERR_INIT_FAIL = -6,     /**< Failed to initialize backend */
    ARTIE_CAN_ERR_CLOSE_FAIL = -7,    /**< Failed to close backend */
    ARTIE_CAN_ERR_CLOSED = -8,        /**< Backend is closed */
    ARTIE_CAN_ERR_SEND_BUSY = -9,     /**< Bus is busy, cannot send frame */
} artie_can_error_t;
