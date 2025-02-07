#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>


// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;

    // Convert wait_to_obtain_ms to timespec format
    struct timespec ts;
    ts.tv_sec = thread_func_args->wait_to_obtain_ms / 1000;  
    ts.tv_nsec = (thread_func_args->wait_to_obtain_ms % 1000) * 1000000;
    nanosleep(&ts, NULL);

    // Obtain mutex
    if (pthread_mutex_lock(thread_func_args->mutex) != 0) 
    {
        ERROR_LOG("Failed to lock mutex");
        return NULL;
    }

    // Convert wait_to_release_ms to timespec format
    ts.tv_sec = thread_func_args->wait_to_release_ms / 1000;
    ts.tv_nsec = (thread_func_args->wait_to_release_ms % 1000) * 1000000;
    nanosleep(&ts, NULL);

    // Release mutex
    if (pthread_mutex_unlock(thread_func_args->mutex) != 0) 
    {
        ERROR_LOG("Failed to unlock mutex");
        return NULL;
    }

    // Set success flag
    thread_func_args->thread_complete_success = true;

    // Return the thread_data pointer
    return (void*)thread_func_args;
}



bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
    struct thread_data* thread_struct = (struct thread_data*)malloc(sizeof(struct thread_data));
    if (thread_struct == NULL) 
    {
        ERROR_LOG("Memory allocation failed");
        return false;
    }
    //char* thread_data = "Thread Data";
    thread_struct->mutex = mutex;
    thread_struct->wait_to_obtain_ms = wait_to_obtain_ms;
    thread_struct->wait_to_release_ms = wait_to_release_ms;
    thread_struct->thread_complete_success = false;


    int ret = pthread_create(thread, NULL, threadfunc, (void *)thread_struct);
    //pthread_mutex_lock(thread_struct->mutex);
    
    if(ret)
    {
        errno = ret;
        perror("pthread_create");
        //hread_struct->thread_complete_success = false;
        free(thread_struct);
        return false;
    }
    thread_struct->threadID = *thread;
    //thread_struct->thread_complete_success = true;
    //pthread_exit (NULL);

    return true;
}

