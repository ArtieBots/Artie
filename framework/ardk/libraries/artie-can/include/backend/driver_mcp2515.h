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
 * @brief Reset the MCP2515 device by means of the SPI interface. Does not require
 * reinitialization of the driver.
 *
 * @param context The Artie CAN context struct that is being used with this driver.
 * @return artie_can_error_t Error code indicating the result of the reset attempt.
 */
artie_can_error_t driver_mcp2515_reset(artie_can_context_t *context);

/**
 * @brief The function we call when the microcontroller receives an interrupt from the MCP2515.
 *
 * @param context The Artie CAN context struct that is being used with this driver.
 * @return artie_can_error_t Error code indicating the result of processing.
 * If this function is called from within a greater ISR, this may be useful
 * to the MCU. If this function is called directly from the hardware as the ISR,
 * then this error code will not be returned to any caller.
 */
artie_can_error_t driver_mcp2515_isr(artie_can_context_t *context);
