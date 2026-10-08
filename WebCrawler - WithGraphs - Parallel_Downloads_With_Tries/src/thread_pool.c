#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "thread_pool.h"

// Distributes total_threads as evenly as possible among worker_count workers.
// The distribution array will contain the number of threads assigned to each worker.
void distribute_threads(int total_threads, int worker_count, int distribution[])
{
    int base = total_threads / worker_count;    // Minimum threads each worker will get
    int rem = total_threads % worker_count;     // Remaining threads to distribute one by one

    if (base >= 1)                             // If each worker can get at least one thread
    {
        for (int i = 0; i < worker_count; i++)
        {
            distribution[i] = base;            // Assign base threads to each worker
        }

        int i = 0;
        while (rem != 0)
        {
            distribution[i]++;                 // Distribute remaining threads one by one to workers
            i++;
            rem--;
            if (i >= worker_count)
                i = 0;                        // Loop back to the first worker if needed
        }
    }
    else
    {                                          // If total_threads < worker_count, assign one thread to each worker
        for (int i = 0; i < worker_count; i++)
        {
            distribution[i] = 1;               // Assign one thread to each worker
        }
    }
}

// Initializes a TaskStack with a given capacity.
void init_task_stack(TaskStack *stack, int capacity)
{
    stack->tasks = malloc(sizeof(ThreadTask) * capacity); // Allocate memory for tasks array
    stack->count = 0;                                     // Initialize task count to zero
    stack->capacity = capacity;                           // Set the maximum number of tasks
    pthread_mutex_init(&stack->lock, NULL);               // Initialize mutex for thread safety
}

// Pushes a new task onto the stack in a thread-safe manner.
void push_task(TaskStack *stack, ThreadTask task)
{
    pthread_mutex_lock(&stack->lock);                     // Lock the stack for exclusive access
    if (stack->count < stack->capacity)                   // Check if there is space in the stack
    {
        stack->tasks[stack->count++] = task;              // Add the task and increment the count
    }
    pthread_mutex_unlock(&stack->lock);                   // Unlock the stack
}

// Pops a task from the stack in a thread-safe manner.
// Returns 1 if a task was popped, 0 if the stack was empty.
int pop_task(TaskStack *stack, ThreadTask *task)
{
    pthread_mutex_lock(&stack->lock);                     // Lock the stack for exclusive access
    int result = 0;                                       // Default: no task popped
    if (stack->count > 0)
    {
        *task = stack->tasks[--stack->count];             // Pop the last task and decrement count
        result = 1;                                       // Indicate success
    }
    pthread_mutex_unlock(&stack->lock);                   // Unlock the stack
    return result;
}

// Worker thread function that processes tasks from the stack until empty.
void *worker_thread(void *arg)
{
    TaskStack *stack = (TaskStack *)arg;                  // Cast argument to TaskStack pointer
    ThreadTask task;                                      // Temporary variable to hold a task

    while (pop_task(stack, &task))                        // Keep popping tasks until stack is empty
    {
        task.worker_fn(&task);                            // Execute the worker function for the task
    }

    return NULL;                                          // Thread exits when no tasks remain
}

// Launches worker threads for each stack according to threads_per_stack distribution.
void run_worker_threads(TaskStack *stacks, int num_stacks, const int threads_per_stack[])
{
    pthread_t *threads = NULL;                            // Array to hold thread IDs
    int total_threads = 0;                                // Total number of threads to create

    // Calculate total number of threads needed
    for (int i = 0; i < num_stacks; i++)
    {
        total_threads += threads_per_stack[i];
    }

    threads = malloc(sizeof(pthread_t) * total_threads);  // Allocate memory for thread IDs
    int thread_index = 0;                                 // Index for threads array

    // Create threads for each stack as specified in threads_per_stack
    for (int i = 0; i < num_stacks; i++)
    {
        for (int j = 0; j < threads_per_stack[i]; j++)
        {
            if (pthread_create(&threads[thread_index++], NULL, worker_thread, &stacks[i]))
            {
                perror("pthread_create");                 // Print error if thread creation fails
            }
        }
    }

    // Wait for all threads to finish
    for (int i = 0; i < total_threads; i++)
    {
        pthread_join(threads[i], NULL);                   // Join each thread
    }

    free(threads);                                        // Free memory allocated for thread IDs
}