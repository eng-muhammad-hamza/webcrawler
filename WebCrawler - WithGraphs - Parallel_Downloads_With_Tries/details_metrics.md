# **Metrics Library - Code Explanation**  

## **1. Introduction**  
This code defines a **metrics tracking system** in C, designed to measure and report performance statistics for a multi-threaded or distributed system. It tracks:  
- **Timing metrics** (e.g., total runtime, URL download time).  
- **Worker performance** (e.g., task execution times, min/max/avg durations).  
- **Data statistics** (e.g., bytes downloaded, URLs processed/failed).  

**Primary Use Case**:  
- Benchmarking system performance.  
- Debugging bottlenecks in task execution.  
- Logging runtime statistics for analysis.  

---

## **2. Key Functionality**  
- **Time Tracking**: Measures execution time of different system components.  
- **Worker Metrics Aggregation**: Computes min/max/avg times for tasks.  
- **System-Wide Reporting**: Logs all collected metrics in a structured format.  

---

## **3. Step-by-Step Breakdown**  

### **3.1 Data Structures**  

#### **`TimeMetric`**  
Tracks start/end times and computes duration in milliseconds.  
```c
typedef struct {
    struct timespec start_time;  // High-precision start time (sec + ns)
    struct timespec end_time;    // High-precision end time (sec + ns)
    double duration_ms;          // Computed duration in milliseconds
} TimeMetric;
```  
- **Purpose**: Used for timing discrete operations (e.g., URL downloads).  

#### **`WorkerMetrics`**  
Aggregates performance data for a worker task.  
```c
typedef struct {
    double total_time;   // Cumulative time spent on tasks
    int task_count;     // Number of tasks completed
    double min_time;    // Fastest task execution time
    double max_time;    // Slowest task execution time
} WorkerMetrics;
```  
- **Purpose**: Tracks efficiency of workers (e.g., word counting, palindrome detection).  

#### **`SystemMetrics`**  
Top-level container for all metrics.  
```c
typedef struct {
    TimeMetric total_runtime;           // Total system uptime
    TimeMetric url_download;            // Time spent downloading URLs
    TimeMetric thread_distribution;     // Time to distribute tasks
    TimeMetric thread_execution;        // Time for thread execution
    size_t total_bytes_downloaded;      // Total data downloaded
    int urls_processed;                 // Successful URL fetches
    int urls_failed;                   // Failed URL fetches
    WorkerMetrics word_count_metrics;   // Stats for word counting
    // ... (other worker metrics)
} SystemMetrics;
```  
- **Purpose**: Centralizes all system performance data.  

---

### **3.2 Core Functions**  

#### **`start_timer(TimeMetric *metric)`**  
- **Purpose**: Records the current time as the start of an operation.  
- **Key Line**:  
  ```c
  clock_gettime(CLOCK_MONOTONIC, &metric->start_time);
  ```  
  - Uses `CLOCK_MONOTONIC` for a steady, non-adjustable clock.  

#### **`end_timer(TimeMetric *metric)`**  
- **Purpose**: Stops the timer and computes duration in milliseconds.  
- **Key Calculation**:  
  ```c
  metric->duration_ms = (end_time.tv_sec - start_time.tv_sec) * 1000.0;
  metric->duration_ms += (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0;
  ```  
  - Combines seconds and nanoseconds for high precision.  

#### **`init_worker_metrics(WorkerMetrics *metrics)`**  
- **Purpose**: Resets worker metrics to default values.  
- **Key Line**:  
  ```c
  metrics->min_time = DBL_MAX;  // Ensures first duration becomes the new min
  ```  

#### **`update_worker_metrics(WorkerMetrics *metrics, double duration)`**  
- **Purpose**: Updates stats after a task completes.  
- **Key Logic**:  
  ```c
  metrics->total_time += duration;
  metrics->task_count++;
  if (duration < metrics->min_time) metrics->min_time = duration;
  if (duration > metrics->max_time) metrics->max_time = duration;
  ```  

#### **`print_system_metrics(SystemMetrics *metrics)`**  
- **Purpose**: Logs all collected metrics.  
- **Key Outputs**:  
  - Total runtime.  
  - Worker performance (task count, avg/min/max times).  
  - Data statistics (bytes downloaded, URL success/failure rates).  

---

## **4. Critical Code Lines**  

| **Line** | **Explanation** |  
|----------|----------------|  
| `clock_gettime(CLOCK_MONOTONIC, ...)` | Uses a monotonic clock to avoid time skew. |  
| `metrics->min_time = DBL_MAX` | Ensures the first task sets the initial minimum. |  
| `metric->duration_ms += (end_time.tv_nsec - start_time.tv_nsec) / 1000000.0` | Converts nanoseconds to milliseconds. |  
| `metrics->task_count > 0 ? metrics->total_time / metrics->task_count : 0` | Safely computes average time (avoids division by zero). |  

---

## **5. Dependencies**  
- **Headers**:  
  - `<time.h>` (for `clock_gettime`).  
  - `<float.h>` (for `DBL_MAX`).  
  - `<stdio.h>` (for `printf`).  

---