#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <unistd.h>
#include "workers.h"
#include "thread_pool.h"
#include "metrics.h"

// Set the maximum number of URLs we can handle
#define MAX_URLS 100
// Set the maximum length for each URL string
#define MAX_URL_LEN 1024

// Limit how many times we retry a failed download
#define MAX_RETRIES 3

// This struct is used to store the downloaded data and its size
typedef struct {
    char *memory;   // Pointer to the downloaded content
    size_t size;    // How many bytes we have downloaded
} MemoryBlock;

// This struct holds all the info needed for a download job
typedef struct {
    const char *url;        // The URL to download
    char **url_data;        // Array to store downloaded data for each URL
    int url_index;          // Index of this URL in the array
    size_t max_bytes;       // Max bytes to download from this URL
    SystemMetrics *metrics; // Pointer to metrics struct for stats
    pthread_mutex_t *lock;  // Mutex to protect shared data
    int attempt_count;      // How many times we've tried this download
} DownloadTask;

// This function is called by libcurl when it has data to write
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;                                     // Calculate how much data we got
    MemoryBlock *mem = (MemoryBlock *)userp;
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);         // Make sure we have enough memory to store the new data
    if (!ptr) return 0;                                                 // If we can't allocate memory, tell curl to stop
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);              // Copy the new data into our buffer
    mem->size += realsize;
    mem->memory[mem->size] = 0;                                         // Null-terminate the string
    return realsize;                                                    // Tell curl how many bytes we handled
}

// This is the function each download thread runs
void* download_thread(void *arg) {
    DownloadTask *task = (DownloadTask *)arg;
    CURL *curl_handle = NULL;
    MemoryBlock chunk = {0};                                                    // Start with empty memory and size 0
    CURLcode res = CURLE_OK;
    char range_str[64];                                                         // We'll use this buffer to build the HTTP Range header string
    // Try downloading up to MAX_RETRIES times
    for (task->attempt_count = 0; task->attempt_count < MAX_RETRIES; task->attempt_count++) {
        chunk.memory = malloc(1);                                               // Start with 1 byte, will grow as needed
        if (!chunk.memory) {
            fprintf(stderr, "[ERROR] Memory allocation failed for %s (attempt %d)\n",
                    task->url, task->attempt_count + 1);
            sleep(1);                                                           // Wait a bit before trying again
            continue;
        }
        chunk.size = 0;
        curl_handle = curl_easy_init();                                         // Get a new curl handle
        if (!curl_handle) {
            free(chunk.memory);
            fprintf(stderr, "[ERROR] curl_easy_init failed for %s (attempt %d)\n",
                    task->url, task->attempt_count + 1);
            sleep(1);                                                           // Wait a bit before trying again
            continue;
        }
        // If we want to limit how much we download, set the Range header using curl option setting which takes a range string
        if (task->max_bytes > 0) {
            int written = snprintf(range_str, sizeof(range_str), "0-%zu", task->max_bytes - 1);             // %zu is the specifier for unsigned size_t type variable
            if (written < 0 || (size_t)written >= sizeof(range_str)) {
                fprintf(stderr, "[ERROR] Failed to format range string or buffer too small for %s\n", task->url);
                curl_easy_cleanup(curl_handle);
                free(chunk.memory);
                sleep(1);                                                       // Wait a bit before trying again
                continue;
            }
            curl_easy_setopt(curl_handle, CURLOPT_RANGE, range_str);            // Setting the Range Option
        }
        // Set up curl options for this download
        curl_easy_setopt(curl_handle, CURLOPT_URL, task->url);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_callback);   // Setting Write Function
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);       // Setting the Block at which data must be written
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "Multithreaded Web Crawler");
        curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 15L);                    // Give up after 15 seconds
        curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 5L);              // 5 seconds to connect
        curl_easy_setopt(curl_handle, CURLOPT_LOW_SPEED_LIMIT, 8192L);          // If speed drops below 8KB/sec then
        curl_easy_setopt(curl_handle, CURLOPT_LOW_SPEED_TIME, 20L);             // try maintaining the speed for 20 seconds, otherwise abort
        curl_easy_setopt(curl_handle, CURLOPT_FAILONERROR, 1L);                 // Fail on HTTP errors
        curl_easy_setopt(curl_handle, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
        curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L);              // Follow redirects if there is any

        // Actually do the download
        res = curl_easy_perform(curl_handle);

        if (res == CURLE_OK) {
            // If we got more data than we wanted, cut it off
            if (chunk.size > task->max_bytes && task->max_bytes > 0) {
                chunk.memory[task->max_bytes] = '\0';
                chunk.size = task->max_bytes;
            }
            break;                                                              // Download succeeded, stop retrying
        }

        // If we get here, the download failed
        curl_easy_cleanup(curl_handle);
        curl_handle = NULL;
        if (chunk.memory) {
            free(chunk.memory);
            chunk.memory = NULL;
        }

        fprintf(stderr, "[WARNING] Attempt %d failed for %s: %s\n",
                task->attempt_count + 1, task->url, curl_easy_strerror(res));
        sleep(2);                                                               // Wait a bit longer before next try
    }

    // Now update shared data (metrics, url_data) safely
    pthread_mutex_lock(task->lock);
    if (res == CURLE_OK) {
        printf("[DOWNLOAD] %03d: %-60s [OK] %zu bytes (%d attempts)\n",
               task->url_index + 1, task->url, chunk.size, task->attempt_count + 1);
        // Save the downloaded data for this URL
        task->url_data[task->url_index] = chunk.memory;
        task->metrics->urls_processed++;
        task->metrics->total_bytes_downloaded += chunk.size;
    } else {
        fprintf(stderr, "[ERROR] Final attempt failed for %s: %s\n",
                task->url, curl_easy_strerror(res));
        task->metrics->urls_failed++;
        if (chunk.memory) {
            free(chunk.memory);
        }
        task->url_data[task->url_index] = NULL;                                 // Mark as failed
    }
    pthread_mutex_unlock(task->lock);

    if (curl_handle) {
        curl_easy_cleanup(curl_handle);
    }
    return NULL;
}

// Reads URLs from a file and puts them in the urls array
int read_urls(const char *filepath, char urls[][MAX_URL_LEN], int max_urls) {
    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        perror("[ERROR] Cannot open URL file");
        return 0;
    }
    int count = 0;
    // Read each line from the file, up to max_urls
    while (fgets(urls[count], MAX_URL_LEN, fp) && count < max_urls) {
        // Remove newline at the end of the line
        urls[count][strcspn(urls[count], "\r\n")] = 0;
        count++;
    }
    fclose(fp);
    return count;                                                       // Return how many URLs we read
}

int main(int argc, char *argv[]) {
    // Check if user gave us the right number of arguments
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <urls.txt> <num_threads> <chunk_bytes>\n", argv[0]);
        return 1;
    }
    // Set up metrics to track performance
    SystemMetrics metrics;
    init_metrics(&metrics);
    printf("\n[SYSTEM] Starting Multithreaded Web Crawler\n");
    printf("[CONFIG] Threads: %d, Max bytes per URL: %s\n", atoi(argv[2]), argv[3]);
    // Initialize curl library for the whole program
    curl_global_init(CURL_GLOBAL_ALL);
    // Array to hold all the URLs we read from the file
    char urls[MAX_URLS][MAX_URL_LEN];
    int url_count = read_urls(argv[1], urls, MAX_URLS);
    printf("[SYSTEM] Loaded %d URLs to process\n", url_count);

    // Start the download phase
    printf("\n[PHASE] Downloading URL content\n");
    start_timer(&metrics.url_download);

    // Allocate memory to store the downloaded data for each URL
    char **url_data = malloc(sizeof(char*) * url_count);
    for (int i = 0; i < url_count; i++) {
        url_data[i] = NULL;
    }
    // Allocate arrays for threads and their tasks
    pthread_t *download_threads = malloc(sizeof(pthread_t) * url_count);
    DownloadTask *download_tasks = malloc(sizeof(DownloadTask) * url_count);
    pthread_mutex_t download_lock;
    pthread_mutex_init(&download_lock, NULL);

    // Set up and start a thread for each URL download
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

    // Wait for all download threads to finish
    for (int i = 0; i < url_count; i++) {
        pthread_join(download_threads[i], NULL);
    }

    // Free Up the Downloader Resources
    pthread_mutex_destroy(&download_lock);
    free(download_threads);
    free(download_tasks);
    end_timer(&metrics.url_download);
    
    printf("[STATUS] Downloads completed: %d success, %d failed\n", 
           metrics.urls_processed, metrics.urls_failed);

    // Now we move on to distributing threads for analysis
    printf("\n[PHASE] Distributing %d threads\n", atoi(argv[2]));
    start_timer(&metrics.thread_distribution);
    // List of worker functions for different analysis tasks
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
    // Split up the total threads among the different worker types
    distribute_threads(atoi(argv[2]), worker_count, worker_threads);
    end_timer(&metrics.thread_distribution);
    printf("[THREADS] Distribution:\n");
    // Print out how many threads each worker type gets
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
    // Now we set up the task stacks for each worker type
    printf("\n[PHASE] Processing tasks\n");
    SharedStats stats;
    init_shared_stats(&stats, &metrics);
    TaskStack stacks[worker_count];
    for (int i = 0; i < worker_count; i++) {
        init_task_stack(&stacks[i], url_count);
    }
    // For every downloaded URL, add a task for each worker type
    for (int url_idx = 0; url_idx < url_count; url_idx++) {
        if (url_data[url_idx] != NULL) { // Only if download succeeded
            for (int worker_idx = 0; worker_idx < worker_count; worker_idx++) {
                ThreadTask task = {
                    .data = url_data[url_idx],
                    .stats = &stats,
                    .worker_fn = workers[worker_idx],
                    .url_index = url_idx
                };
                push_task(&stacks[worker_idx], task);
            }
        }
    }
    // Start the worker threads to process all the tasks
    start_timer(&metrics.thread_execution);
    run_worker_threads(stacks, worker_count, worker_threads);
    end_timer(&metrics.thread_execution);
    printf("[STATUS] All tasks completed\n");

    // Free up all the memory we used for downloaded data
    for (int i = 0; i < url_count; i++) {
        if (url_data[i] != NULL) {
            free(url_data[i]);
            url_data[i] = NULL;
        }
    }
    free(url_data);
    url_data = NULL;

    // Clean up the task stacks and destroy their mutexes
    for (int i = 0; i < worker_count; i++) {
        free(stacks[i].tasks);
        pthread_mutex_destroy(&stacks[i].lock);
    }
    // Print out the final stats and metrics
    printf("\n=== Final Results ===\n");
    print_statistics(&stats);
    print_system_metrics(&metrics);
    // Save the metrics to a CSV file for later analysis
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
    // Also append metrics to a history file so we can track progress over time
    FILE *history = fopen("metrics_history.csv", "a");
    if (history) {
        // If the file is empty, write the header first
        if (ftell(history) == 0) {
            fprintf(history, "timestamp,total_runtime,url_download,thread_distribution,"
                    "thread_execution,word_count_total,top_words_total,longest_word_total,"
                    "sentence_count_total,char_freq_total,palindrome_total,"
                    "word_len_dist_total,word_start_char_total,avg_word_len_total,"
                    "urls_processed,urls_failed,total_bytes\n");
        }
        // Write the current time and all the metrics
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
    // Free up the stats struct and clean up curl before exiting
    free_shared_stats(&stats);
    curl_global_cleanup();
    return 0;
}
