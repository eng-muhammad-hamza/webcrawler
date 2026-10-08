#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "workers.h"

// Define a function pointer type for worker functions executed by threads.
// Each worker function takes a void* argument and returns a void*.
typedef void* (*WorkerFunction)(void*);

// Structure representing a single task to be executed by a thread.
typedef struct {
    const char *data;           // Pointer to the data that the worker will process.
    SharedStats *stats;         // Pointer to shared statistics structure for collecting results.
    WorkerFunction worker_fn;   // Function pointer to the worker function to execute.
    int url_index;              // Index indicating which URL this task is associated with.
} ThreadTask;

// Structure representing a stack (LIFO) of thread tasks, used for task management.
typedef struct {
    ThreadTask *tasks;          // Dynamic array of ThreadTask structures.
    int count;                  // Current number of tasks in the stack.
    int capacity;               // Maximum number of tasks the stack can hold.
    pthread_mutex_t lock;       // Mutex to ensure thread-safe access to the stack.
} TaskStack;

// Distributes the total number of threads among a given number of workers.
// Fills the 'distribution' array with the number of threads assigned to each worker.
void distribute_threads(int total_threads, int worker_count, int distribution[]);

// Initializes a TaskStack with the specified capacity and sets up its mutex.
void init_task_stack(TaskStack *stack, int capacity);

// Adds a new task to the stack in a thread-safe manner.
void push_task(TaskStack *stack, ThreadTask task);

// Removes a task from the stack in a thread-safe manner.
// Returns 1 if a task was popped, 0 if the stack was empty.
int pop_task(TaskStack *stack, ThreadTask *task);

// Generic worker thread function that delegates work to the actual worker function.
// The argument is expected to be a pointer to a ThreadTask.
void* worker_thread(void *arg);

// Launches worker threads for each task stack, assigning the specified number of threads per stack.
void run_worker_threads(TaskStack *stacks, int num_stacks, const int threads_per_stack[]);

#endif