/**
 * @file artie_can_backend_socketcan.c
 * @brief SocketCAN backend for ARM64 Linux systems
 */

#include "artie_can.h"
#include "artie_can_backends.h"

#ifdef __linux__
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <poll.h>

typedef struct {
    int socket_fd;
    char interface[16];
} socketcan_context_t;

static socketcan_context_t g_socketcan_ctx = {-1, "can0"};

static int socketcan_init(void *ctx)
{
    socketcan_context_t *sc = (socketcan_context_t *)ctx;
    struct sockaddr_can addr;
    struct ifreq ifr;

    /* Create socket */
    sc->socket_fd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (sc->socket_fd < 0) {
        return -1;
    }

    /* Get interface index */
    strncpy(ifr.ifr_name, sc->interface, IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';

    if (ioctl(sc->socket_fd, SIOCGIFINDEX, &ifr) < 0) {
        close(sc->socket_fd);
        sc->socket_fd = -1;
        return -1;
    }

    /* Bind socket to CAN interface */
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(sc->socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sc->socket_fd);
        sc->socket_fd = -1;
        return -1;
    }

    return 0;
}

static int socketcan_send(void *ctx, const artie_can_frame_t *frame)
{
    socketcan_context_t *sc = (socketcan_context_t *)ctx;
    struct can_frame can_frame;

    if (sc->socket_fd < 0) {
        return -1;
    }

    /* Convert to SocketCAN frame */
    memset(&can_frame, 0, sizeof(can_frame));

    if (frame->extended) {
        can_frame.can_id = frame->can_id | CAN_EFF_FLAG;
    } else {
        can_frame.can_id = frame->can_id & CAN_SFF_MASK;
    }

    can_frame.can_dlc = frame->dlc;
    memcpy(can_frame.data, frame->data, frame->dlc);

    /* Send frame */
    ssize_t nbytes = write(sc->socket_fd, &can_frame, sizeof(struct can_frame));
    if (nbytes != sizeof(struct can_frame)) {
        return -1;
    }

    return 0;
}

static int socketcan_receive(void *ctx, artie_can_frame_t *frame, uint32_t timeout_ms)
{
    socketcan_context_t *sc = (socketcan_context_t *)ctx;
    struct can_frame can_frame;

    if (sc->socket_fd < 0) {
        return -1;
    }

    /* Use poll for timeout */
    struct pollfd pfd;
    pfd.fd = sc->socket_fd;
    pfd.events = POLLIN;

    int ret = poll(&pfd, 1, timeout_ms);
    if (ret < 0) {
        return -1;  /* Error */
    } else if (ret == 0) {
        return -2;  /* Timeout */
    }

    /* Receive frame */
    ssize_t nbytes = read(sc->socket_fd, &can_frame, sizeof(struct can_frame));
    if (nbytes != sizeof(struct can_frame)) {
        return -1;
    }

    /* Convert from SocketCAN frame */
    if (can_frame.can_id & CAN_EFF_FLAG) {
        frame->extended = true;
        frame->can_id = can_frame.can_id & CAN_EFF_MASK;
    } else {
        frame->extended = false;
        frame->can_id = can_frame.can_id & CAN_SFF_MASK;
    }

    frame->dlc = can_frame.can_dlc;
    memcpy(frame->data, can_frame.data, can_frame.can_dlc);

    return 0;
}

static int socketcan_close(void *ctx)
{
    socketcan_context_t *sc = (socketcan_context_t *)ctx;

    if (sc->socket_fd >= 0) {
        close(sc->socket_fd);
        sc->socket_fd = -1;
    }

    return 0;
}

int artie_can_backend_socketcan_init(artie_can_backend_t *backend)
{
    if (!backend) {
        return -1;
    }

    backend->init = socketcan_init;
    backend->send = socketcan_send;
    backend->receive = socketcan_receive;
    backend->close = socketcan_close;
    backend->context = &g_socketcan_ctx;

    return 0;
}

#else /* !__linux__ */

/* Stub implementation for non-Linux platforms */
int artie_can_backend_socketcan_init(artie_can_backend_t *backend)
{
    (void)backend;
    return -1;  /* Not supported on this platform */
}

#endif /* __linux__ */
