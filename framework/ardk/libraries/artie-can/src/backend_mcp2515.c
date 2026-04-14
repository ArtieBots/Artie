#include <stdbool.h>
#include <stdint.h>
#include "backend.h"
#include "backend_mcp2515.h"
#include "err.h"

artie_can_error_t artie_can_init_context_mcp2515(artie_can_mcp2515_context_t *context, uint8_t dummy)
{
    // TODO
    return ARTIE_CAN_ERR_NONE;
}

artie_can_error_t artie_can_init_mcp2515(artie_can_mcp2515_context_t *context, artie_can_backend_t *handle)
{
    if (context == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // TODO: Implement MCP2515 initialization logic here
    // Don't forget to copy the context into the resulting struct.

    return ARTIE_CAN_ERR_NONE;
}
