/**
 * @file artie_can_utils.c
 * @brief Utility functions for Artie CAN library
 */

#include "artie_can.h"
#include <string.h>

/**
 * @brief CRC16-CCITT implementation
 * Polynomial: 0x1021 (x^16 + x^12 + x^5 + 1)
 * Initial value: 0xFFFF
 */
uint16_t artie_can_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }

    return crc;
}

/**
 * @brief CRC24 implementation
 * Polynomial: 0x864CFB (x^24 + x^23 + x^18 + x^17 + x^14 + x^11 + x^10 + x^7 + x^6 + x^5 + x^4 + x^3 + x + 1)
 * Initial value: 0xB704CE
 */
uint32_t artie_can_crc24(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xB704CE;
    const uint32_t polynomial = 0x864CFB;

    for (size_t i = 0; i < len; i++) {
        crc ^= (uint32_t)data[i] << 16;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x800000) {
                crc = (crc << 1) ^ polynomial;
            } else {
                crc = crc << 1;
            }
        }
    }

    return crc & 0xFFFFFF;  /* Keep only 24 bits */
}

/**
 * @brief Perform byte stuffing according to Artie CAN specification
 *
 * The first byte is a special byte indicating the index of the next special byte.
 * Special bytes must be inserted every 254 bytes at most.
 * 0xFF indicates no more data.
 * 0x00 special byte indicates an error.
 */
int artie_can_byte_stuff(const uint8_t *input, size_t input_len,
                         uint8_t *output, size_t output_max_len, size_t *output_len)
{
    if (!input || !output || !output_len) {
        return -1;
    }

    /* Handle empty input */
    if (input_len == 0) {
        if (output_max_len < 1) {
            return -1;  /* Not enough space */
        }
        output[0] = 0xFF;  /* No data */
        *output_len = 1;
        return 0;
    }

    size_t out_idx = 0;
    size_t in_idx = 0;

    while (in_idx < input_len) {
        /* Calculate how many data bytes we can write before next special byte */
        size_t remaining = input_len - in_idx;
        size_t chunk_size = (remaining > 254) ? 254 : remaining;

        /* Check if we have enough space for special byte + data + potentially final special byte */
        if (out_idx + 1 + chunk_size + 1 > output_max_len) {
            return -1;  /* Not enough space */
        }

        /* Write special byte indicating position of next special byte */
        output[out_idx++] = (uint8_t)chunk_size;

        /* Copy data bytes */
        memcpy(&output[out_idx], &input[in_idx], chunk_size);
        out_idx += chunk_size;
        in_idx += chunk_size;
    }

    /* Add final special byte */
    if (out_idx >= output_max_len) {
        return -1;  /* Not enough space */
    }
    output[out_idx++] = 0xFF;

    *output_len = out_idx;
    return 0;
}

/**
 * @brief Remove byte stuffing from data
 */
int artie_can_byte_unstuff(const uint8_t *input, size_t input_len,
                           uint8_t *output, size_t output_max_len, size_t *output_len)
{
    if (!input || !output || !output_len) {
        return -1;
    }

    if (input_len == 0) {
        return -1;  /* Invalid input */
    }

    /* Check for no data case */
    if (input_len == 1 && input[0] == 0xFF) {
        *output_len = 0;
        return 0;
    }

    size_t in_idx = 0;
    size_t out_idx = 0;

    while (in_idx < input_len) {
        uint8_t special_byte = input[in_idx++];

        /* Check for error condition */
        if (special_byte == 0x00) {
            return -1;  /* Error in stuffing */
        }

        /* Check for end marker */
        if (special_byte == 0xFF) {
            break;
        }

        /* Validate we have enough input data */
        if (in_idx + special_byte > input_len) {
            return -1;  /* Invalid stuffing */
        }

        /* Check output space */
        if (out_idx + special_byte > output_max_len) {
            return -1;  /* Not enough output space */
        }

        /* Copy data bytes */
        memcpy(&output[out_idx], &input[in_idx], special_byte);
        out_idx += special_byte;
        in_idx += special_byte;
    }

    *output_len = out_idx;
    return 0;
}

/**
 * @brief Parse CAN ID to determine protocol type
 */
uint8_t artie_can_get_protocol(const artie_can_frame_t *frame)
{
    if (!frame) {
        return 0xFF;  /* Invalid */
    }

    /* Extract top 3 bits of 29-bit CAN ID */
    return (uint8_t)((frame->can_id >> 26) & 0x07);
}
