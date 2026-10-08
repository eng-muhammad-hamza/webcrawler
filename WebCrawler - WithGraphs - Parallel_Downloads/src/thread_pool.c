#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "thread_pool.h"

// Threads Distributer Functions
void distribute_threads(int total_threads, int worker_count, int distribution[])
{
    int base = total_threads / worker_count;                // Get base no of threads ensuring equal distribution
    int rem = total_threads % worker_count;                 // Gets the extra remaining threads

    if (base >= 1)                                          // Check if the base no of thread is more than 1
    {
        for (int i = 0; i < worker_count; i++)
        {
            distribution[i] = base;                         // Distribute the base thread to every worker function
        }

        int i = 0;
        while (rem != 0)
        {
            distribution[i]++;                              // Distributing the remaining threads to any possible worker function in sequence
            i++;
            rem--;
            if (i >= worker_count)
                i = 0;
        }
    }
    else
    {                                                       // If the base no of thread is less than 1 then that means we only want 1,2, or 3 threads in program
        for (int i = 0; i < worker_count; i++)
        {
            distribution[i] = 1;                            // Distribute 1 thread to each worker function
        }
    }
}

// Initialize the Queue of Thread tasks
void init_task_queue(TaskQueue *queue, int capacity)
{
    queue->tasks = malloc(sizeof(ThreadTask) * capacity);   // Allocate Memory dynamically on runtime
    queue->count = 0;                                       // Initialize Count of tasks to zero
    queue->capacity = capacity;                             // Set the max capacity of the queue
    pthread_mutex_init(&queue->lock, NULL);                 // Initialize the queue lock
}

// Add the task to the queue
void enqueue_task(TaskQueue *queue, ThreadTask task)
{
    pthread_mutex_lock(&queue->lock);                       // Acquire Lock
    if (queue->count < queue->capacity)                     // Checking the overflow error
    {
        queue->tasks[queue->count++] = task;                // Adds the Required task to the queue
    }
    pthread_mutex_unlock(&queue->lock);                     // Release Lock
}

// Extract the tasks from the queue to process it
int dequeue_task(TaskQueue *queue, ThreadTask *task)
{
    pthread_mutex_lock(&queue->lock);                       // Acquir Lock
    int result = 0;                                         // '0' Unsuccess
    if (queue->count > 0)
    {
        *task = queue->tasks[--queue->count];               // Extract the task (the worker function) and decrement count of tasks
        result = 1;                                         // '1' Success
    }
    pthread_mutex_unlock(&queue->lock);                     // Release Lock
    return result;
}

// Worker Thread Holder
void *worker_thread(void *arg)
{
    TaskQueue *queue = (TaskQueue *)arg;                    // Extract and typecast the arguments
    ThreadTask task;                                        // Task holder variable

    while (dequeue_task(queue, &task))                      // If '1' means the task is extracted
    {
        task.worker_fn(&task);                              // Run the worker function
    }

    return NULL;                                            // Other wise return null if the queue is empty
}

// Runner Function -- Threads_per_queue is equal to the distributed worker threads
void run_worker_threads(TaskQueue *queues, int num_queues, const int threads_per_queue[])
{
    pthread_t *threads = NULL;                                                                  // Thread IDs holder
    int total_threads = 0;                                                                      // Total No of Threads to be created

    // Calculate total threads needed
    for (int i = 0; i < num_queues; i++)
    {
        total_threads += threads_per_queue[i];
    }

    threads = malloc(sizeof(pthread_t) * total_threads);                                        // Create Thread IDs for total no of threads dynamically
    int thread_index = 0;                                                                       // Index of the Current Thread

    // Create threads for each queue
    for (int i = 0; i < num_queues; i++)
    {
        // Iterate till the no of threads per queue means till number represented by the worker_thread array's ith index
        for (int j = 0; j < threads_per_queue[i]; j++)
        {
            if (pthread_create(&threads[thread_index++], NULL, worker_thread, &queues[i]))      // Create the Worker Threads
            {
                perror("pthread_create");
            }
        }
    }

    // Join all threads
    for (int i = 0; i < total_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }

    free(threads);                                                                              // Free Up the Memory
}