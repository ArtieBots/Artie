/**
 * @file artie_can_userdefs.h
 * @brief Artie CAN library user definitions header file
 *
 * This file is intended for users of the Artie CAN library to adjust compile-time
 * configuration parameters, such as buffer sizes and limits. It is included by artie_can.h,
 * so there is no need to include it directly.
 */
#ifndef ARTIE_CAN_USERDEFS_H
#define ARTIE_CAN_USERDEFS_H

/** Number of mock (dead-end) contexts for testing purposes */
#define ARTIE_CAN_N_MOCK_CONTEXTS 1

/** Number of TCP mock contexts for testing purposes */
#define ARTIE_CAN_N_MOCK_TCP_CONTEXTS 1

/** Number of MCP2515 CAN contexts */
#define ARTIE_CAN_N_MCP2515_CONTEXTS 1

#endif /* ARTIE_CAN_USERDEFS_H */
