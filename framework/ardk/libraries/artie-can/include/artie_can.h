/**
 * @file artie_can.h
 * @brief Artie CAN library main header file
 *
 * This library provides an interface for communication over the Controller Area Network (CAN)
 * bus used in Artie robots, implementing the Artie CAN Protocol as specified in:
 * docs/specifications/CANProtocol.md
 *
 * The library supports four protocols:
 * - RTACP (Real Time Artie CAN Protocol) - for strict real-time message delivery
 * - RPCACP (Remote Procedure Call Artie CAN Protocol) - for RPCs
 * - PSACP (Pub/Sub Artie CAN Protocol) - for pub/sub messaging
 * - BWACP (Block Write Artie CAN Protocol) - for large data transfers
 *
 * No dynamic memory allocation is used.
 */

#ifndef ARTIE_CAN_H
#define ARTIE_CAN_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ===== Constants ===== */

#define ARTIE_CAN_MAX_DATA_SIZE 8
#define ARTIE_CAN_MAX_ADDRESS 0x3F
#define ARTIE_CAN_BROADCAST_ADDRESS 0x00
#define ARTIE_CAN_MULTICAST_ADDRESS 0x3F

/* Protocol identifiers (top 3 bits of CAN ID) */
#define ARTIE_CAN_PROTOCOL_RTACP 0x00  /* 000 */
#define ARTIE_CAN_PROTOCOL_RPCACP 0x02 /* 010 */
#define ARTIE_CAN_PROTOCOL_PSACP_HIGH 0x04 /* 100 */
#define ARTIE_CAN_PROTOCOL_BWACP 0x05 /* 101 */
#define ARTIE_CAN_PROTOCOL_PSACP_LOW 0x06 /* 110 */

/* Priority levels */
#define ARTIE_CAN_PRIORITY_HIGH 0x00
#define ARTIE_CAN_PRIORITY_MED_HIGH 0x01
#define ARTIE_CAN_PRIORITY_MED_LOW 0x02
#define ARTIE_CAN_PRIORITY_LOW 0x03

/* Maximum payload sizes (accounting for byte stuffing overhead) */
#define ARTIE_CAN_MAX_STUFFED_PAYLOAD 2048
#define ARTIE_CAN_MAX_RPC_PAYLOAD 1024
#define ARTIE_CAN_MAX_PUBSUB_PAYLOAD 2048

/* Error codes (errno compatible) */
#define ARTIE_CAN_SUCCESS 0
#define ARTIE_CAN_ERROR_PERM 0x01    /* EPERM */
#define ARTIE_CAN_ERROR_E2BIG 0x07   /* E2BIG */
#define ARTIE_CAN_ERROR_ENOEXEC 0x08 /* ENOEXEC */
#define ARTIE_CAN_ERROR_EAGAIN 0x0B  /* EAGAIN */
#define ARTIE_CAN_ERROR_EINVAL 0x16  /* EINVAL */
#define ARTIE_CAN_ERROR_EALREADY 0x72 /* EALREADY */

/* ===== Type Definitions ===== */

/**
 * @brief CAN frame structure
 */
typedef struct {
    uint32_t can_id;    /**< CAN identifier (29 bits for extended) */
    uint8_t dlc;        /**< Data length code (0-8) */
    uint8_t data[ARTIE_CAN_MAX_DATA_SIZE]; /**< Data payload */
    bool extended;      /**< True if extended CAN frame */
} artie_can_frame_t;

/**
 * @brief Backend types
 */
typedef enum {
    ARTIE_CAN_BACKEND_SOCKETCAN,  /**< Linux SocketCAN backend */
    ARTIE_CAN_BACKEND_MCP2515,    /**< Bare-metal MCP2515 backend */
    ARTIE_CAN_BACKEND_MOCK        /**< Mock backend for testing */
} artie_can_backend_type_t;

/**
 * @brief Backend interface structure
 */
typedef struct {
    /**
     * @brief Initialize the CAN backend
     * @param ctx Backend-specific context
     * @return 0 on success, negative error code on failure
     */
    int (*init)(void *ctx);

    /**
     * @brief Send a CAN frame
     * @param ctx Backend-specific context
     * @param frame Frame to send
     * @return 0 on success, negative error code on failure
     */
    int (*send)(void *ctx, const artie_can_frame_t *frame);

    /**
     * @brief Receive a CAN frame
     * @param ctx Backend-specific context
     * @param frame Buffer to store received frame
     * @param timeout_ms Timeout in milliseconds (0 for non-blocking)
     * @return 0 on success, negative error code on failure
     */
    int (*receive)(void *ctx, artie_can_frame_t *frame, uint32_t timeout_ms);

    /**
     * @brief Close the CAN backend
     * @param ctx Backend-specific context
     * @return 0 on success, negative error code on failure
     */
    int (*close)(void *ctx);

    void *context; /**< Backend-specific context */
} artie_can_backend_t;

/**
 * @brief RTACP frame types
 */
typedef enum {
    ARTIE_CAN_RTACP_ACK = 0,
    ARTIE_CAN_RTACP_MSG = 1
} artie_can_rtacp_frame_type_t;

/**
 * @brief RTACP message structure
 */
typedef struct {
    uint8_t priority;
    uint8_t sender_addr;
    uint8_t target_addr;
    artie_can_rtacp_frame_type_t frame_type;
    uint8_t data[ARTIE_CAN_MAX_DATA_SIZE];
    uint8_t data_len;
} artie_can_rtacp_msg_t;

/**
 * @brief RPCACP frame types
 */
typedef enum {
    ARTIE_CAN_RPCACP_ACK = 0,
    ARTIE_CAN_RPCACP_NACK = 1,
    ARTIE_CAN_RPCACP_START_RPC = 2,
    ARTIE_CAN_RPCACP_START_RETURN = 3,
    ARTIE_CAN_RPCACP_TX_DATA = 4,
    ARTIE_CAN_RPCACP_RX_DATA = 5
} artie_can_rpcacp_frame_type_t;

/**
 * @brief RPCACP message structure
 */
typedef struct {
    uint8_t priority;
    uint8_t sender_addr;
    uint8_t target_addr;
    uint8_t random_value;
    artie_can_rpcacp_frame_type_t frame_type;
    bool is_synchronous;
    uint8_t procedure_id;
    uint16_t crc16;
    uint8_t payload[ARTIE_CAN_MAX_RPC_PAYLOAD];
    size_t payload_len;
    uint8_t nack_error_code;  /* Only for NACK frames */
} artie_can_rpcacp_msg_t;

/**
 * @brief PSACP frame types
 */
typedef enum {
    ARTIE_CAN_PSACP_PUB = 1,
    ARTIE_CAN_PSACP_DATA = 3
} artie_can_psacp_frame_type_t;

/**
 * @brief PSACP message structure
 */
typedef struct {
    uint8_t priority;
    uint8_t sender_addr;
    uint8_t topic;
    bool high_priority;
    artie_can_psacp_frame_type_t frame_type;
    uint16_t crc16;
    uint8_t payload[ARTIE_CAN_MAX_PUBSUB_PAYLOAD];
    size_t payload_len;
} artie_can_psacp_msg_t;

/**
 * @brief BWACP frame types
 */
typedef enum {
    ARTIE_CAN_BWACP_REPEAT = 1,
    ARTIE_CAN_BWACP_READY = 3,
    ARTIE_CAN_BWACP_DATA = 7
} artie_can_bwacp_frame_type_t;

/**
 * @brief BWACP class bit positions for multicast
 */
typedef enum {
    ARTIE_CAN_BWACP_CLASS_SBC = 0,
    ARTIE_CAN_BWACP_CLASS_MCU = 1,
    ARTIE_CAN_BWACP_CLASS_SENSOR = 2,
    ARTIE_CAN_BWACP_CLASS_MOTOR = 3,
    ARTIE_CAN_BWACP_CLASS_RESERVED_4 = 4,
    ARTIE_CAN_BWACP_CLASS_RESERVED_5 = 5
} artie_can_bwacp_class_t;

/**
 * @brief BWACP message structure
 */
typedef struct {
    uint8_t priority;
    uint8_t sender_addr;
    uint8_t target_addr;
    uint8_t class_mask;  /* For multicast */
    artie_can_bwacp_frame_type_t frame_type;
    bool is_repeat;
    bool parity;
    uint32_t crc24;  /* For READY frame */
    uint32_t address;  /* Application-specific address */
    uint8_t payload[ARTIE_CAN_MAX_STUFFED_PAYLOAD];
    size_t payload_len;
} artie_can_bwacp_msg_t;

/**
 * @brief Main CAN context structure
 */
typedef struct {
    uint8_t node_address;  /**< This node's address (6 bits) */
    artie_can_backend_t backend;  /**< Backend interface */
} artie_can_context_t;

/* ===== Core API Functions ===== */

/**
 * @brief Initialize the Artie CAN context
 * @param ctx Context to initialize
 * @param node_address This node's CAN address (0-63)
 * @param backend_type Type of backend to use
 * @return 0 on success, negative error code on failure
 */
int artie_can_init(artie_can_context_t *ctx, uint8_t node_address, artie_can_backend_type_t backend_type);

/**
 * @brief Initialize with custom backend
 * @param ctx Context to initialize
 * @param node_address This node's CAN address (0-63)
 * @param backend Custom backend implementation
 * @return 0 on success, negative error code on failure
 */
int artie_can_init_custom(artie_can_context_t *ctx, uint8_t node_address, const artie_can_backend_t *backend);

/**
 * @brief Close the CAN context
 * @param ctx Context to close
 * @return 0 on success, negative error code on failure
 */
int artie_can_close(artie_can_context_t *ctx);

/* ===== RTACP Functions ===== */

/**
 * @brief Send an RTACP message
 * @param ctx CAN context
 * @param msg Message to send
 * @param wait_ack Wait for ACK if targeted (not broadcast)
 * @return 0 on success, negative error code on failure
 */
int artie_can_rtacp_send(artie_can_context_t *ctx, const artie_can_rtacp_msg_t *msg, bool wait_ack);

/**
 * @brief Receive an RTACP message
 * @param ctx CAN context
 * @param msg Buffer to store received message
 * @param timeout_ms Timeout in milliseconds (0 for non-blocking)
 * @return 0 on success, negative error code on failure
 */
int artie_can_rtacp_receive(artie_can_context_t *ctx, artie_can_rtacp_msg_t *msg, uint32_t timeout_ms);

/* ===== RPCACP Functions ===== */

/**
 * @brief Send an RPC request
 * @param ctx CAN context
 * @param target_addr Target node address
 * @param priority Message priority
 * @param is_synchronous True for synchronous (blocking) RPC
 * @param procedure_id RPC procedure ID
 * @param payload Serialized arguments (MsgPack format)
 * @param payload_len Payload length
 * @return 0 on success, negative error code on failure
 */
int artie_can_rpcacp_call(artie_can_context_t *ctx, uint8_t target_addr, uint8_t priority,
                          bool is_synchronous, uint8_t procedure_id,
                          const uint8_t *payload, size_t payload_len);

/**
 * @brief Wait for RPC response (for synchronous RPCs)
 * @param ctx CAN context
 * @param response Buffer to store response payload
 * @param max_len Maximum response length
 * @param actual_len Actual response length received
 * @param timeout_ms Timeout in milliseconds
 * @return 0 on success, negative error code on failure
 */
int artie_can_rpcacp_wait_response(artie_can_context_t *ctx, uint8_t *response,
                                   size_t max_len, size_t *actual_len, uint32_t timeout_ms);

/**
 * @brief Receive and handle an RPC request
 * @param ctx CAN context
 * @param msg Buffer to store received RPC message
 * @param timeout_ms Timeout in milliseconds
 * @return 0 on success, negative error code on failure
 */
int artie_can_rpcacp_receive(artie_can_context_t *ctx, artie_can_rpcacp_msg_t *msg, uint32_t timeout_ms);

/**
 * @brief Send an RPC response (for synchronous RPCs)
 * @param ctx CAN context
 * @param target_addr Target node address (original requester)
 * @param priority Message priority
 * @param procedure_id RPC procedure ID
 * @param random_value Random value from request
 * @param payload Serialized return value (MsgPack format)
 * @param payload_len Payload length
 * @return 0 on success, negative error code on failure
 */
int artie_can_rpcacp_respond(artie_can_context_t *ctx, uint8_t target_addr, uint8_t priority,
                             uint8_t procedure_id, uint8_t random_value,
                             const uint8_t *payload, size_t payload_len);

/**
 * @brief Send an ACK for an RPC request
 * @param ctx CAN context
 * @param target_addr Target node address (original requester)
 * @param priority Message priority
 * @param random_value Random value from request
 * @return 0 on success, negative error code on failure
 */
int artie_can_rpcacp_send_ack(artie_can_context_t *ctx, uint8_t target_addr,
                              uint8_t priority, uint8_t random_value);

/**
 * @brief Send a NACK for an RPC request
 * @param ctx CAN context
 * @param target_addr Target node address (original requester)
 * @param priority Message priority
 * @param random_value Random value from request
 * @param error_code Error code (errno value)
 * @return 0 on success, negative error code on failure
 */
int artie_can_rpcacp_send_nack(artie_can_context_t *ctx, uint8_t target_addr,
                               uint8_t priority, uint8_t random_value, uint8_t error_code);

/* ===== PSACP Functions ===== */

/**
 * @brief Publish a message to a topic
 * @param ctx CAN context
 * @param topic Topic ID
 * @param priority Message priority
 * @param high_priority True to use high priority pub/sub (compete with RTACP)
 * @param payload Serialized message data
 * @param payload_len Payload length
 * @return 0 on success, negative error code on failure
 */
int artie_can_psacp_publish(artie_can_context_t *ctx, uint8_t topic, uint8_t priority,
                            bool high_priority, const uint8_t *payload, size_t payload_len);

/**
 * @brief Receive a published message
 * @param ctx CAN context
 * @param msg Buffer to store received message
 * @param timeout_ms Timeout in milliseconds
 * @return 0 on success, negative error code on failure
 */
int artie_can_psacp_receive(artie_can_context_t *ctx, artie_can_psacp_msg_t *msg, uint32_t timeout_ms);

/* ===== BWACP Functions ===== */

/**
 * @brief Send a block write ready frame
 * @param ctx CAN context
 * @param target_addr Target address (or MULTICAST_ADDRESS for multicast)
 * @param class_mask Class mask for multicast (ignored if not multicast)
 * @param priority Message priority
 * @param address Application-specific address
 * @param payload Data payload
 * @param payload_len Payload length
 * @param interrupt If true, interrupt any ongoing transfer
 * @return 0 on success, negative error code on failure
 */
int artie_can_bwacp_send_ready(artie_can_context_t *ctx, uint8_t target_addr, uint8_t class_mask,
                               uint8_t priority, uint32_t address, const uint8_t *payload,
                               size_t payload_len, bool interrupt);

/**
 * @brief Send block write data (call after send_ready)
 * @param ctx CAN context
 * @param target_addr Target address
 * @param class_mask Class mask for multicast
 * @param priority Message priority
 * @param payload Data payload
 * @param payload_len Payload length
 * @return 0 on success, negative error code on failure
 */
int artie_can_bwacp_send_data(artie_can_context_t *ctx, uint8_t target_addr, uint8_t class_mask,
                              uint8_t priority, const uint8_t *payload, size_t payload_len);

/**
 * @brief Receive block write messages
 * @param ctx CAN context
 * @param msg Buffer to store received message
 * @param timeout_ms Timeout in milliseconds
 * @return 0 on success, negative error code on failure
 */
int artie_can_bwacp_receive(artie_can_context_t *ctx, artie_can_bwacp_msg_t *msg, uint32_t timeout_ms);

/**
 * @brief Send a repeat request
 * @param ctx CAN context
 * @param target_addr Target address
 * @param priority Message priority
 * @param repeat_all True to repeat entire sequence, false to repeat last frame
 * @return 0 on success, negative error code on failure
 */
int artie_can_bwacp_send_repeat(artie_can_context_t *ctx, uint8_t target_addr,
                                uint8_t priority, bool repeat_all);

/* ===== Utility Functions ===== */

/**
 * @brief Parse a CAN frame to determine protocol type
 * @param frame CAN frame to parse
 * @return Protocol type (top 3 bits of ID)
 */
uint8_t artie_can_get_protocol(const artie_can_frame_t *frame);

/**
 * @brief Compute CRC16 over data
 * @param data Data buffer
 * @param len Data length
 * @return CRC16 value
 */
uint16_t artie_can_crc16(const uint8_t *data, size_t len);

/**
 * @brief Compute CRC24 over data
 * @param data Data buffer
 * @param len Data length
 * @return CRC24 value
 */
uint32_t artie_can_crc24(const uint8_t *data, size_t len);

/**
 * @brief Perform byte stuffing on data
 * @param input Input data
 * @param input_len Input data length
 * @param output Output buffer for stuffed data
 * @param output_max_len Maximum output buffer size
 * @param output_len Actual output length
 * @return 0 on success, negative error code on failure
 */
int artie_can_byte_stuff(const uint8_t *input, size_t input_len,
                         uint8_t *output, size_t output_max_len, size_t *output_len);

/**
 * @brief Remove byte stuffing from data
 * @param input Stuffed input data
 * @param input_len Stuffed input data length
 * @param output Output buffer for unstuffed data
 * @param output_max_len Maximum output buffer size
 * @param output_len Actual output length
 * @return 0 on success, negative error code on failure
 */
int artie_can_byte_unstuff(const uint8_t *input, size_t input_len,
                           uint8_t *output, size_t output_max_len, size_t *output_len);

#ifdef __cplusplus
}
#endif

#endif /* ARTIE_CAN_H */
