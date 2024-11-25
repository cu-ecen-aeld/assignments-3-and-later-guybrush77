#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    struct thread_data* ptr_data = (struct thread_data *)thread_param;

    usleep(1000 * ptr_data->wait_to_obtain_ms);

    if (0 != pthread_mutex_lock(ptr_data->mutex)) {
        ptr_data->thread_complete_success = false;
        return thread_param;
    }

    usleep(1000 * ptr_data->wait_to_release_ms);

    if (0 !=  pthread_mutex_unlock(ptr_data->mutex)) {
        ptr_data->thread_complete_success = false;
        return thread_param;
    }

    ptr_data->thread_complete_success = true;

    return thread_param;
}

bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex, int wait_to_obtain_ms, int wait_to_release_ms)
{
    struct thread_data* ptr_data = malloc(sizeof(struct thread_data));

    if (ptr_data == NULL) {
        return false;
    }

    ptr_data->mutex = mutex;
    ptr_data->wait_to_obtain_ms = wait_to_obtain_ms;
    ptr_data->wait_to_release_ms = wait_to_release_ms;
    ptr_data->thread_complete_success = false;

    if (0 != pthread_create(thread, NULL, threadfunc, ptr_data)) {
        free(ptr_data);
        return false;
    }
    return true;
}

