#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <unistd.h>
#include "workers.h"
#include "thread_pool.h"
#include "metrics.h"

// Maximum number of URLs to process
#define MAX_URLS 100
// Maximum length of each URL string
#define MAX_URL_LEN 1024

#define MAX_DOWNLOAD_THREADS 15  // Optimal concurrent connections
#define MAX_RETRIES 3           // Max download attempts

// Structure to hold downloaded memory data and its size
typedef struct {
    char *memory;       // Pointer to the downloaded data
    size_t size;        // Size of the downloaded data
} MemoryBlock;

// Enhanced DownloadTask structure
typedef struct {
    const char *url;
    char **url_data;
    int url_index;
    size_t max_bytes;
    SystemMetrics *metrics;
    pthread_mutex_t *lock;
    int attempt_count;
} DownloadTask;

// Callback function for libcurl to write downloaded data into memory
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;                 // Calculate actual size of incoming data
    MemoryBlock *mem = (MemoryBlock *)userp;
    // Reallocate memory to fit new data
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) return 0;                             // If allocation fails, return 0 to signal error
    mem->memory = ptr;
    // Copy new data into the memory block
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;                     // Null-terminate the string
    return realsize;                                // Return number of bytes handled
}

// Reads URLs from a file into the provided array, up to max_urls
int read_urls(const char *filepath, char urls[][MAX_URL_LEN], int max_urls) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        perror("[ERROR] Cannot open URL file");
        return 0;
    }
    int count = 0;
    // Read each line (URL) from the file
    while (fgets(urls[count], MAX_URL_LEN, fp) && count < max_urls) {
        // Remove newline characters from the end
        urls[count][strcspn(urls[count], "\r\n")] = 0;
        count++;
    }
    fclose(fp);
    return count;                                           // Return the number of URLs read
}


void* download_thread(void *arg) {
    DownloadTask *task = (DownloadTask *)arg;
    CURL *curl_handle = NULL;
    MemoryBlock chunk = {0}; // Initialize to zero
    CURLcode res = CURLE_OK;

    // Buffer for the range string (e.g., "0-999999999" + null terminator)
    // A size_t can be large, ensure buffer is sufficient. `snprintf` will handle overflow.
    char range_str[64]; // Should be enough for "0-<SIZE_T_MAX>"

    for (task->attempt_count = 0; task->attempt_count < MAX_RETRIES; task->attempt_count++) {
        chunk.memory = malloc(1); // Start with 1 byte for initial allocation
        if (!chunk.memory) {
            fprintf(stderr, "[ERROR] Memory allocation failed for %s (attempt %d)\n",
                    task->url, task->attempt_count + 1);
            // Consider a small delay before next attempt, or just continue
            sleep(1); // Small delay before retrying
            continue;
        }
        chunk.size = 0;

        // Initialize CURL handle
        curl_handle = curl_easy_init();
        if (!curl_handle) {
            free(chunk.memory);
            fprintf(stderr, "[ERROR] curl_easy_init failed for %s (attempt %d)\n",
                    task->url, task->attempt_count + 1);
            sleep(1); // Small delay before retrying
            continue;
        }

        // --- NEW: Set CURLOPT_RANGE based on task->max_bytes ---
        if (task->max_bytes > 0) {
            // Construct the range string "0-N" where N is max_bytes - 1
            int written = snprintf(range_str, sizeof(range_str), "0-%zu", task->max_bytes - 1);
            if (written < 0 || (size_t)written >= sizeof(range_str)) {
                fprintf(stderr, "[ERROR] Failed to format range string or buffer too small for %s\n", task->url);
                curl_easy_cleanup(curl_handle);
                free(chunk.memory);
                sleep(1); // Small delay before retrying
                continue; // Treat as an error and retry
            }
            curl_easy_setopt(curl_handle, CURLOPT_RANGE, range_str);
        }
        // --- END NEW ---

        // Set other CURL options
        curl_easy_setopt(curl_handle, CURLOPT_URL, task->url);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "Multithreaded Web Crawler");
        curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 15L); // Increased from 10 to 15 seconds
        curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 5L);
        curl_easy_setopt(curl_handle, CURLOPT_LOW_SPEED_LIMIT, 8192L); // 8KB/sec
        curl_easy_setopt(curl_handle, CURLOPT_LOW_SPEED_TIME, 20L);    // Must maintain for 20 seconds
        curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1L);        // Fail on HTTP 4xx/5xx errors
        curl_easy_setopt(curl_handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
        curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L); // Follow HTTP 3xx redirections

        // Perform download
        res = curl_easy_perform(curl_handle);

        if (res == CURLE_OK) {
            // Success - process the data
            // We've requested a specific range. If the server is compliant, chunk.size should be <= max_bytes.
            // This `if` block below is a client-side safeguard if the server somehow sends more data than requested
            // (e.g., if it ignores the Range header).
            // For a truly exact `max_bytes` limit, you could also check `chunk.size` in `write_callback`
            // and return 0 if it exceeds `task->max_bytes`.
            if (chunk.size > task->max_bytes && task->max_bytes > 0) {
                 // Truncate the memory and size to exactly max_bytes
                chunk.memory[task->max_bytes] = '\0';
                chunk.size = task->max_bytes;
            }
            break; // Exit retry loop on success
        }

        // Cleanup for failed attempt
        curl_easy_cleanup(curl_handle);
        curl_handle = NULL; // Reset handle for next attempt
        if (chunk.memory) {
            free(chunk.memory);
            chunk.memory = NULL;
        }

        // Log failure
        fprintf(stderr, "[WARNING] Attempt %d failed for %s: %s\n",
                task->attempt_count + 1, task->url, curl_easy_strerror(res));
        sleep(2); // Wait before retrying (back-off strategy)
    }

    // Process final result outside the loop
    pthread_mutex_lock(task->lock);
    if (res == CURLE_OK) {
        printf("[DOWNLOAD] %03d: %-50s [OK] %zu bytes (%d attempts)\n",
               task->url_index + 1, task->url, chunk.size, task->attempt_count + 1);

        // Store the downloaded data
        task->url_data[task->url_index] = chunk.memory;
        task->metrics->urls_processed++;
        task->metrics->total_bytes_downloaded += chunk.size;
    } else {
        fprintf(stderr, "[ERROR] Final attempt failed for %s: %s\n",
                task->url, curl_easy_strerror(res));
        task->metrics->urls_failed++;
        if (chunk.memory) { // Free memory if it was allocated but download failed
            free(chunk.memory);
        }
        task->url_data[task->url_index] = NULL; // Ensure pointer is NULL on failure
    }
    pthread_mutex_unlock(task->lock);

    if (curl_handle) { // Clean up curl handle if it was initialized
        curl_easy_cleanup(curl_handle);
    }
    return NULL;
}

// --- Download Manager (unchanged, just calling the modified download_thread) ---
// Download manager with thread pool
void download_urls_parallel(char urls[][MAX_URL_LEN], char *url_data[], int url_count,
                            size_t max_bytes, SystemMetrics *metrics) {
    if (url_count <= 0 || !urls || !url_data || !metrics) {
        fprintf(stderr, "[ERROR] Invalid parameters to download_urls_parallel\n");
        return;
    }

    // Initialize all url_data pointers to NULL
    for (int i = 0; i < url_count; i++) {
        url_data[i] = NULL;
    }

    pthread_t thread_pool[MAX_DOWNLOAD_THREADS];
    DownloadTask *tasks = malloc(sizeof(DownloadTask) * url_count);
    pthread_mutex_t lock;
    pthread_mutex_init(&lock, NULL);

    // Initialize tasks
    for (int i = 0; i < url_count; i++) {
        tasks[i] = (DownloadTask){
            .url = urls[i],
            .url_data = url_data,
            .url_index = i,
            .max_bytes = max_bytes, // This is where max_bytes from main is passed to the task
            .metrics = metrics,
            .lock = &lock,
            .attempt_count = 0
        };
    }

    // Process URLs in batches using thread pool
    for (int i = 0; i < url_count; i += MAX_DOWNLOAD_THREADS) {
        int batch_size = (url_count - i) < MAX_DOWNLOAD_THREADS ?
                         (url_count - i) : MAX_DOWNLOAD_THREADS;

        // Create threads for current batch
        for (int j = 0; j < batch_size; j++) {
            if (pthread_create(&thread_pool[j], NULL, download_thread, &tasks[i + j])) {
                perror("pthread_create");
                // If thread creation fails, mark URL as failed and ensure no data pointer is set
                tasks[i + j].url_data[i + j] = NULL;
                pthread_mutex_lock(&lock);
                metrics->urls_failed++;
                pthread_mutex_unlock(&lock);
            }
        }

        // Wait for current batch to complete
        for (int j = 0; j < batch_size; j++) {
            pthread_join(thread_pool[j], NULL);
        }
    }

    pthread_mutex_destroy(&lock);
    free(tasks);
}

int main(int argc, char *argv[]) {
    // Check for correct number of command-line arguments
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <urls.txt> <num_threads> <chunk_bytes>\n", argv[0]);
        return 1;
    }
    size_t chunk_bytes = (size_t)atol(argv[3]); // Use atol for long, cast to size_t
    // Initialize system metrics for performance tracking
    SystemMetrics metrics;
    init_metrics(&metrics);
    printf("\n[SYSTEM] Starting Multithreaded Web Crawler\n");
    printf("[CONFIG] Threads: %d, Max bytes per URL: %s\n", atoi(argv[2]), argv[3]);
    // Initialize global curl state
    curl_global_init(CURL_GLOBAL_ALL);
    // Array to store URLs read from file
    char urls[MAX_URLS][MAX_URL_LEN];
    int url_count = read_urls(argv[1], urls, MAX_URLS);
    printf("[SYSTEM] Loaded %d URLs to process\n", url_count);

    // Download phase
    printf("\n[PHASE] Downloading URL content\n");
    start_timer(&metrics.url_download);

    char **url_data = malloc(sizeof(char*) * url_count);
    download_urls_parallel(urls, url_data, url_count, chunk_bytes, &metrics);

    end_timer(&metrics.url_download);
    printf("[STATUS] Downloads completed: %d success, %d failed\n",
        metrics.urls_processed, metrics.urls_failed);

    // The rest of the code remains unchanged...
    // Thread distribution phase: assign threads to worker functions
    printf("\n[PHASE] Distributing %d threads\n", atoi(argv[2]));
    start_timer(&metrics.thread_distribution);
    // Array of worker function pointers for different analysis tasks
    WorkerFunction workers[] = {
        worker_word_count,
        worker_top_words,
        worker_longest_word,
        worker_sentence_count,
        worker_char_frequency,
        worker_palindrome_detector,
        worker_word_length_dist,
        worker_word_start_char,
        worker_avg_word_length
    };
    int worker_count = sizeof(workers) / sizeof(workers[0]);
    int worker_threads[worker_count];
    // Distribute the total number of threads among the worker functions
    distribute_threads(atoi(argv[2]), worker_count, worker_threads);
    end_timer(&metrics.thread_distribution);
    printf("[THREADS] Distribution:\n");
    // Print the thread distribution for each worker
    for (int i = 0; i < worker_count; i++) {
        const char *worker_name;
        if (i == 0) worker_name = "word_count";
        else if (i == 1) worker_name = "top_words";
        else if (i == 2) worker_name = "longest_word";
        else if (i == 3) worker_name = "sentence_count";
        else if (i == 4) worker_name = "char_frequency";
        else if (i == 5) worker_name = "palindrome_det";
        else if (i == 6) worker_name = "word_len_dist";
        else if (i == 7) worker_name = "word_start_char";
        else worker_name = "avg_word_len";
        printf("  %-15s: %d threads\n", worker_name, worker_threads[i]);
    }
    // Task processing phase: create and enqueue tasks for each worker
    printf("\n[PHASE] Processing tasks\n");
    SharedStats stats;
    init_shared_stats(&stats, &metrics);
    // Create a task queue for each worker type
    TaskQueue queues[worker_count];
    for (int i = 0; i < worker_count; i++) {
        init_task_queue(&queues[i], url_count);
    }
    // For each downloaded URL, enqueue a task for every worker
    for (int url_idx = 0; url_idx < url_count; url_idx++) {
        if (url_data[url_idx] != NULL) {  // Only enqueue tasks for successfully downloaded URLs
            for (int worker_idx = 0; worker_idx < worker_count; worker_idx++) {
                ThreadTask task = {
                    .data = url_data[url_idx],
                    .stats = &stats,
                    .worker_fn = workers[worker_idx],
                    .url_index = url_idx
                };
                enqueue_task(&queues[worker_idx], task);
            }
        }
    }
    // Start worker threads to process the tasks
    start_timer(&metrics.thread_execution);
    run_worker_threads(queues, worker_count, worker_threads);
    end_timer(&metrics.thread_execution);
    printf("[STATUS] All tasks completed\n");

    // Cleanup: free downloaded data and destroy queues
    for (int i = 0; i < url_count; i++) {
        if (url_data[i] != NULL) {
            free(url_data[i]);
            url_data[i] = NULL;  // Good practice
        }
    }
    free(url_data);
    url_data = NULL;

    for (int i = 0; i < worker_count; i++) {
        free(queues[i].tasks);
        pthread_mutex_destroy(&queues[i].lock);
    }
    // Print final statistics and metrics
    printf("\n=== Final Results ===\n");
    print_statistics(&stats);
    print_system_metrics(&metrics);
    // Save metrics to a CSV file for visualization
    FILE *csv = fopen("metrics.csv", "w");
    if (csv) {
        fprintf(csv, "metric,value\n");
        fprintf(csv, "total_runtime,%.2f\n", metrics.total_runtime.duration_ms);
        fprintf(csv, "url_download,%.2f\n", metrics.url_download.duration_ms);
        fprintf(csv, "thread_distribution,%.2f\n", metrics.thread_distribution.duration_ms);
        fprintf(csv, "thread_execution,%.2f\n", metrics.thread_execution.duration_ms);
        fprintf(csv, "word_count_total,%.2f\n", metrics.word_count_metrics.total_time);
        fprintf(csv, "top_words_total,%.2f\n", metrics.top_words_metrics.total_time);
        fprintf(csv, "longest_word_total,%.2f\n", metrics.longest_word_metrics.total_time);
        fprintf(csv, "sentence_count_total,%.2f\n", metrics.sentence_count_metrics.total_time);
        fprintf(csv, "char_freq_total,%.2f\n", metrics.char_freq_metrics.total_time);
        fprintf(csv, "palindrome_total,%.2f\n", metrics.palindrome_metrics.total_time);
        fprintf(csv, "word_len_dist_total,%.2f\n", metrics.word_len_dist_metrics.total_time);
        fprintf(csv, "word_start_char_total,%.2f\n", metrics.word_start_char_metrics.total_time);
        fprintf(csv, "avg_word_len_total,%.2f\n", metrics.avg_word_len_metrics.total_time);
        fprintf(csv, "urls_processed,%d\n", metrics.urls_processed);
        fprintf(csv, "urls_failed,%d\n", metrics.urls_failed);
        fprintf(csv, "total_bytes,%zu\n", metrics.total_bytes_downloaded);
        fclose(csv);
    }
    // Append metrics to a history file for tracking over time
    FILE *history = fopen("metrics_history.csv", "a");
    if (history) {
        // If file is empty, write header
        if (ftell(history) == 0) {
            fprintf(history, "timestamp,total_runtime,url_download,thread_distribution,"
                    "thread_execution,word_count_total,top_words_total,longest_word_total,"
                    "sentence_count_total,char_freq_total,palindrome_total,"
                    "word_len_dist_total,word_start_char_total,avg_word_len_total,"
                    "urls_processed,urls_failed,total_bytes\n");
        }
        // Write current timestamp and metrics
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        fprintf(history, "\"%04d-%02d-%02d %02d:%02d:%02d\",%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%d,%zu\n",
                t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                t->tm_hour, t->tm_min, t->tm_sec,
                metrics.total_runtime.duration_ms,
                metrics.url_download.duration_ms,
                metrics.thread_distribution.duration_ms,
                metrics.thread_execution.duration_ms,
                metrics.word_count_metrics.total_time,
                metrics.top_words_metrics.total_time,
                metrics.longest_word_metrics.total_time,
                metrics.sentence_count_metrics.total_time,
                metrics.char_freq_metrics.total_time,
                metrics.palindrome_metrics.total_time,
                metrics.word_len_dist_metrics.total_time,
                metrics.word_start_char_metrics.total_time,
                metrics.avg_word_len_metrics.total_time,
                metrics.urls_processed,
                metrics.urls_failed,
                metrics.total_bytes_downloaded);
        fclose(history);
    }
    // Free shared statistics and cleanup curl
    free_shared_stats(&stats);
    curl_global_cleanup();
    return 0;
}