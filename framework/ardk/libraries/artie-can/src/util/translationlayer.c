/**
 * @file translationlayer.c
 * @brief Platform-independent wrapper implementations for OS-specific functionality.
 *
 */

#include "translationlayer.h"

#ifndef _WIN32
#include <errno.h>
#endif

bool create_thread(thread_handle_t *handle, thread_func_t func, void *arg)
{
    if (handle == NULL || func == NULL)
    {
        return false;
    }

#ifdef _WIN32
    *handle = CreateThread(NULL, 0, func, arg, 0, NULL);
    return (*handle != NULL);
#else
    return (pthread_create(handle, NULL, func, arg) == 0);
#endif
}

bool join_thread(thread_handle_t handle, uint32_t timeout_ms)
{
#ifdef _WIN32
    DWORD wait_result = WaitForSingleObject(handle, (timeout_ms == 0) ? INFINITE : timeout_ms);
    if (wait_result == WAIT_OBJECT_0)
    {
        CloseHandle(handle);
        return true;
    }
    return false;
#else
    // POSIX doesn't have a timeout for pthread_join, so we just join unconditionally
    // In practice, the thread should exit quickly due to the stop flag
    (void)timeout_ms;  // Unused on POSIX
    return (pthread_join(handle, NULL) == 0);
#endif
}

static uint32_t _critical_section_enter(void)
{
#ifdef _WIN32
    // On Windows with native atomics, critical sections are not needed
    return 0;
#elif defined(__GNUC__) || defined(__clang__)
    // On POSIX with native atomics, critical sections are not needed
    return 0;
#elif defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__) || defined(__ARM_ARCH_6M__)
    // ARM Cortex-M: Disable interrupts using CPSID i (Change Processor State, Interrupt Disable)
    uint32_t primask;
    __asm__ volatile ("mrs %0, primask" : "=r" (primask));
    __asm__ volatile ("cpsid i" : : : "memory");
    return primask;
#elif defined(__ARM_ARCH)
    // Generic ARM: Use CPSR (Current Program Status Register)
    uint32_t cpsr;
    __asm__ volatile ("mrs %0, cpsr" : "=r" (cpsr));
    __asm__ volatile ("cpsid i" : : : "memory");
    return cpsr;
#else
    // Unknown bare metal platform: Provide a weak implementation
    // Users should override this for their specific platform
    return 0;
#endif
}

static void _critical_section_exit(uint32_t state)
{
#ifdef _WIN32
    // On Windows with native atomics, critical sections are not needed
    (void)state;
#elif defined(__GNUC__) || defined(__clang__)
    // On POSIX with native atomics, critical sections are not needed
    (void)state;
#elif defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__) || defined(__ARM_ARCH_6M__)
    // ARM Cortex-M: Restore interrupt state using PRIMASK
    __asm__ volatile ("msr primask, %0" : : "r" (state) : "memory");
#elif defined(__ARM_ARCH)
    // Generic ARM: Restore CPSR
    __asm__ volatile ("msr cpsr_c, %0" : : "r" (state) : "memory");
#else
    // Unknown bare metal platform: Provide a weak implementation
    (void)state;
#endif
}

void atomic_store(atomic_uint32_t *ptr, uint32_t value)
{
#ifdef _WIN32
    // Windows: Use InterlockedExchange which returns the original value, but we ignore it here
    InterlockedExchange((volatile LONG *)ptr, (LONG)value);
#else
    // GCC/Clang: Use atomic built-ins with sequential consistency
    __atomic_store_n(ptr, value, __ATOMIC_SEQ_CST);
#endif
}

uint32_t atomic_fetch_or(atomic_uint32_t *ptr, uint32_t value)
{
#ifdef _WIN32
    // Windows: Use InterlockedOr which returns the original value
    // Note: InterlockedOr takes LONG* which is 32-bit on both x86 and x64
    return (uint32_t)InterlockedOr((volatile LONG *)ptr, (LONG)value);
#elif defined(__GNUC__) || defined(__clang__)
    // GCC/Clang: Use atomic built-ins with sequential consistency
    return __atomic_fetch_or(ptr, value, __ATOMIC_SEQ_CST);
#else
    // Bare metal or unsupported platform: Use critical section
    uint32_t state = critical_section_enter();
    uint32_t old_value = *ptr;
    *ptr = old_value | value;
    critical_section_exit(state);
    return old_value;
#endif
}

uint32_t atomic_fetch_and(atomic_uint32_t *ptr, uint32_t value)
{
#ifdef _WIN32
    // Windows: Use InterlockedAnd which returns the original value
    return (uint32_t)InterlockedAnd((volatile LONG *)ptr, (LONG)value);
#elif defined(__GNUC__) || defined(__clang__)
    // GCC/Clang: Use atomic built-ins with sequential consistency
    return __atomic_fetch_and(ptr, value, __ATOMIC_SEQ_CST);
#else
    // Bare metal or unsupported platform: Use critical section
    uint32_t state = _critical_section_enter();
    uint32_t old_value = *ptr;
    *ptr = old_value & value;
    _critical_section_exit(state);
    return old_value;
#endif
}

uint32_t atomic_fetch_add(atomic_uint32_t *ptr, uint32_t value)
{
#ifdef _WIN32
    // Windows: Use InterlockedExchangeAdd which returns the original value
    return (uint32_t)InterlockedExchangeAdd((volatile LONG *)ptr, (LONG)value);
#elif defined(__GNUC__) || defined(__clang__)
    // GCC/Clang: Use atomic built-ins with sequential consistency
    return __atomic_fetch_add(ptr, value, __ATOMIC_SEQ_CST);
#else
    // Bare metal or unsupported platform: Use critical section
    uint32_t state = _critical_section_enter();
    uint32_t old_value = *ptr;
    *ptr = old_value + value;
    _critical_section_exit(state);
    return old_value;
#endif
}

uint32_t atomic_fetch_sub(atomic_uint32_t *ptr, uint32_t value)
{
#ifdef _WIN32
    // Windows: Use InterlockedExchangeAdd with negative value
    return (uint32_t)InterlockedExchangeAdd((volatile LONG *)ptr, -(LONG)value);
#elif defined(__GNUC__) || defined(__clang__)
    // GCC/Clang: Use atomic built-ins with sequential consistency
    return __atomic_fetch_sub(ptr, value, __ATOMIC_SEQ_CST);
#else
    // Bare metal or unsupported platform: Use critical section
    uint32_t state = _critical_section_enter();
    uint32_t old_value = *ptr;
    *ptr = old_value - value;
    _critical_section_exit(state);
    return old_value;
#endif
}

bool socket_subsystem_init(void)
{
#ifdef _WIN32
    WSADATA wsa_data;
    int err = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    return (err == 0);
#else
    // No initialization needed on POSIX systems
    return true;
#endif
}

void socket_subsystem_cleanup(void)
{
#ifdef _WIN32
    WSACleanup();
#else
    // No cleanup needed on POSIX systems
#endif
}

int close_socket(socket_t sock)
{
#ifdef _WIN32
    return closesocket(sock);
#else
    return close(sock);
#endif
}

int shutdown_socket(socket_t sock, int how)
{
#ifdef _WIN32
    // Windows uses SD_RECEIVE (0), SD_SEND (1), SD_BOTH (2)
    return shutdown(sock, how);
#else
    // POSIX uses SHUT_RD (0), SHUT_WR (1), SHUT_RDWR (2)
    return shutdown(sock, how);
#endif
}

int get_socket_error(void)
{
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

bool is_socket_error_wouldblock(void)
{
#ifdef _WIN32
    int error = WSAGetLastError();
    return (error == WSAETIMEDOUT || error == WSAEINTR || error == WSAEWOULDBLOCK);
#else
    return (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR);
#endif
}

int set_socket_reuse_port(socket_t sock)
{
#ifdef _WIN32
    // Windows doesn't have SO_REUSEPORT; SO_REUSEADDR is sufficient
    // This is a no-op on Windows
    (void)sock;
    return 0;
#else
    // On Unix-like systems, set SO_REUSEPORT if available
    #ifdef SO_REUSEPORT
    int reuse = 1;
    return setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (char *)&reuse, sizeof(reuse));
    #else
    // SO_REUSEPORT not available on this platform
    (void)sock;
    return 0;
    #endif
#endif
}

int set_socket_receive_timeout(socket_t sock, uint32_t timeout_ms)
{
#ifdef _WIN32
    // Windows uses DWORD milliseconds for SO_RCVTIMEO
    DWORD timeout = timeout_ms;
    return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
#else
    // POSIX uses struct timeval for SO_RCVTIMEO
    timeval_t timeout;
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;
    return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
#endif
}
