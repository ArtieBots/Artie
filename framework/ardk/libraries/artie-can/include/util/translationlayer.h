/**
 * @file translationlayer.h
 * @brief This header file includes the various things that are OS-specific.
 *
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

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
    #include <pthread.h>
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

// Platform-specific sleep functions
#ifdef _WIN32
    #define SLEEP_MS(ms) Sleep(ms)
    #define SLEEP_US(us) Sleep((us) / 1000)
#else
    #define SLEEP_MS(ms) usleep((ms) * 1000)
    #define SLEEP_US(us) usleep(us)
#endif

/**
 * @brief Platform-independent thread function signature.
 * @param arg Pointer to thread parameters.
 * @return Platform-specific thread return value.
 */
#ifdef _WIN32
    typedef DWORD (WINAPI *thread_func_t)(LPVOID arg);
#else
    typedef void* (*thread_func_t)(void* arg);
#endif

/**
 * @brief Create a new thread.
 * @param handle Pointer to store the thread handle.
 * @param func The thread function to execute.
 * @param arg Argument to pass to the thread function.
 * @return true on success, false on failure.
 */
bool create_thread(thread_handle_t *handle, thread_func_t func, void *arg);

/**
 * @brief Wait for a thread to complete.
 * @param handle The thread handle.
 * @param timeout_ms Timeout in milliseconds (0 for infinite wait).
 * @return true if the thread completed, false on timeout or error.
 */
bool join_thread(thread_handle_t handle, uint32_t timeout_ms);
