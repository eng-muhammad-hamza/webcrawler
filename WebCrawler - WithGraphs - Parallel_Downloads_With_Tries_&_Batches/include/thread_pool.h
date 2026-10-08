#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include "workers.h"

// Making a datatype alias of worker functions of threads
typedef void* (*WorkerFunction)(void*);

// Structure to hold the thread task data
typedef struct {
    const char *data;                                           // Data to be processed
    SharedStats *stats;                                         // Shared Stats to hold the end data
    WorkerFunction worker_fn;                                   // The Worker Function to Run
    int url_index;                                              // To track which URL this task belongs to
} ThreadTask;

// A Queue Data holder Structure
typedef struct {
    ThreadTask *tasks;                                          // Thread Tasks Pointer
    int count;                                                  // Count to hold no of tasks per queue
    int capacity;                                               // Max Capacity to hold the no of tasks, must be qual to url count for safe check
    pthread_mutex_t lock;                                       // Queue Lock for thread safe addition of tasks
} TaskQueue;

void distribute_threads(int total_threads, int worker_count, int distribution[]);               // No of Threads Distributor
void init_task_queue(TaskQueue *queue, int capacity);                                           // Queue Initializer
void enqueue_task(TaskQueue *queue, ThreadTask task);                                           // Queue Task Adder
int dequeue_task(TaskQueue *queue, ThreadTask *task);                                           // Queue Taks Remover
void* worker_thread(void *arg);                                                                 // A dummy thread function holder which dynamically delegates its functionality to actual worker function
void run_worker_threads(TaskQueue *queues, int num_queues, const int threads_per_queue[]);      // Worker Function Runner

#endif