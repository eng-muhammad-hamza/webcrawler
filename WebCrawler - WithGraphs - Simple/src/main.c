#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "workers.h"
#include "thread_pool.h"
#include "metrics.h"

#define MAX_URLS 100
#define MAX_URL_LEN 1024

// Structure for storing downloaded data in memory
typedef struct {
    char *memory;                                                   // Pointer to the downloaded data
    size_t size;                                                    // Size of the downloaded data
} MemoryBlock;

// Callback function for CURL to write received data
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;                                 // Calculate actual size of received data
    MemoryBlock *mem = (MemoryBlock *)userp;                        // Get our memory block

    // Reallocate memory to accommodate new data (+1 for null terminator)
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr)                                                       // If realloc fails
        return 0;                                                   // Signal failure to CURL

    mem->memory = ptr;                                              // Update memory pointer
    // Append new data to existing buffer
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;                                          // Update size
    mem->memory[mem->size] = 0;                                     // Null-terminate the data

    return realsize;                                                // Tell CURL we processed all data
}

// Function to download data from a URL with size limit
char *download_url_data(const char *url, size_t max_bytes) {
    CURL *curl_handle;                                              // CURL handle
    CURLcode res;                                                   // CURL result code

    // Initialize memory block
    MemoryBlock chunk;
    chunk.memory = malloc(1);                                       // Start with minimal allocation
    chunk.size = 0;                                                 // No data yet

    curl_handle = curl_easy_init();                                 // Initialize CURL
    if (!curl_handle)
        return NULL;                                                // Failed to initialize

    // Set CURL options:
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);                // URL to download
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_callback);   // Our callback to process data
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);       // Block to Where we want to store data
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "Multithreaded Web Crawler");      // User agent
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 10L);            // 10 second timeout

    // Perform the download
    res = curl_easy_perform(curl_handle);
    curl_easy_cleanup(curl_handle);                                 // Clean up CURL handle

    if (res != CURLE_OK) {  // If download failed
        fprintf(stderr, "[ERROR] Failed to download %s: %s\n", url, curl_easy_strerror(res));
        free(chunk.memory);
        return NULL;
    }

    // Truncate data if it exceeds max_bytes
    if (chunk.size > max_bytes) {
        chunk.memory[max_bytes] = '\0';                             // Force null-termination at max_bytes
        chunk.size = max_bytes;                                     // Update size
    }
    else {
        // Just ensure null-termination (though callback already did this)
        chunk.memory[chunk.size] = '\0';
    }

    return chunk.memory;                                            // Return downloaded data
}

// Function to read URLs from a file
int read_urls(const char *filepath, char urls[][MAX_URL_LEN], int max_urls) {
    FILE *fp = fopen(filepath, "r");                                // Open file in read mode
    if (!fp) {
        perror("[ERROR] Cannot open URL file");
        return 0;
    }

    int count = 0;
    // Read URLs until we reach max_urls or end of file
    while (fgets(urls[count], MAX_URL_LEN, fp) && count < max_urls) {
        // Remove newline characters
        urls[count][strcspn(urls[count], "\r\n")] = 0;
        count++;
    }

    fclose(fp);                                                     // Close file
    return count;                                                   // Return number of URLs read
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <urls.txt> <num_threads> <chunk_bytes>\n", argv[0]);
        return 1;
    }

    SystemMetrics metrics;
    init_metrics(&metrics);
    
    printf("\n[SYSTEM] Starting Multithreaded Web Crawler\n");
    printf("[CONFIG] Threads: %d, Max bytes per URL: %s\n", atoi(argv[2]), argv[3]);

    curl_global_init(CURL_GLOBAL_ALL);
    char urls[MAX_URLS][MAX_URL_LEN];
    int url_count = read_urls(argv[1], urls, MAX_URLS);
    printf("[SYSTEM] Loaded %d URLs to process\n", url_count);

    // Download phase
    printf("\n[PHASE] Downloading URL content\n");
    start_timer(&metrics.url_download);
    char **url_data = malloc(sizeof(char*) * url_count);
    for (int i = 0; i < url_count; i++) {
        printf("[DOWNLOAD] %03d/%03d: %-50s", i+1, url_count, urls[i]);
        url_data[i] = download_url_data(urls[i], atoi(argv[3]));
        if (!url_data[i]) {
            metrics.urls_failed++;
            printf(" [FAILED]\n");
        } else {
            metrics.urls_processed++;
            metrics.total_bytes_downloaded += strlen(url_data[i]);
            printf(" [OK] %zu bytes\n", strlen(url_data[i]));
        }
    }
    end_timer(&metrics.url_download);
    printf("[STATUS] Downloads completed: %d success, %d failed\n", 
           metrics.urls_processed, metrics.urls_failed);

    // Thread distribution
    printf("\n[PHASE] Distributing %d threads\n", atoi(argv[2]));
    start_timer(&metrics.thread_distribution);
    WorkerFunction workers[] = {worker_word_count, worker_top_words, worker_longest_word};
    int worker_count = sizeof(workers) / sizeof(workers[0]);
    int worker_threads[worker_count];
    distribute_threads(atoi(argv[2]), worker_count, worker_threads);
    end_timer(&metrics.thread_distribution);
    
    printf("[THREADS] Distribution:\n");
    for (int i = 0; i < worker_count; i++) {
        printf("  %-12s: %d threads\n", 
               (i == 0) ? "word_count" : (i == 1) ? "top_words" : "longest_word", 
               worker_threads[i]);
    }

    // Task processing
    printf("\n[PHASE] Processing tasks\n");
    SharedStats stats;
    init_shared_stats(&stats, &metrics);
    
    TaskQueue queues[worker_count];
    for (int i = 0; i < worker_count; i++) {
        init_task_queue(&queues[i], url_count);
    }

    for (int url_idx = 0; url_idx < url_count; url_idx++) {
        if (url_data[url_idx]) {
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

    start_timer(&metrics.thread_execution);
    run_worker_threads(queues, worker_count, worker_threads);
    end_timer(&metrics.thread_execution);
    printf("[STATUS] All tasks completed\n");

    // Cleanup and output
    for (int i = 0; i < url_count; i++) free(url_data[i]);
    free(url_data);
    
    for (int i = 0; i < worker_count; i++) {
        free(queues[i].tasks);
        pthread_mutex_destroy(&queues[i].lock);
    }

    printf("\n=== Final Results ===\n");
    print_statistics(&stats);
    print_system_metrics(&metrics);
    
    // Save metrics for visualization
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
        fprintf(csv, "urls_processed,%d\n", metrics.urls_processed);
        fprintf(csv, "urls_failed,%d\n", metrics.urls_failed);
        fprintf(csv, "total_bytes,%zu\n", metrics.total_bytes_downloaded);
        fclose(csv);
    }

    // Append metrics to history file
    FILE *history = fopen("metrics_history.csv", "a");
    if (history) {
        if (ftell(history) == 0) {
            // Write header if new file
            fprintf(history, "timestamp,total_runtime,url_download,thread_distribution,"
                    "thread_execution,word_count_total,top_words_total,longest_word_total,"
                    "urls_processed,urls_failed,total_bytes\n");
        }
        
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        fprintf(history, "\"%04d-%02d-%02d %02d:%02d:%02d\",%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%d,%zu\n",
                t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                t->tm_hour, t->tm_min, t->tm_sec,
                metrics.total_runtime.duration_ms,
                metrics.url_download.duration_ms,
                metrics.thread_distribution.duration_ms,
                metrics.thread_execution.duration_ms,
                metrics.word_count_metrics.total_time,
                metrics.top_words_metrics.total_time,
                metrics.longest_word_metrics.total_time,
                metrics.urls_processed,
                metrics.urls_failed,
                metrics.total_bytes_downloaded);
        fclose(history);
    }

    free_shared_stats(&stats);

    curl_global_cleanup();
    return 0;
}