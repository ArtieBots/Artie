#include <stdbool.h>
#include <stdint.h>
#include "backend.h"
#include "backend_tcp.h"
#include "err.h"

artie_can_error_t artie_can_init_context_tcp(artie_can_tcp_context_t *context, const char *host, uint16_t port, bool is_server)
{
    // TODO
}

artie_can_error_t artie_can_init_tcp(artie_can_tcp_context_t *context, artie_can_backend_t *handle)
{
    if (context == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // TODO: Implement TCP initialization logic here
    // Don't forget to copy the context struct into the handle

    return ARTIE_CAN_ERR_NONE;
}
