# **Thread Pool Implementation with Task Stack - Code Explanation**

## **1. Introduction**
This implementation provides a thread pool that uses a **stack-based (LIFO) task management system** for parallel task execution. Key features:

- **Stack-based task storage** (Last-In-First-Out)
- **Thread-safe operations** using mutex locks
- **Dynamic thread distribution** across multiple task stacks
- **Worker function delegation** via function pointers

## **2. Key Data Structures**

### **ThreadTask**
```c
typedef struct {
    const char *data;           // Input data for processing
    SharedStats *stats;         // Shared result storage
    WorkerFunction worker_fn;   // Function to execute
    int url_index;              // Task identifier
} ThreadTask;
```
- Represents a single unit of work
- Contains data + worker function to process it

### **TaskStack**
```c
typedef struct {
    ThreadTask *tasks;          // Array storing tasks
    int count;                  // Current task count
    int capacity;               // Maximum capacity
    pthread_mutex_t lock;       // Synchronization mutex
} TaskStack;
```
- Implements a thread-safe LIFO stack
- Uses mutex to prevent race conditions

## **3. Core Functions**

### **Thread Distribution**
```c
void distribute_threads(int total_threads, int worker_count, int distribution[])
```
- Evenly distributes threads across workers
- Handles two cases:
  1. When threads ≥ workers (base + remainder distribution)
  2. When threads < workers (1 thread per worker)

### **Stack Operations**
```c
void init_task_stack(TaskStack *stack, int capacity)
```
- Allocates memory for tasks
- Initializes mutex for thread safety

```c
void push_task(TaskStack *stack, ThreadTask task)
```
- Thread-safe insertion
- Locks mutex → adds task → unlocks mutex

```c
int pop_task(TaskStack *stack, ThreadTask *task)
```
- Thread-safe removal (LIFO)
- Returns 1 (success) or 0 (empty)

### **Thread Management**
```c
void* worker_thread(void *arg)
```
- Worker thread entry point
- Continuously pops and executes tasks until stack is empty

```c
void run_worker_threads(TaskStack *stacks, int num_stacks, const int threads_per_stack[])
```
1. Calculates total threads needed
2. Creates threads for each stack
3. Joins all threads for completion

## **4. Critical Code Analysis**

### **LIFO Behavior**
```c
*stack->tasks[--stack->count]  // In pop_task()
```
- Removes most recently added task first
- More efficient than FIFO (no element shifting)

### **Thread Safety**
```c
pthread_mutex_lock(&stack->lock);
// Critical section
pthread_mutex_unlock(&stack->lock);
```
- Protects all stack operations
- Ensures only one thread accesses stack at a time

### **Dynamic Worker Assignment**
```c
if (pthread_create(&threads[thread_index++], NULL, worker_thread, &stacks[i]))
```
- Creates specified number of threads per stack
- Threads automatically balance work via shared stack