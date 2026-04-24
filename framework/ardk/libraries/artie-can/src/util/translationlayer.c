/**
 * @file translationlayer.c
 * @brief Platform-independent wrapper implementations for OS-specific functionality.
 *
 */

#include "translationlayer.h"

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
