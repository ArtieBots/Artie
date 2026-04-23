/**
 * @file translationlayer.h
 * @brief This header file includes the various things that are OS-specific.
 *
 */

#pragma once

// Platform-specific includes for sockets
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
#endif

// Platform-specific definitions for sockets
#ifdef _WIN32
    typedef SOCKET socket_t;
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
#else
    typedef int socket_t;
    #define INVALID_SOCKET_VALUE -1
#endif

// Platform-specific definitions for threads
#ifdef _WIN32
    typedef HANDLE thread_handle_t;
    #define INVALID_THREAD_HANDLE NULL
#else
    typedef pthread_t thread_handle_t;
    #define INVALID_THREAD_HANDLE 0
#endif

// Platform-specific sleep function
#ifdef _WIN32
    #define SLEEP_MS(ms) Sleep(ms)
#else
    #define SLEEP_MS(ms) usleep((ms) * 1000)
#endif
