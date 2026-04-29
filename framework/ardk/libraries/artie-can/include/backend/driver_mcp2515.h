/**
 * @file driver_mcp2515.h
 * @brief Header file for Artie CAN MCP2515 driver.
 *
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "backend.h"
#include "context.h"
#include "err.h"

/** Configuration struct for the MCP2515. */
typedef struct {
    uint32_t baudrate;      ///< The CAN bus baudrate to use in bits per second. Maximum is 125 kHz.
    uint32_t filtermask;    ///< The filter/mask configuration for the MCP2515. Only the lower 29 bits are used and each 1 in the filtermask means that the corresponding bit in the CAN ID will be used for filtering incoming messages.
    uint32_t filter;        ///< The filter value for the MCP2515. Only the lower 29 bits are used and this value is compared against the CAN ID of incoming messages according to the filtermask to determine whether to accept or reject the message.
} driver_mcp2515_config_t;

/**
 * @brief Initialize the MCP2515 driver and populate the provided context with the necessary information for it to operate.
 * Must be done as the first step before using any of this drivers other functions.
 *
 * @param context The Artie CAN context struct that will be used with this driver.
 * @return artie_can_error_t Error code indicating the result of the initialization.
 */
artie_can_error_t driver_mcp2515_init(artie_can_context_t *context);

/**
 * @brief Configure the MCP2515 device with the provided parameters.
 *
 * @param context The Artie CAN context struct that is being used with this driver.
 * @param config The configuration parameters for the MCP2515 device.
 * @return artie_can_error_t An error code indicating the result of the configuration attempt.
 */
artie_can_error_t driver_mcp2515_config(artie_can_context_t *context, driver_mcp2515_config_t *config);

/**
 * @brief Deinitialize the MCP2515 driver and free any resources it is using.
 * After this call, the context should not be used with this driver again without reinitialization.
 *
 * @param context The Artie CAN context struct that is being used with this driver.
 * @return artie_can_error_t Error code indicating the result of the deinitialization attempt.
 */
artie_can_error_t driver_mcp2515_deinit(artie_can_context_t *context);

/**
 * @brief Send the given CAN frame using the MCP2515 device.
 *
 * @param context The Artie CAN context struct that is being used with this driver.
 * @param frame The CAN frame to send.
 * @return artie_can_error_t Error code indicating the result of the send attempt.
 */
artie_can_error_t driver_mcp2515_send(artie_can_context_t *context, const artie_can_frame_t *frame);

/**
 * @brief Read a pending CAN frame from the MCP2515 device. Returns an error if there is no pending frame to read.
 * If both receive buffers are full, RXB0 has higher priority and will be the one returned by this function.
 *
 * @param context The Artie CAN context struct that is being used with this driver.
 * @param frame Pointer to a location where the received CAN frame should be stored if the receive is successful.
 * @return artie_can_error_t Error code indicating the result of the receive attempt. If successful, the received frame will be stored in the location pointed to by the frame parameter.
 */
artie_can_error_t driver_mcp2515_receive(artie_can_context_t *context, artie_can_frame_t *frame);

/**
 * @brief Reset the MCP2515 device by means of the SPI interface. Does not require
 * reinitialization of the driver.
 *
 * @param context The Artie CAN context struct that is being used with this driver.
 * @return artie_can_error_t Error code indicating the result of the reset attempt.
 */
artie_can_error_t driver_mcp2515_reset(artie_can_context_t *context);
