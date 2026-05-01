#include <stdbool.h>
#include <stdint.h>
#include "backend.h"
#include "backend_mcp2515_context.h"
#include "driver_mcp2515.h"
#include "context.h"
#include "err.h"

/** Painstakingly not AI-generated register addresses copied from the datasheet. */
typedef enum {
    MCP2515_REG_RXF0SIDH  = 0x00, MCP2515_REG_RXF3SIDH = 0x10, MCP2515_REG_RXM0SIDH = 0x20, MCP2515_REG_TXB0CTRL = 0x30, MCP2515_REG_TXB1CTRL = 0x40, MCP2515_REG_TXB2CTRL = 0x50, MCP2515_REG_RXB0CTRL = 0x60, MCP2515_REG_RXB1CTRL = 0x70,
    MCP2515_REG_RXF0SIDL  = 0x01, MCP2515_REG_RXF3SIDL = 0x11, MCP2515_REG_RXM0SIDL = 0x21, MCP2515_REG_TXB0SIDH = 0x31, MCP2515_REG_TXB1SIDH = 0x41, MCP2515_REG_TXB2SIDH = 0x51, MCP2515_REG_RXB0SIDH = 0x61, MCP2515_REG_RXB1SIDH = 0x71,
    MCP2515_REG_RXF0EID8  = 0x02, MCP2515_REG_RXF3EID8 = 0x12, MCP2515_REG_RXM0EID8 = 0x22, MCP2515_REG_TXB0SIDL = 0x32, MCP2515_REG_TXB1SIDL = 0x42, MCP2515_REG_TXB2SIDL = 0x52, MCP2515_REG_RXB0SIDL = 0x62, MCP2515_REG_RXB1SIDL = 0x72,
    MCP2515_REG_RXF0EID0  = 0x03, MCP2515_REG_RXF3EID0 = 0x13, MCP2515_REG_RXM0EID0 = 0x23, MCP2515_REG_TXB0EID8 = 0x33, MCP2515_REG_TXB1EID8 = 0x43, MCP2515_REG_TXB2EID8 = 0x53, MCP2515_REG_RXB0EID8 = 0x63, MCP2515_REG_RXB1EID8 = 0x73,
    MCP2515_REG_RXF1SIDH  = 0x04, MCP2515_REG_RXF4SIDH = 0x14, MCP2515_REG_RXM1SIDH = 0x24, MCP2515_REG_TXB0EID0 = 0x34, MCP2515_REG_TXB1EID0 = 0x44, MCP2515_REG_TXB2EID0 = 0x54, MCP2515_REG_RXB0EID0 = 0x64, MCP2515_REG_RXB1EID0 = 0x74,
    MCP2515_REG_RXF1SIDL  = 0x05, MCP2515_REG_RXF4SIDL = 0x15, MCP2515_REG_RXM1SIDL = 0x25, MCP2515_REG_TXB0DLC  = 0x35, MCP2515_REG_TXB1DLC  = 0x45, MCP2515_REG_TXB2DLC  = 0x55, MCP2515_REG_RXB0DLC  = 0x65, MCP2515_REG_RXB1DLC  = 0x75,
    MCP2515_REG_RXF1EID8  = 0x06, MCP2515_REG_RXF4EID8 = 0x16, MCP2515_REG_RXM1EID8 = 0x26, MCP2515_REG_TXB0D0   = 0x36, MCP2515_REG_TXB1D0   = 0x46, MCP2515_REG_TXB2D0   = 0x56, MCP2515_REG_RXB0D0   = 0x66, MCP2515_REG_RXB1D0   = 0x76,
    MCP2515_REG_RXF1EID0  = 0x07, MCP2515_REG_RXF4EID0 = 0x17, MCP2515_REG_RXM1EID0 = 0x27, MCP2515_REG_TXB0D1   = 0x37, MCP2515_REG_TXB1D1   = 0x47, MCP2515_REG_TXB2D1   = 0x57, MCP2515_REG_RXB0D1   = 0x67, MCP2515_REG_RXB1D1   = 0x77,
    MCP2515_REG_RXF2SIDH  = 0x08, MCP2515_REG_RXF5SIDH = 0x18, MCP2515_REG_CNF3     = 0x28, MCP2515_REG_TXB0D2   = 0x38, MCP2515_REG_TXB1D2   = 0x48, MCP2515_REG_TXB2D2   = 0x58, MCP2515_REG_RXB0D2   = 0x68, MCP2515_REG_RXB1D2   = 0x78,
    MCP2515_REG_RXF2SIDL  = 0x09, MCP2515_REG_RXF5SIDL = 0x19, MCP2515_REG_CNF2     = 0x29, MCP2515_REG_TXB0D3   = 0x39, MCP2515_REG_TXB1D3   = 0x49, MCP2515_REG_TXB2D3   = 0x59, MCP2515_REG_RXB0D3   = 0x69, MCP2515_REG_RXB1D3   = 0x79,
    MCP2515_REG_RXF2EID8  = 0x0A, MCP2515_REG_RXF5EID8 = 0x1A, MCP2515_REG_CNF1     = 0x2A, MCP2515_REG_TXB0D4   = 0x3A, MCP2515_REG_TXB1D4   = 0x4A, MCP2515_REG_TXB2D4   = 0x5A, MCP2515_REG_RXB0D4   = 0x6A, MCP2515_REG_RXB1D4   = 0x7A,
    MCP2515_REG_RXF2EID0  = 0x0B, MCP2515_REG_RXF5EID0 = 0x1B, MCP2515_REG_CANINTE  = 0x2B, MCP2515_REG_TXB0D5   = 0x3B, MCP2515_REG_TXB1D5   = 0x4B, MCP2515_REG_TXB2D5   = 0x5B, MCP2515_REG_RXB0D5   = 0x6B, MCP2515_REG_RXB1D5   = 0x7B,
    MCP2515_REG_BFPCTRL   = 0x0C, MCP2515_REG_TEC      = 0x1C, MCP2515_REG_CANINTF  = 0x2C, MCP2515_REG_TXB0D6   = 0x3C, MCP2515_REG_TXB1D6   = 0x4C, MCP2515_REG_TXB2D6   = 0x5C, MCP2515_REG_RXB0D6   = 0x6C, MCP2515_REG_RXB1D6   = 0x7C,
    MCP2515_REG_TXRTSCTRL = 0x0D, MCP2515_REG_REC      = 0x1D, MCP2515_REG_EFLG     = 0x2D, MCP2515_REG_TXB0D7   = 0x3D, MCP2515_REG_TXB1D7   = 0x4D, MCP2515_REG_TXB2D7   = 0x5D, MCP2515_REG_RXB0D7   = 0x6D, MCP2515_REG_RXB1D7   = 0x7D,
    MCP2515_REG_CANSTAT   = 0x0E,
    MCP2515_REG_CANCTRL   = 0x0F,
} mcp2515_register_t;

/** For use with RX STATUS instruction. */
typedef enum {
    FILTER_MATCH_RXF0 = 0,
    FILTER_MATCH_RXF1 = 1,
    FILTER_MATCH_RXF2 = 2,
    FILTER_MATCH_RXF3 = 3,
    FILTER_MATCH_RXF4 = 4,
    FILTER_MATCH_RXF5 = 5,
    FILTER_MATCH_RXF0_ROLLOVER = 6,
    FILTER_MATCH_RXF1_ROLLOVER = 7,
} mcp2515_filter_match_t;

/** For use with RX STATUS instruction. */
typedef enum {
    MESSAGE_TYPE_STANDARD_DATA_FRAME = 0,
    MESSAGE_TYPE_STANDARD_REMOTE_FRAME = 1,
    MESSAGE_TYPE_EXTENDED_DATA_FRAME = 2,
    MESSAGE_TYPE_EXTENDED_REMOTE_FRAME = 3,
} mcp2515_message_type_t;

/** For use with RX STATUS instruction. */
typedef enum {
    RX_BUFFER_NO_RX_MSG = 0,
    RX_BUFFER_0 = 1,
    RX_BUFFER_1 = 2,
    RX_BUFFER_BOTH = 3,
} mcp2515_rx_buffer_t;

/** Status flags returned from the MCTP2515 via the rx status instruction. */
typedef struct {
    uint8_t filter_match;          //< Which filter matched the received message, if any
    uint8_t message_type_received; //< Whether the received message was a standard data frame, an extended data frame, a standard remote frame, or an extended remote frame.
    uint8_t rx_buffer;             //< Which receive buffer (RXB0 or RXB1) has a pending message that can be read.
} mcp2515_rx_status_t;

/** Status flags returned from the MCTP2515 via the read status instruction. */
typedef struct {
    uint8_t rx_buffer0_interrupt_flag; //< RX0IF (CANINTF[0])
    uint8_t rx_buffer1_interrupt_flag; //< RX1IF (CANINTF[1])
    uint8_t tx_buffer0_interrupt_flag; //< TX0IF (CANINTF[2])
    uint8_t tx_buffer1_interrupt_flag; //< TX1IF (CANINTF[3])
    uint8_t tx_buffer2_interrupt_flag; //< TX2IF (CANINTF[4])
    uint8_t tx_buffer2_request_flag;   //< TXREQ (TXB2CNTRL[3])
    uint8_t tx_buffer1_request_flag;   //< TXREQ (TXB1CNTRL[3])
    uint8_t tx_buffer0_request_flag;   //< TXREQ (TXB0CNTRL[3])
} mcp2515_status_t;

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

/** Send a Request-to-Send (RTS) instruction. buffer_mask is a bitmask where bit 0 is TXB0, bit 1 is TXB1, bit 2 is TXB2. See Section 12.7 of the datasheet. */
static artie_can_error_t _rts_instruction(artie_can_context_t *context, uint8_t buffer_mask)
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

    // Send the RTS instruction byte over SPI
    uint8_t instruction_byte = MCP2515_INSTRUCTION_RTS | (buffer_mask & 0x07);
    err = mcp2515_ctx->write_byte(instruction_byte);
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

/** Read the status byte. See Section 12.8 of the datasheet. */
static artie_can_error_t _read_status_instruction(artie_can_context_t *context, mctp_2515_status_t *status)
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

    // Send the READ STATUS instruction byte over SPI
    err = mcp2515_ctx->write_byte(MCP2515_INSTRUCTION_READ_STATUS);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        // Pull CS high to deselect the device before returning
        mcp2515_ctx->write_cs_pin(true);
        return err;
    }

    // Send a dummy byte to clock the SPI bus and read the status byte
    err = mcp2515_ctx->write_byte(0x00);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        // Pull CS high to deselect the device before returning
        mcp2515_ctx->write_cs_pin(true);
        return err;
    }

    // The byte read from SPI should have been inserted into the context pointer
    // by the write_byte function, so we can just read it from there and put it in the output struct.
    uint8_t read_byte = *(mcp2515_ctx->read_byte);
    status->rx_buffer0_interrupt_flag = (read_byte >> 0) & 0x01;
    status->rx_buffer1_interrupt_flag = (read_byte >> 1) & 0x01;
    status->tx_buffer0_request_flag = (read_byte >> 2) & 0x01;
    status->tx_buffer0_interrupt_flag = (read_byte >> 3) & 0x01;
    status->tx_buffer1_request_flag = (read_byte >> 4) & 0x01;
    status->tx_buffer1_interrupt_flag = (read_byte >> 5) & 0x01;
    status->tx_buffer2_request_flag = (read_byte >> 6) & 0x01;
    status->tx_buffer2_interrupt_flag= (read_byte >> 7) & 0x01;

    // Pull CS high to deselect the device
    err = mcp2515_ctx->write_cs_pin(true);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    return ARTIE_CAN_ERR_NONE;
}

/** Read the RX status byte. See Section 12.9 of the datasheet. */
static artie_can_error_t _rx_status_instruction(artie_can_context_t *context, mcp2515_rx_status_t *status)
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

    // Send the RX STATUS instruction byte over SPI
    err = mcp2515_ctx->write_byte(MCP2515_INSTRUCTION_RX_STATUS);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        // Pull CS high to deselect the device before returning
        mcp2515_ctx->write_cs_pin(true);
        return err;
    }

    // Send a dummy byte to clock the SPI bus and read the status byte
    err = mcp2515_ctx->write_byte(0x00);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        // Pull CS high to deselect the device before returning
        mcp2515_ctx->write_cs_pin(true);
        return err;
    }

    // The byte read from SPI should have been inserted into the context pointer
    // by the write_byte function, so we can just read it from there and put it in the output buffer.
    uint8_t read_byte = *(mcp2515_ctx->read_byte);
    status->filter_match = (mcp2515_filter_match_t)(read_byte & 0x07);
    status->message_type_received = (mcp2515_message_type_t)((read_byte & 0x18) >> 3);
    status->rx_buffer = (mcp2515_rx_buffer_t)((read_byte & 0xC0) >> 6);

    // Pull CS high to deselect the device
    err = mcp2515_ctx->write_cs_pin(true);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    return ARTIE_CAN_ERR_NONE;
}

/** Bit modify instruction. See Section 12.10 of the datasheet. Only certain addresses can be used with this instruction. */
static artie_can_error_t _bit_modify_instruction(artie_can_context_t *context, uint8_t addr, uint8_t mask, uint8_t data)
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

    // Send the BIT MODIFY instruction byte over SPI
    err = mcp2515_ctx->write_byte(MCP2515_INSTRUCTION_BIT_MODIFY);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        // Pull CS high to deselect the device before returning
        mcp2515_ctx->write_cs_pin(true);
        return err;
    }

    // Send the address byte over SPI
    err = mcp2515_ctx->write_byte(addr);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        // Pull CS high to deselect the device before returning
        mcp2515_ctx->write_cs_pin(true);
        return err;
    }

    // Send the mask byte over SPI
    err = mcp2515_ctx->write_byte(mask);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        // Pull CS high to deselect the device before returning
        mcp2515_ctx->write_cs_pin(true);
        return err;
    }

    // Send the data byte over SPI
    err = mcp2515_ctx->write_byte(data);
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

static artie_can_error_t _write_byte_and_verify(artie_can_context_t *context, uint8_t addr, uint8_t data, uint8_t mask)
{
    artie_can_error_t err;

    // Write the data to the device
    err = _write_instruction(context, addr, data, 1);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    // Read back the data from the device
    uint8_t read_back_data;
    err = _read_instruction(context, addr, &read_back_data, 1);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    // Verify that the read back data matches what we wrote (after applying the mask)
    if ((read_back_data & mask) != (data & mask))
    {
        return ARTIE_CAN_ERR_DRIVER;
    }

    return ARTIE_CAN_ERR_NONE;
}

static artie_can_error_t _configure_bfpctrl(artie_can_context_t *context, driver_mcp2515_config_t *config)
{
    artie_can_error_t err;

    uint8_t bfpctrl_value = 0x00;
    if (config->bfp0_int_enabled)
    {
        // Set up the Buffer full pin 0 as an interrupt pin
        bfpctrl_value |= (1 << 0);
        bfpctrl_value |= (1 << 2);
    }

    if (config->bfp1_int_enabled)
    {
        // Set up the Buffer full pin 1 as an interrupt pin
        bfpctrl_value |= (1 << 1);
        bfpctrl_value |= (1 << 3);
    }

    err = _write_instruction(context, MCP2515_REG_BFPCTRL, &bfpctrl_value, 1);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    return ARTIE_CAN_ERR_NONE;
}

static artie_can_error_t _configure_canctrl(artie_can_context_t *context, driver_mcp2515_config_t *config)
{
    artie_can_error_t err;

    uint8_t canctrl_value = 0x00;
    canctrl_value |= ((uint8_t)config->mode << 5) & 0xE0; // Set the mode bits (bits 7-5)
    canctrl_value &= ~(1 << 4); // Do not abort any ongoing transmission
    canctrl_value &= ~(1 << 3); // No one-shot mode
    canctrl_value &= ~(1 << 2); // Disable CLKOUT pin
    err = _write_instruction(context, MCP2515_REG_CANCTRL, &canctrl_value, 1);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    return ARTIE_CAN_ERR_NONE;
}

static artie_can_error_t _configure_txrtsctrl(artie_can_context_t *context, driver_mcp2515_config_t *config)
{
    artie_can_error_t err;

    // Set the contents of TXRTSCTRL (we do not make use of the RTS pins, so we'll just disable them)
    uint8_t txrtsctrl_value = 0x00;
    err = _write_instruction(context, MCP2515_REG_TXRTSCTRL, &txrtsctrl_value, 1);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    return ARTIE_CAN_ERR_NONE;
}

static artie_can_error_t _configure_bit_timing(artie_can_context_t *context, driver_mcp2515_config_t *config)
{
    artie_can_error_t err;

    // CNF1 through 3 configure bit rate timing. All nodes on a CAN bus must have the same bit rate,
    // though their clocks are not necessarily the same.
    //
    // Nominal bit rate = f_bit = (1/t_bit)
    // t_bit is the nominal bit time
    // t_bit = t_syncseg + t_propseg + t_ps1 + t_ps2
    //
    // The sync segment, which has a duration of t_syncseg, is used to synchronize the nodes on the bus.
    // The sync segment is fixed at one time quantum (1 Tq).
    //
    // The propagation segment, which has a duration of t_propseg, is used to compensate for physical
    // distances and other reasons for delays between nodes.
    // t_propseg is programmable from 1 to 8 time quanta.
    //
    // Phase segment 1 and phase segment 2 are used to compensate for edge phase errors on the bus.
    // PS1 can be lengthened (PS2 shortened) by resynchronization.
    // t_ps1 is programmable from 1 to 8 time quanta
    // t_ps2 is programmable from 2 to 8 time quanta
    //
    // The time quantum (Tq) is defined as:
    //          Tq = 2 * (BRP[5:0] + 1) / oscillator frequency
    //
    // There is also a synchronization jump width (SJW), which is the resolution of lengthening or shortening
    // of PS1 or PS2 done as a result of resynchronization. We choose a base PS1 and PS2, and an SJW,
    // and the CAN peripheral will attempt to adjust PS1 and/or PS2 by increments of SJW in order to
    // resynchronize to the other nodes on the bus. The SJW also therefore determines the threshold
    // amount of phase difference that would be required for the peripheral to attempt a resync. If the phase
    // difference is off by more than (or equal to) 1 SJW, it attempts to adjust.
    // The SJW is programmable between 1 and 4 time quanta.
    //
    // Note the following restrictions on the programmability of the time values:
    // 1) (t_propseg + t_ps1) >= t_ps2
    // 2) (t_propseg + t_ps1) >= t_delay (where t_delay is typically 1 or 2 time quanta)
    // 3) t_ps2 > SJW
    //
    // Note that oscillators should be chosen on each node to ensure that they do not differ by more than 1.7%
    // Also note that a quartz oscillator is required if a bus rate of more than 125 kHZ is desired.
    //
    // Let's set SJW to 2 to be conservative. That being the case, PS2 must be at least 3 and (t_propseg + t_ps1) must be at least 3 as well.
    // For Artie implementations, we require a CAN bus baudrate of 500 kHz.
    // The frequency of the oscillator is defined in the config struct. For most common MCP2515 breakout boards,
    //     the populated oscillator is 8 MHz.
    // f_bit = 500,000
    // t_bit = 1/500,000 = 2us = t_syncseg + t_propseg + t_ps1 + t_ps2
    // 2us = (1 + t_propseg + t_ps1 + t_ps2)Tq
    // (t_propseg + t_ps1) >= t_ps2
    // 1Tq <= t_ps1 <= 8Tq
    // 2Tq <= t_ps2 <= 8Tq
    //
    // It seems like we are free to choose how many Tq are in our 2us, so let's go with 16.
    //
    // 2us = 16Tq = (1 + t_propseg + t_ps1 + t_ps2)Tq
    // 16 = (1 + t_propseg + t_ps1 + t_ps2)
    //
    // t_propseg should be the amount of delay introduced by wire length and hardware speed,
    // the datasheet uses 2 Tq @ 500 ns per Tq, which is a full microsecond, or 8 Tq in our
    // values.
    // That gives us 8 more Tq to play with.
    // The datasheet says the sampling occurs right after the PS1 segment of t_bit and that
    // this should be at about 60-70% of t_bit. That means 1 + t_propseg + t_ps1 should equal
    // 10. If t_propseg is 8, that means t_ps1 should be 1.
    // That leaves 6 Tq for PS2.
    // Putting it all together we have:
    //
    // PS2 should be set to 6
    // PS1 should be set to 1
    // PRSEG should be set to 8
    //
    // The prescaler bits need to be set so to: (((1/2) * ((1/16) * 2e-6)) * osc_freq) - 1

    // CNF3
    // PHSEG[2:0] bits set the length (in time quanta) of t_ps2 if the BTLMODE bit (CNF2[7]) is set to 1 (register value + 1 is the number of time quanta)
    //                                                no effect if the BTLMODE bit (CNF2[7]) is set to 0
    uint8_t cnf3_value = 0x00;
    cnf3_value |= (0x05 & 0x07); // Set t_ps2 to 6 time quanta (we add 1 because the register value is one less than the number of time quanta)
    err = _write_instruction(context, MCP2515_REG_CNF3, &cnf3_value, 1);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    // CNF2
    // PRSEG[2:0] bits set t_propseg in units of time quanta
    // PHSEG1[2:0] bits set t_ps1 in units of time quanta
    // SAM bit sets the sampling mode - setting to 1 causes the bus to be sampled 3 times instead of 1
    // BTLMODE bit controls how t_ps2 is determined. If 1, t_ps2 is determined by CNF3:PHSEG[2:0]
    //                                               If 0, t_ps2 is greater than t_ps1. I'm not sure what this means.
    uint8_t cnf2_value = 0x00;
    cnf2_value |= 0x07; // Set t_propseg to 8 time quanta (value is register value + 1)
    cnf2_value |= (0x00 << 3); // Set t_ps1 to 1 time quantum (value is register value + 1)
    cnf2_value |= (0x01 << 7); // Set BTLMODE to 1 so that t_ps2 is determined by CNF3:PHSEG[2:0]
    err = _write_instruction(context, MCP2515_REG_CNF2, &cnf2_value, 1);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    // CNF1 (baud rate prescaler)
    // BRP[5:0] bits control the baudrate prescaler. These bits set the length of Tq relative to the oscillator frequency.
    //          Tq = 2 * (BRP[5:0] + 1) / oscillator frequency
    // SJW[1:0] bits select the SJW in terms of number of time quanta.
    uint8_t cnf1_value = 0x00;
    uint8_t brp_value = (int8_t)(((1.0/2.0) * ((1.0/16.0) * 2e-6) * config->oscillator_freq_hz) - 1);
    cnf1_value |= (brp_value & 0x3F);
    cnf1_value |= (0x01 << 6); // Set SJW to 2 time quanta
    err = _write_instruction(context, MCP2515_REG_CNF1, &cnf1_value, 1);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    return ARTIE_CAN_ERR_NONE;
}

static artie_can_error_t _configure_interrupts(artie_can_context_t *context, driver_mcp2515_config_t *config)
{
    artie_can_error_t err;

    // CANINTE
    // The Artie CAN library requires the following interrupts:
    // - RX buffer 0 full interrupt (RX0IE in CANINTE[0])
    // - RX buffer 1 full interrupt (RX1IE in CANINTE[1])
    // - Error interrupt flag (ERRIE in CANINTE[5])
    // TODO: Decide if we want the TX interrupts
    uint8_t caninte_value = 0x00;
    caninte_value |= (1 << 0); // Enable RX buffer 0 full interrupt
    caninte_value |= (1 << 1); // Enable RX buffer 1 full interrupt
    caninte_value |= (1 << 5); // Enable error interrupt
    err = _write_instruction(context, MCP2515_REG_CANINTE, &caninte_value, 1);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    // CANINTF
    // Clear all interrupt flags to start with a clean slate
    uint8_t canintf_value = 0x00;
    err = _write_instruction(context, MCP2515_REG_CANINTF, &canintf_value, 1);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    return ARTIE_CAN_ERR_NONE;
}

static artie_can_error_t _configure_buffer_control_registers(artie_can_context_t *context, driver_mcp2515_config_t *config)
{
    artie_can_error_t err;

    // If we are configured to use RTACP, we will dedicate TX buffer 2 to RTACP messages and should set
    // that buffer's priority bits accordingly. The other buffers will be used for all other protocols,
    // so their priority bits can be set to the same value as one another.

    // TXB0CTRL
    uint8_t txb0ctrl_value = 0x00;
    err = _write_instruction(context, MCP2515_REG_TXB0CTRL, &txb0ctrl_value, 1);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    // TXB1CTRL
    uint8_t txb1ctrl_value = 0x00;
    err = _write_instruction(context, MCP2515_REG_TXB1CTRL, &txb1ctrl_value, 1);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    // TXB2CTRL
    uint8_t txb2ctrl_value = 0x00;
    if (context->protocol_flags & ARTIE_CAN_PROTOCOL_FLAG_RTACP)
    {
        // Set TX buffer 2 to have the highest priority since it's dedicated to RTACP messages
        txb2ctrl_value |= 0x03;
    }
    else
    {
        // If we're not using RTACP, we can set all buffers to the same priority since they're all used for the same protocols
        txb2ctrl_value |= 0x00;
    }

    // RXB0CTRL (set everything here to 0)
    uint8_t rxb0ctrl_value = 0x00;
    err = _write_instruction(context, MCP2515_REG_RXB0CTRL, &rxb0ctrl_value, 1);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    // RXB1CTRL
    uint8_t rxb1ctrl_value = 0x00;
    err = _write_instruction(context, MCP2515_REG_RXB1CTRL, &rxb1ctrl_value, 1);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    return ARTIE_CAN_ERR_NONE;
}

static void _get_two_highest_priority_filters(artie_can_context_t *context, uint32_t *filters_out)
{
    // We will set the filters based on the two highest priority protocols that are enabled in the context.
    // The priority order is RTACP > RPCACP > PSACP > BWACP
    // If only one protocol is enabled, both filters will be set to the same value.
    // Additionally, we set the EIDE bit and address bits in the filters.

    // First set the address bits and EIDE bit.
    filters_out[0] = ((node_address << 8) & 0x0001F800) | (1 << 19);

    // How many protocols are enabled?
    size_t num_protocols_enabled = 0;
    bool enabled_protocols_in_priority_order[5] = {false};
    uint32_t filters[5] = {filters_out[0], filters_out[0], filters_out[0], filters_out[0], filters_out[0]};
    if (context->protocol_flags & ARTIE_CAN_PROTOCOL_FLAG_RTACP)
    {
        num_protocols_enabled++;
        enabled_protocols_in_priority_order[0] = true;
        filters[0] |= 0x00000000; // 000 priority
    }

    if (context->protocol_flags & ARTIE_CAN_PROTOCOL_FLAG_RPCACP)
    {
        num_protocols_enabled++;
        enabled_protocols_in_priority_order[1] = true;
        filters[1] |= 0x40000000; // 010 priority
    }

    if (context->protocol_flags & ARTIE_CAN_PROTOCOL_FLAG_PSACP)
    {
        num_protocols_enabled++;
        enabled_protocols_in_priority_order[2] = true;
        filters[2] |= 0x80000000; // 100 priority
        enabled_protocols_in_priority_order[3] = true;
        filters[3] |= 0xC0000000; // 110 priority; PSACP has two different priorities
    }

    if (context->protocol_flags & ARTIE_CAN_PROTOCOL_FLAG_BWACP)
    {
        num_protocols_enabled++;
        enabled_protocols_in_priority_order[4] = true;
        filters[4] |= 0xA0000000; // 101 priority
    }

    // Take the first 2 filters in priority order
    size_t out_index = 0;
    for (size_t i = 0; i < 5; i++)
    {
        if (enabled_protocols_in_priority_order[i])
        {
            filters_out[out_index] = filters[i];
            out_index++;
        }

        if (out_index >= 2)
        {
            break;
        }
    }
}

static artie_can_error_t _write_filter_or_mask(artie_can_context_t *context, uint8_t start_addr, uint32_t filter_or_mask)
{
    artie_can_error_t err;

    uint8_t sidh_value = (filter_or_mask & 0xFF000000) >> 24;
    err = _write_instruction(context, start_addr + 0, &sidh_value, 1);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    uint8_t sidl_value = (filter_or_mask & 0x00FF0000) >> 16;
    err = _write_instruction(context, start_addr + 1, &sidl_value, 1);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    uint8_t eid8_value = (filter_or_mask & 0x0000FF00) >> 8;
    err = _write_instruction(context, start_addr + 2, &eid8_value, 1);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    uint8_t eid0_value = (filter_or_mask & 0x000000FF) >> 0;
    err = _write_instruction(context, start_addr + 3, &eid0_value, 1);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    return ARTIE_CAN_ERR_NONE;
}

static artie_can_error_t _configure_filters_rfx0(artie_can_context_t *context, driver_mcp2515_config_t *config)
{
    artie_can_error_t err;

    // Initialize the filter bits based on node address and protocol flags in the context
    // We filter based on which protocols we are configured to use (we only accept frames that
    // are of a protocol that we are configured to use) and we filter based on target address
    // so that we can ignore all messages not meant for us.
    // However, the pub/sub address space is quite large. If we are configured to use pub/sub,
    // we always accept all frames of any protocol that we are interested in, regardless of address.
    uint32_t filter_mask = (context->protocol_flags & ARTIE_CAN_PROTOCOL_FLAG_PSACP) ? 0xE0000000 : 0xE001F800;
    err = _write_filter_or_mask(context, MCP2515_REG_RXM0SIDH, filter_mask);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    // RXF0 and RXF1 are the filter bits for RX buffer 0. We will set these based on the node address and protocol flags in the context.
    // Since there are only two filters for RX buffer 0, we will use them to filter for the two highest priority protocols.
    uint32_t filters[2] = {0, 0};
    _get_two_highest_priority_filters(context, filters); // Also fills the address bits and EIDE bit, regardless of the protocol. If only one protocol is enabled, both filters are identical.
    uint32_t filter_rxf0 = filters[0];
    uint32_t filter_rxf1 = filters[1];

    err = _write_filter_or_mask(context, MCP2515_REG_RXF0SIDH, filter_rxf0);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    err = _write_filter_or_mask(context, MCP2515_REG_RXF1SIDH, filter_rxf1);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    return ARTIE_CAN_ERR_NONE;
}

static artie_can_error_t _configure_filters_rfx1(artie_can_context_t *context, driver_mcp2515_config_t *config)
{
    artie_can_error_t err;

    // Initialize the filter bits based on node address and protocol flags in the context
    // We filter based on which protocols we are configured to use (we only accept frames that
    // are of a protocol that we are configured to use) and we filter based on target address
    // so that we can ignore all messages not meant for us.
    // However, the pub/sub address space is quite large. If we are configured to use pub/sub,
    // we always accept all frames of any protocol that we are interested in, regardless of address.
    uint32_t filter_mask = (context->protocol_flags & ARTIE_CAN_PROTOCOL_FLAG_PSACP) ? 0xE0000000 : 0xE001F800;

    // If we are configured for every type of protocol, we don't have enough filters, so just accept all messages
    // in this buffer.
    if (context->protocol_flags == (ARTIE_CAN_PROTOCOL_FLAG_RTACP | ARTIE_CAN_PROTOCOL_FLAG_PSACP | ARTIE_CAN_PROTOCOL_FLAG_RPCACP | ARTIE_CAN_PROTOCOL_FLAG_BWACP))
    {
        filter_mask = 0x00000000;
    }

    err = _write_filter_or_mask(context, MCP2515_REG_RXM1SIDH, filter_mask);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    // If we accept all messages, we are done
    if (filter_mask == 0x00000000)
    {
        return ARTIE_CAN_ERR_NONE;
    }

    // Otherwise, we can configure our four available filters

    // Lazy: find the highest priority protocol and set them all equal to that to begin with.
    uint32_t filters[2] = {0, 0};
    _get_two_highest_priority_filters(context, filters); // Also fills the address bits and EIDE bit, regardless of the protocol. If only one protocol is enabled, both filters are identical.
    uint32_t filter_rxf2 = filters[0];
    uint32_t filter_rxf3 = filters[0];
    uint32_t filter_rxf4 = filters[0];
    uint32_t filter_rxf5 = filters[0];

    // Now filter on the other protocols, if available
    artie_can_protocol_t protocols[4] = {ARTIE_CAN_PROTOCOL_FLAG_RTACP, ARTIE_CAN_PROTOCOL_FLAG_PSACP, ARTIE_CAN_PROTOCOL_FLAG_RPCACP, ARTIE_CAN_PROTOCOL_FLAG_BWACP};
    uint32_t all_filters[4] = {filter_rxf2, filter_rxf3, filter_rxf4, filter_rxf5};
    size_t filter_index = 0;
    for (int i = 0; i < 4; i++)
    {
        if (context->protocol_flags & protocols[i])
        {
            switch (protocols[i])
            {
                case ARTIE_CAN_PROTOCOL_FLAG_RTACP:
                    if (filter_index >= 4)
                    {
                        // This should never happen since we check for the case of all protocols being enabled above, but we'll check just in case.
                        return ARTIE_CAN_ERR_INTERNAL;
                    }
                    all_filters[filter_index] &= ~(ARTIE_CAN_FRAME_ID_PROTOCOL_MASK);
                    all_filters[filter_index] |= (ARTIE_CAN_RTACP_PROTOCOL_ID) << ARTIE_CAN_FRAME_ID_PROTOCOL_LOCATION;
                    filter_index += 1;
                    break;
                case ARTIE_CAN_PROTOCOL_FLAG_PSACP:
                    if (filter_index >= 3)
                    {
                        // This should never happen since we check for the case of all protocols being enabled above, but we'll check just in case.
                        return ARTIE_CAN_ERR_INTERNAL;
                    }
                    // Takes up two filters (one high priority and one low priority)
                    all_filters[filter_index] &= ~(ARTIE_CAN_FRAME_ID_PROTOCOL_MASK);
                    all_filters[filter_index] |= (ARTIE_CAN_PSACP_HIGH_PROTOCOL_ID) << ARTIE_CAN_FRAME_ID_PROTOCOL_LOCATION;
                    filter_index += 1;
                    all_filters[filter_index] &= ~(ARTIE_CAN_FRAME_ID_PROTOCOL_MASK);
                    all_filters[filter_index] |= (ARTIE_CAN_PSACP_LOW_PROTOCOL_ID) << ARTIE_CAN_FRAME_ID_PROTOCOL_LOCATION;
                    filter_index += 1;
                    break;
                case ARTIE_CAN_PROTOCOL_FLAG_RPCACP:
                    if (filter_index >= 4)
                    {
                        // This should never happen since we check for the case of all protocols being enabled above, but we'll check just in case.
                        return ARTIE_CAN_ERR_INTERNAL;
                    }
                    all_filters[filter_index] &= ~(ARTIE_CAN_FRAME_ID_PROTOCOL_MASK);
                    all_filters[filter_index] |= (ARTIE_CAN_RPCACP_PROTOCOL_ID) << ARTIE_CAN_FRAME_ID_PROTOCOL_LOCATION;
                    filter_index += 1;
                    break;
                case ARTIE_CAN_PROTOCOL_FLAG_BWACP:
                    if (filter_index >= 4)
                    {
                        // This should never happen since we check for the case of all protocols being enabled above, but we'll check just in case.
                        return ARTIE_CAN_ERR_INTERNAL;
                    }
                    all_filters[filter_index] &= ~(ARTIE_CAN_FRAME_ID_PROTOCOL_MASK);
                    all_filters[filter_index] |= (ARTIE_CAN_BWACP_PROTOCOL_ID) << ARTIE_CAN_FRAME_ID_PROTOCOL_LOCATION;
                    filter_index += 1;
                    break;
                default:
                    return ARTIE_CAN_ERR_INTERNAL;
            }
        }
    }

    // Now write all four filters
    err = _write_filter_or_mask(context, MCP2515_REG_RXF2SIDH, all_filters[0]);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    err = _write_filter_or_mask(context, MCP2515_REG_RXF3SIDH, all_filters[1]);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    err = _write_filter_or_mask(context, MCP2515_REG_RXF4SIDH, all_filters[2]);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    err = _write_filter_or_mask(context, MCP2515_REG_RXF5SIDH, all_filters[3]);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    return ARTIE_CAN_ERR_NONE;
}

static artie_can_error_t _get_mode(artie_can_context_t *context, mcp2515_mode_t *mode)
{
    // Read the CANSTAT register to determine the current mode (bits 7-5)
    uint8_t read_byte;
    artie_can_error_t err = _read_instruction(context, MCP2515_REG_CANSTAT, &read_byte, 1);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    // Get the mode from the CANSTAT register (bits 7-5)
    uint8_t precast_mode = ((read_byte & 0xE0) >> 5);
    if (precast_mode > MCP2515_MODE_CONFIGURATION)
    {
        // This should never happen since the mode bits should only ever be 0-4, but we'll check just in case.
        return ARTIE_CAN_ERR_DRIVER;
    }

    // Convert to the enum type and return via output parameter
    *mode = (mcp2515_mode_t)precast_mode;

    return ARTIE_CAN_ERR_NONE;
}

static artie_can_error_t _set_mode(artie_can_context_t *context, mcp2515_mode_t mode)
{
    // Check if the device is already in the requested mode
    artie_can_error_t err;
    mcp2515_mode_t current_mode;
    err = _get_mode(context, &current_mode);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    // If we're already in the requested mode, do nothing
    if (current_mode == mode)
    {
        return ARTIE_CAN_ERR_NONE;
    }

    // Otherwise, set the mode by writing to the CANCTRL register (bits 7-5)
    uint8_t mode_bits = ((uint8_t)mode << 5) & 0xE0;
    err = _bit_modify_instruction(context, MCP2515_REG_CANCTRL, 0xE0, mode_bits);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t driver_mcp2515_init(artie_can_context_t *context)
{
    artie_can_error_t err;

    // TODO

    // Reset the device to ensure it's in a known state
    err = _reset_instruction(context);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t driver_mcp2515_config(artie_can_context_t *context, driver_mcp2515_config_t *config)
{
    artie_can_error_t err;

    // Cast context
    artie_can_mcp2515_context_t *mcp2515_ctx = (artie_can_mcp2515_context_t *)(context->backend_context);

    // Save the configuration to the context
    // TODO

    // Set device to configuration mode
    err = _set_mode(context, MCP2515_MODE_CONFIGURATION);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    // Set the contents of BFPCTRL
    err = _configure_bfpctrl(context, config);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    // Set the contents of TXRTSCTRL (we do not make use of the RTS pins, so we'll just disable them)
    err = _configure_txrtsctrl(context, config);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    // Set the contents of CANCTRL
    err = _configure_canctrl(context, config);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    // Set the contents of CNF1, CNF2, and CNF3 to configure the bit timing
    err = _configure_bit_timing(context, config);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    // Set up the interrupt configuration
    err = _configure_interrupts(context, config);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    // Set up the TX and RX buffer control registers
    err = _configure_buffer_control_registers(context, config);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    // Set up the filters for RFX0 (high priority buffer)
    err = _configure_filters_rfx0(context, config);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    // Set up the filters for RFX1 (low priority buffer)
    err = _configure_filters_rfx1(context, config);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    // Switch to selected mode
    err = _set_mode(context, config->mode);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    // TODO: Go back through this function and ensure that we verify each write.

    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t driver_mcp2515_deinit(artie_can_context_t *context)
{
    // TODO

    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t driver_mcp2515_send(artie_can_context_t *context, const artie_can_frame_t *frame)
{
    // TODO

    // Check the error flags on the device first to ensure we can actually send right now

    // Choose which TXBnCTRL register based on the frame protocol. We allocate TXB2 for
    // RTACP frames (if this node is configured for RTACP). The other buffers are used
    // for the other protocols.

    // Check TXBnCTRL register

    // Load TXBnSIDH

    // Load TXBnSIDL, including the EXIDE bit

    // Load TXBnEIDm

    // Load TXBnDLC

    // If data is present, load TXBnDm

    // Send RTS instruction for the appropriate buffer

    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t driver_mcp2515_isr(artie_can_context_t *context)
{
    // TODO

    // Read the CANINTF register to determine which interrupt(s) occurred

    // If it was a RX buffer full interrupt, read the frame from the appropriate RX buffer and call the receive callback with the frame data

    // If it was an error interrupt, read the error flags from EFLG and return an appropriate error code

    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t driver_mcp2515_receive(artie_can_context_t *context, artie_can_frame_t *frame)
{
    // TODO
    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t driver_mcp2515_reset(artie_can_context_t *context)
{
    artie_can_error_t err;

    // Reset the device
    err = _reset_instruction(context);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    // Reconfigure device according to context
    err = driver_mcp2515_config(context, todo);
    if (err != ARTIE_CAN_ERR_NONE)
    {
        return err;
    }

    return ARTIE_CAN_ERR_NONE;
}
