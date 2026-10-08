
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "workers.h"
#include "thread_pool.h"
#include "metrics.h"

// Maximum number of URLs to process
#define MAX_URLS 100
// Maximum length of each URL string
#define MAX_URL_LEN 1024

// Structure to hold downloaded memory data and its size
typedef struct {
    char *memory;       // Pointer to the downloaded data
    size_t size;        // Size of the downloaded data
} MemoryBlock;

// Structure for URL download task
typedef struct {
    const char *url;            // URL to download
    char **url_data;            // Pointer to where to store the result
    int url_index;              // Index of this URL
    size_t max_bytes;           // Maximum bytes to download
    SystemMetrics *metrics;     // Metrics to update
    pthread_mutex_t *lock;      // Mutex for thread-safe metrics updates
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

void* download_thread(void *arg) {
    DownloadTask *task = (DownloadTask *)arg;
    CURL *curl_handle;
    CURLcode res;
    MemoryBlock chunk;
    chunk.memory = malloc(1);  // Start with 1 byte
    chunk.size = 0;

    if (!chunk.memory) {
        pthread_mutex_lock(task->lock);
        task->metrics->urls_failed++;
        fprintf(stderr, "[ERROR] Memory allocation failed for %s\n", task->url);
        pthread_mutex_unlock(task->lock);
        return NULL;
    }

    curl_handle = curl_easy_init();
    if (!curl_handle) {
        free(chunk.memory);
        pthread_mutex_lock(task->lock);
        task->metrics->urls_failed++;
        fprintf(stderr, "[ERROR] curl_easy_init failed for %s\n", task->url);
        pthread_mutex_unlock(task->lock);
        return NULL;
    }

    // Set curl options
    curl_easy_setopt(curl_handle, CURLOPT_URL, task->url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "Multithreaded Web Crawler");
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1L);  // Fail on HTTP errors
    curl_easy_setopt(curl_handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);  // Use HTTP/1.1

    res = curl_easy_perform(curl_handle);
    curl_easy_cleanup(curl_handle);

    pthread_mutex_lock(task->lock);
    if (res != CURLE_OK) {
        fprintf(stderr, "[ERROR] Failed to download %s: %s\n", task->url, curl_easy_strerror(res));
        task->metrics->urls_failed++;
        free(chunk.memory);
    } else {
        // Truncate data if needed
        if (chunk.size > task->max_bytes) {
            chunk.memory[task->max_bytes] = '\0';
            chunk.size = task->max_bytes;
        } else {
            chunk.memory[chunk.size] = '\0';
        }

        printf("[DOWNLOAD] %03d: %-50s [OK] %zu bytes\n", 
               task->url_index + 1, task->url, chunk.size);
        
        task->url_data[task->url_index] = chunk.memory;
        task->metrics->urls_processed++;
        task->metrics->total_bytes_downloaded += chunk.size;
    }
    pthread_mutex_unlock(task->lock);

    return NULL;
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

int main(int argc, char *argv[]) {
    // Check for correct number of command-line arguments
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <urls.txt> <num_threads> <chunk_bytes>\n", argv[0]);
        return 1;
    }
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

    // Download phase: fetch content for each URL in parallel
    printf("\n[PHASE] Downloading URL content\n");
    start_timer(&metrics.url_download);
    
    // Initialize url_data array to NULL
    char **url_data = malloc(sizeof(char*) * url_count);
    for (int i = 0; i < url_count; i++) {
        url_data[i] = NULL;
    }
    pthread_t *download_threads = malloc(sizeof(pthread_t) * url_count);
    DownloadTask *download_tasks = malloc(sizeof(DownloadTask) * url_count);
    pthread_mutex_t download_lock;
    pthread_mutex_init(&download_lock, NULL);

    // Create download tasks and threads
    for (int i = 0; i < url_count; i++) {
        download_tasks[i] = (DownloadTask){
            .url = urls[i],
            .url_data = url_data,
            .url_index = i,
            .max_bytes = atoi(argv[3]),
            .metrics = &metrics,
            .lock = &download_lock
        };
        if (pthread_create(&download_threads[i], NULL, download_thread, &download_tasks[i])) {
            perror("pthread_create");
            download_tasks[i].url_data[i] = NULL;
            metrics.urls_failed++;
        }
    }

    // Wait for all download threads to complete
    for (int i = 0; i < url_count; i++) {
        pthread_join(download_threads[i], NULL);
    }

    pthread_mutex_destroy(&download_lock);
    free(download_threads);
    free(download_tasks);
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
        }
    }
    free(url_data);

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