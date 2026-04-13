#include "backend.h"
#include "err.h"
#include "rtacp.h"

artie_can_error_t artie_can_rtacp_init_frame(artie_can_backend_t *handle, artie_can_frame_t *frame, uint8_t todo)
{
    if (handle == NULL || frame == NULL)
    {
        return ARTIE_CAN_ERR_INVALID_ARG;
    }

    // TODO

    return ARTIE_CAN_ERR_NONE;
}
