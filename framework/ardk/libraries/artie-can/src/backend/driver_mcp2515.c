#include <stdbool.h>
#include <stdint.h>
#include "backend.h"
#include "backend_mcp2515_context.h"
#include "driver_mcp2515.h"
#include "context.h"
#include "err.h"

/** Modes the MCP2515 can be in. */
typedef enum {
    MCP2515_MODE_NORMAL = 0,
    MCP2515_MODE_SLEEP = 1,
    MCP2515_MODE_LOOPBACK = 2,
    MCP2515_MODE_LISTEN_ONLY = 3,
    MCP2515_MODE_CONFIGURATION = 4,
} mcp2515_mode_t;

/** The available SPI instructions. Instructions are one byte each. See Table 12.1 in the datasheet. */
typedef enum {
    MCP2515_INSTRUCTION_RESET = 0xC0,
    MCP2515_INSTRUCTION_READ = 0x03,
    MCP2515_INSTRUCTION_READ_RX_BUFFER = 0x90,
    MCP2515_INSTRUCTION_WRITE = 0x02,
    MCP2515_INSTRUCTION_LOAD_TX_BUFFER = 0x40,
    MCP2515_INSTRUCTION_RTS = 0x80,
    MCP2515_INSTRUCTION_READ_STATUS = 0xA0,
    MCP2515_INSTRUCTION_RX_STATUS = 0xB0,
    MCP2515_INSTRUCTION_BIT_MODIFY = 0x05,
} mcp2515_instruction_t;

/** Write a RESET instruction to the device. See Section 12.2 of the datasheet. */
static artie_can_error_t _reset_instruction(artie_can_context_t *context)
{
    // Cast context
    artie_can_mcp2515_context_t *mcp2515_ctx = (artie_can_mcp2515_context_t *)(context->backend_context);

    // Pull CS low to select the device
    artie_can_error_t err;
    err = mcp2515_ctx->write_cs_pin(false);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    // Send the RESET instruction byte over SPI
    err = mcp2515_ctx->write_byte(MCP2515_INSTRUCTION_RESET);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        // Pull CS high to deselect the device before returning
        mcp2515_ctx->write_cs_pin(true);
        return err;
    }

    // Pull CS high to deselect the device
    err = mcp2515_ctx->write_cs_pin(true);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    return ARTIE_CAN_ERR_NONE;
}

/** Write a READ instruction to the device. See Section 12.3 of the datasheet. */
static artie_can_error_t _read_instruction(artie_can_context_t *context, uint8_t start_addr, uint8_t *bytes_to_read, size_t nbytes)
{
    // Cast context
    artie_can_mcp2515_context_t *mcp2515_ctx = (artie_can_mcp2515_context_t *)(context->backend_context);

    // Pull CS low to select the device
    artie_can_error_t err;
    err = mcp2515_ctx->write_cs_pin(false);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    // Send the READ instruction byte over SPI
    err = mcp2515_ctx->write_byte(MCP2515_INSTRUCTION_READ);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        // Pull CS high to deselect the device before returning
        mcp2515_ctx->write_cs_pin(true);
        return err;
    }

    // For each byte we want to read, read a byte from SPI and store it in the provided buffer
    for (size_t i = 0; i < nbytes; i++)
    {
        // Send a dummy byte to clock the SPI bus
        err = mcp2515_ctx->write_byte(0x00);
        if (err != ARTIE_CAN_ERR_NONE)
        {
            // Pull CS high to deselect the device before returning
            mcp2515_ctx->write_cs_pin(true);
            return err;
        }

        // The byte read from SPI should have been inserted into the context pointer
        // by the write_byte function, so we can just read it from there and put it in the output buffer.
        bytes_to_read[i] = *(mcp2515_ctx->read_byte);
    }

    // Pull CS high to deselect the device
    err = mcp2515_ctx->write_cs_pin(true);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    return ARTIE_CAN_ERR_NONE;
}

/** buffer index is either 0 or 1. start_at_id true means start reading from the SIDH portion, otherwise start reading at D0. After this instruction, the RXnIF is cleared. */
static artie_can_error_t _read_rx_buffer_instruction(artie_can_context_t *context, uint8_t buffer_index, bool start_at_id, uint8_t *bytes_to_read, size_t nbytes)
{
    // Cast context
    artie_can_mcp2515_context_t *mcp2515_ctx = (artie_can_mcp2515_context_t *)(context->backend_context);

    // Pull CS low to select the device
    artie_can_error_t err;
    err = mcp2515_ctx->write_cs_pin(false);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    // Send the READ RX BUFFER instruction byte over SPI
    uint8_t instruction_byte = MCP2515_INSTRUCTION_READ_RX_BUFFER | (buffer_index << 2) | ((uint8_t)start_at_id << 1);
    err = mcp2515_ctx->write_byte(instruction_byte);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        // Pull CS high to deselect the device before returning
        mcp2515_ctx->write_cs_pin(true);
        return err;
    }

    // For each byte we want to read, read a byte from SPI and store it in the provided buffer
    for (size_t i = 0; i < nbytes; i++)
    {
        // Send a dummy byte to clock the SPI bus
        err = mcp2515_ctx->write_byte(0x00);
        if (err != ARTIE_CAN_ERR_NONE)
        {
            // Pull CS high to deselect the device before returning
            mcp2515_ctx->write_cs_pin(true);
            return err;
        }

        // The byte read from SPI should have been inserted into the context pointer
        // by the write_byte function, so we can just read it from there and put it in the output buffer.
        bytes_to_read[i] = *(mcp2515_ctx->read_byte);
    }

    // Pull CS high to deselect the device (this also clears the RXnIF flag in the device)
    err = mcp2515_ctx->write_cs_pin(true);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    return ARTIE_CAN_ERR_NONE;
}

static artie_can_error_t _write_instruction(artie_can_context_t *context, uint8_t start_addr, const uint8_t *bytes_to_write, size_t nbytes)
{
    // Cast context
    artie_can_mcp2515_context_t *mcp2515_ctx = (artie_can_mcp2515_context_t *)(context->backend_context);

    // Pull CS low to select the device
    artie_can_error_t err;
    err = mcp2515_ctx->write_cs_pin(false);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    // Send the WRITE instruction byte over SPI
    err = mcp2515_ctx->write_byte(MCP2515_INSTRUCTION_WRITE);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        // Pull CS high to deselect the device before returning
        mcp2515_ctx->write_cs_pin(true);
        return err;
    }

    // Send the starting address byte over SPI
    err = mcp2515_ctx->write_byte(start_addr);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        // Pull CS high to deselect the device before returning
        mcp2515_ctx->write_cs_pin(true);
        return err;
    }

    // For each byte we want to write, write the byte to SPI
    for (size_t i = 0; i < nbytes; i++)
    {
        err = mcp2515_ctx->write_byte(bytes_to_write[i]);
        if (err != ARTIE_CAN_ERR_NONE)
        {
            // Pull CS high to deselect the device before returning
            // All bytes except this one will likely have been written.
            mcp2515_ctx->write_cs_pin(true);
            return err;
        }
    }

    // Pull CS high to deselect the device
    err = mcp2515_ctx->write_cs_pin(true);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    return ARTIE_CAN_ERR_NONE;
}

/** buffer_index is either 0, 1, or 2. start_at_id = true means start at SIDH otherwise D0. */
static artie_can_error_t _load_tx_buffer_instruction(artie_can_context_t *context, uint8_t buffer_index, bool start_at_id, const uint8_t *bytes_to_write, size_t nbytes)
{
    // Cast context
    artie_can_mcp2515_context_t *mcp2515_ctx = (artie_can_mcp2515_context_t *)(context->backend_context);

    // Pull CS low to select the device
    artie_can_error_t err;
    err = mcp2515_ctx->write_cs_pin(false);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    // Send the LOAD TX BUFFER instruction byte over SPI
    uint8_t instruction_byte = MCP2515_INSTRUCTION_LOAD_TX_BUFFER | (buffer_index << 2) | ((uint8_t)start_at_id << 1);
    err = mcp2515_ctx->write_byte(instruction_byte);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        // Pull CS high to deselect the device before returning
        mcp2515_ctx->write_cs_pin(true);
        return err;
    }

    // For each byte we want to write, write the byte to SPI
    for (size_t i = 0; i < nbytes; i++)
    {
        err = mcp2515_ctx->write_byte(bytes_to_write[i]);
        if (err != ARTIE_CAN_ERR_NONE)
        {
            // Pull CS high to deselect the device before returning
            // All bytes except this one will likely have been written.
            mcp2515_ctx->write_cs_pin(true);
            return err;
        }
    }

    // Pull CS high to deselect the device
    err = mcp2515_ctx->write_cs_pin(true);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    return ARTIE_CAN_ERR_NONE;
}

// TODO: Implement remaining instructions

static artie_can_error_t _set_mode(artie_can_context_t *context, mcp2515_mode_t mode)
{
    // Check if the device is already in the requested mode
    mcp2515_mode_t current_mode;
    artie_can_error_t err = _read_register(context,
    // TODO
    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t driver_mcp2515_init(artie_can_context_t *context)
{
    // TODO

    // Reset the device to ensure it's in a known state
}

artie_can_error_t driver_mcp2515_config(artie_can_context_t *context, driver_mcp2515_config_t *config)
{
    // Set device to configuration mode
    _set_mode(context, MCP2515_MODE_CONFIGURATION);

    // Set up CAN baudrate

    // Initialize the filter bits based on node address and protocol flags in the context

    // Configure interrupts

    // Switch to normal mode (TODO: Think about sleep mode)
}

artie_can_error_t driver_mcp2515_deinit(artie_can_context_t *context)
{
    // TODO
}

artie_can_error_t driver_mcp2515_send(artie_can_context_t *context, const artie_can_frame_t *frame)
{
    // TODO
}

artie_can_error_t driver_mcp2515_receive(artie_can_context_t *context, artie_can_frame_t *frame)
{
    // TODO
}

artie_can_error_t driver_mcp2515_reset(artie_can_context_t *context)
{
    // TODO
}
