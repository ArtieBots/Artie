/**
 * @file frame.h
 * @brief Header file for Artie CAN frame structure.
 *
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

/** Maximum bytes in a single frame's data field */
#define ARTIE_CAN_FRAME_MAX_DATA_LENGTH 8U

/** Length of the CAN ID in bits; we use the least significant 29 bits of a uint32_t */
#define ARTIE_CAN_FRAME_ID_LENGTH (29U)

/** Location of the protocol bits in the CAN ID */
#define ARTIE_CAN_FRAME_ID_PROTOCOL_LOCATION ((ARTIE_CAN_FRAME_ID_LENGTH) - 3U)

/** Location of the frame type bits in the CAN ID */
#define ARTIE_CAN_FRAME_ID_FRAME_TYPE_LOCATION ((ARTIE_CAN_FRAME_ID_PROTOCOL_LOCATION) - 4U)

/** Location of the user-assigned priority bits in the CAN ID */
#define ARTIE_CAN_FRAME_ID_USER_PRIORITY_LOCATION ((ARTIE_CAN_FRAME_ID_FRAME_TYPE_LOCATION) - 2U)

/** Location of the sender address bits in the CAN ID */
#define ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION ((ARTIE_CAN_FRAME_ID_USER_PRIORITY_LOCATION) - 6U)

/** Location of the target address bits in the CAN ID */
#define ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION ((ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION) - 6U)

/** MSB 3 bits of CAN ID specifies protocol (and priority) */
#define ARTIE_CAN_FRAME_ID_PROTOCOL_MASK (0x07 << ARTIE_CAN_FRAME_ID_PROTOCOL_LOCATION)

/** Frame type bit mask */
#define ARTIE_CAN_FRAME_ID_FRAME_TYPE_MASK (0x0F << ARTIE_CAN_FRAME_ID_FRAME_TYPE_LOCATION)

/** User-assigned priority bit mask */
#define ARTIE_CAN_FRAME_ID_USER_PRIORITY_MASK (0x03 << ARTIE_CAN_FRAME_ID_USER_PRIORITY_LOCATION)

/** Sender address bit mask */
#define ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_MASK (0x3F << ARTIE_CAN_FRAME_ID_SENDER_ADDRESS_LOCATION)

/** Target address bit mask */
#define ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_MASK (0x3F << ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION)

/** Maximum nodes in the address space (subtract one for broadcast address) */
#define ARTIE_CAN_MAX_NODES ((ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_MASK >> ARTIE_CAN_FRAME_ID_TARGET_ADDRESS_LOCATION) - 1U)

// Detect compiler and define PACKED macros
#if defined(_MSC_VER)
    /** Placed before a packed struct definition; MSVC packs via a preceding #pragma instead of an attribute. */
    #define PACKED_STRUCT_BEGIN __pragma(pack(push, 1))
    /** Placed after a packed struct definition to restore the previous MSVC pack alignment. */
    #define PACKED_STRUCT_END   __pragma(pack(pop))
    /** Placed between `struct` and `{` in a packed struct definition; a no-op on MSVC, since packing is done via PACKED_STRUCT_BEGIN/END instead. */
    #define PACKED
#else
    /** Placed before a packed struct definition; a no-op on GCC/Clang, since packing is done via the PACKED attribute instead. */
    #define PACKED_STRUCT_BEGIN
    /** Placed after a packed struct definition; a no-op on GCC/Clang, since packing is done via the PACKED attribute instead. */
    #define PACKED_STRUCT_END
    /** Placed between `struct` and `{` in a packed struct definition to disable padding, via the compiler's packed attribute. */
    #define PACKED __attribute__((packed))
#endif

/**
 * @brief Structure representing a CAN frame in the Artie CAN library.
 *
 * This struct is packed to ensure it is serializable.
 */
PACKED_STRUCT_BEGIN
typedef struct PACKED {
    uint32_t id;                                    ///< CAN identifier (always 29 bits in Artie CAN)
    uint8_t dlc;                                    ///< Data Length Code (number of bytes in the data field, 0-8)
    uint8_t data[ARTIE_CAN_FRAME_MAX_DATA_LENGTH];  ///< Data field (up to 8 bytes)
} artie_can_frame_t;
PACKED_STRUCT_END
