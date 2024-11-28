#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    /** Wait, obtain mutex, wait, release mutex as described by thread_data structure
     * HINT: use a cast like the one below to obtain thread arguments from your parameter
     */

    struct thread_data* pthread_data = (struct thread_data *) thread_param;
    DEBUG_LOG("Sleeping for %d ms", pthread_data->wait_to_obtain_ms);
    usleep(pthread_data->wait_to_obtain_ms*1000);
    DEBUG_LOG("Locking mutex");
    pthread_mutex_lock(pthread_data->mutex);
    DEBUG_LOG("Sleeping for %d ms", pthread_data->wait_to_release_ms);
    usleep(pthread_data->wait_to_release_ms*100);
    DEBUG_LOG("Unlocking mutex");
    pthread_mutex_unlock(pthread_data->mutex);

    pthread_data->thread_complete_success = true;

    return (void *) pthread_data;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * Allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    struct thread_data *pthread_data = malloc(sizeof(struct thread_data));

    pthread_data->mutex = mutex;
    pthread_data->wait_to_obtain_ms = wait_to_obtain_ms;
    pthread_data->wait_to_release_ms = wait_to_release_ms;
    pthread_data->thread_complete_success = false;

    int rc = pthread_create(thread, NULL, threadfunc, pthread_data);
    if (rc == 0) {
        DEBUG_LOG("Thread created.");
        // free(pthread_data);  Free it after thread_join();
        return true;
    }

    ERROR_LOG("Failed to start thread. %s", strerror(rc));
    free(pthread_data);
    return false;
}

