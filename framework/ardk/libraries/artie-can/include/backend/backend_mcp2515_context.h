/**
 * @file backend_mcp2515_context.h
 * @brief Definitions for context struct and related functions for the Artie CAN MCP2515 backend.
 *
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/** The number of bytes (registers) we have to read for a full frame. */
#define BYTES_IN_MCP1515_CAN_FRAME 13

/** Modes the MCP2515 can be in. */
typedef enum {
    MCP2515_MODE_NORMAL = 0,
    MCP2515_MODE_SLEEP = 1,
    MCP2515_MODE_LOOPBACK = 2,
    MCP2515_MODE_LISTEN_ONLY = 3,
    MCP2515_MODE_CONFIGURATION = 4,
} mcp2515_mode_t;

/** Configuration struct for the MCP2515. */
typedef struct {
    bool bfp0_int_enabled;          ///< Whether the Buffer Full Pin 0 should be configured as an interrupt pin.
    bool bfp1_int_enabled;          ///< Whether the Buffer Full Pin 1 should be configured as an interrupt pin.
    mcp2515_mode_t mode;            ///< The mode to configure the MCP2515 in.
    uint32_t oscillator_freq_hz;    ///< The frequency of the oscillator connected to the MCP2515.
} driver_mcp2515_config_t;

/** Function pointer type for writing a byte to the MCP2515 over SPI. Should also fill the read_byte in the context. */
typedef artie_can_error_t (*artie_can_write_byte_t)(artie_can_context_t *context, uint8_t byte);

/** Function pointer type for controlling the MCP2515's CS pin. Should set the pin low when selecting the device and high when deselecting. If hardware CS is used, this function can simply be empty. */
typedef artie_can_error_t (*artie_can_write_cs_pin_t)(artie_can_context_t *context, bool cs_low);

/**
 * @brief Structure representing the context object for the Artie CAN MCP2515 backend.
 *
 */
typedef struct {
    // Function pointers for interacting with the MCP2515 hardware.
    // These should be provided by the user of the backend to allow for hardware abstraction.
    artie_can_write_byte_t write_byte;          ///< Function pointer for writing a byte to the MCP2515 over SPI.
    artie_can_write_cs_pin_t write_cs_pin;      ///< Function pointer for controlling the MCP2515's CS pin.

    uint8_t read_byte;                          ///< The most recently read byte from SPI, updated by the write_byte function.
    uint8_t rx_buffer[BYTES_IN_MCP1515_CAN_FRAME]; ///< Buffer for storing the bytes read from the MCP2515 when receiving a frame, which will then be parsed and fed up to the appropriate protocol handler.

    driver_mcp2515_config_t mcp2515_config; ///< Configuration struct for the MCP2515.
} artie_can_mcp2515_context_t;
