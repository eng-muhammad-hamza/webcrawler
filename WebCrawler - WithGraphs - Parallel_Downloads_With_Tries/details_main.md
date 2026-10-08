# Multithreaded Web Crawler - Main File Explanation

## Overview
This C program implements a multithreaded web crawler that:
1. Downloads content from multiple URLs in parallel
2. Processes the downloaded content using various analysis workers
3. Tracks performance metrics and statistics
4. Outputs results in both human-readable and CSV formats

## Program Structure

### 1. Initialization and Setup
```c
int main(int argc, char *argv[]) {
    // Argument checking and initialization
    SystemMetrics metrics;
    init_metrics(&metrics);
    curl_global_init(CURL_GLOBAL_ALL);
```
- Checks command line arguments (URL file, thread count, chunk size)
- Initializes metrics tracking structure
- Initializes the libcurl library for HTTP operations

### 2. URL Loading Phase
```c
char urls[MAX_URLS][MAX_URL_LEN];
int url_count = read_urls(argv[1], urls, MAX_URLS);
```
- Reads URLs from input file into a 2D array
- Each URL is limited to `MAX_URL_LEN` characters
- Maximum of `MAX_URLS` URLs can be processed

### 3. Download Phase
#### Key Components:
- **DownloadTask struct**: Contains all information needed for a download job
- **MemoryBlock struct**: Stores downloaded content and its size
- **write_callback()**: libcurl callback that handles received data
- **download_thread()**: Worker function for each download thread

#### Process Flow:
1. Allocate memory for storing downloaded content
2. Initialize mutex for thread-safe operations
3. Create a thread for each URL download
   - Each thread gets its own DownloadTask configuration
   - Implements retry logic (up to MAX_RETRIES times)
   - Uses libcurl with various optimizations:
     - Timeouts
     - Speed limits
     - Range requests
     - Error handling
4. Threads store results in shared array (protected by mutex)
5. Main thread waits for all downloads to complete

### 4. Worker Thread Distribution
```c
WorkerFunction workers[] = { ... };
distribute_threads(atoi(argv[2]), worker_count, worker_threads);
```
- Defines available worker functions (9 different analysis types)
- Distributes total threads among different worker types
- Prints thread allocation report

### 5. Task Processing Phase
#### Key Components:
- **TaskStack**: Thread-safe stack for holding tasks
- **SharedStats**: Shared statistics structure
- **Worker functions**: Various text analysis functions

#### Process Flow:
1. Initialize task stacks for each worker type
2. Create tasks for each successful download
   - Each URL gets analyzed by all worker types
   - Tasks are pushed to respective stacks
3. Start worker threads with assigned counts
4. Threads pull tasks from stacks and process them
5. Results are aggregated in SharedStats

### 6. Cleanup and Reporting
```c
// Memory cleanup
// Metrics output
// CSV file generation
```
- Frees all allocated memory
- Destroys mutexes
- Prints final statistics
- Generates two CSV files:
  - Current run metrics
  - Historical metrics (appended)

## Key Data Structures

### MemoryBlock
```c
typedef struct {
    char *memory;   // Pointer to downloaded content
    size_t size;    // Size of downloaded content
} MemoryBlock;
```
- Used by libcurl callback to store downloaded data
- Dynamically grows as data is received

### DownloadTask
```c
typedef struct {
    const char *url;        // URL to download
    char **url_data;        // Array to store results
    int url_index;          // Index in URL array
    size_t max_bytes;       // Download size limit
    SystemMetrics *metrics; // Performance tracking
    pthread_mutex_t *lock;  // Synchronization
    int attempt_count;      // Retry counter
} DownloadTask;
```
- Contains all context needed for a download job
- Passed to each download thread

### SystemMetrics
- Tracks various performance metrics:
  - Timings for each phase
  - Download statistics
  - Worker performance data
- Used for both runtime display and CSV output

## Threading Model

### Download Phase
- One thread per URL
- Independent downloads with retries
- Shared results array protected by mutex

### Processing Phase
- Pool of worker threads
- Threads distributed among worker types
- Work-stealing model from task stacks
- Shared statistics with mutex protection

## Error Handling
- URL loading: File open errors
- Download phase:
  - Memory allocation failures
  - Network errors with retries
  - Timeout handling
- Thread creation: Checks for pthread_create failures

## Performance Optimizations
- Connection timeouts
- Download speed thresholds
- Memory reuse between retries
- Parallel downloads
- Work distribution across CPU cores

## Output System
- Console output with progress reporting
- Detailed statistics display
- CSV output for metrics
- Historical metrics tracking

## Configuration Options
- Maximum URLs to process
- Maximum URL length
- Download retry count
- Thread counts
- Download chunk sizes
- Various timeout values