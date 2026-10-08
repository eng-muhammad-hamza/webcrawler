#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "workers.h"
#include "thread_pool.h"

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
    // Check command line arguments
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <urls.txt> <num_threads> <chunk_bytes>\n", argv[0]);
        return 1;
    }

    // Parse command line arguments
    const char *url_file = argv[1];                                 // File containing URLs
    int total_threads = atoi(argv[2]);                              // Number of threads to use
    size_t chunk_bytes = atoi(argv[3]);                             // Maximum bytes to download per URL

    // Validate arguments
    if (total_threads < 1 || chunk_bytes < 1) {
        fprintf(stderr, "[ERROR] Invalid arguments. Threads and bytes must be > 0.\n");
        return 1;
    }

    // Initialize CURL library
    curl_global_init(CURL_GLOBAL_ALL);

    // Read URLs from file
    char urls[MAX_URLS][MAX_URL_LEN];
    int url_count = read_urls(url_file, urls, MAX_URLS);

    if (url_count == 0) {
        fprintf(stderr, "[ERROR] No URLs to process.\n");
        return 1;
    }

    // url_data is char** means it is pointer to a string (char*) array
    // Download all URL data first (single-threaded)
    char **url_data = malloc(sizeof(char*) * url_count);            // Array to store downloaded data
    for (int i = 0; i < url_count; i++) {
        printf("Downloading URL %d/%d: %s\n", i+1, url_count, urls[i]);
        url_data[i] = download_url_data(urls[i], chunk_bytes);
        if (!url_data[i]) {
            fprintf(stderr, "Failed to download URL %d: %s\n", i+1, urls[i]);
        }
    }

    // Define available worker functions
    WorkerFunction workers[] = {worker_word_count, worker_top_words, worker_longest_word};
    int worker_count = sizeof(workers) / sizeof(workers[0]);        // Calculate number of worker types
    
    // Distribute threads among worker types
    int worker_threads[worker_count];
    distribute_threads(total_threads, worker_count, worker_threads);

    // Print thread distribution
    printf("Thread distribution:\n");
    for (int i = 0; i < worker_count; i++) {
        printf("  %s: %d threads\n", 
               (i == 0) ? "word_count" : 
               (i == 1) ? "top_words" : "longest_word", 
               worker_threads[i]);
    }

    // Initialize shared statistics structure
    SharedStats stats;
    init_shared_stats(&stats);

    // Create task queues for each worker type
    TaskQueue queues[worker_count];
    for (int i = 0; i < worker_count; i++) {
        init_task_queue(&queues[i], url_count);
    }

    /* The Number of Queues Created = No of Worker Functions
        0th Index of Each worker function queue contains the data of 1st url if downloaded correctly and so on */
    // Enqueue tasks for each URL and each worker type
    for (int url_idx = 0; url_idx < url_count; url_idx++) {
        if (url_data[url_idx]) {                                    // Only if download succeeded
            for (int worker_idx = 0; worker_idx < worker_count; worker_idx++) {
                ThreadTask task = {
                    .data = url_data[url_idx],                      // The downloaded data
                    .stats = &stats,                                // Shared statistics
                    .worker_fn = workers[worker_idx],               // Which function to call
                    .url_index = url_idx                            // URL identifier
                };
                enqueue_task(&queues[worker_idx], task);            // Add to appropriate queue
                // At this point no threads has been created so no need to implement locks within this function
            }
        }
    }

    // Start all worker threads
    run_worker_threads(queues, worker_count, worker_threads);

    // Cleanup
    for (int i = 0; i < url_count; i++) {
        if (url_data[i]) free(url_data[i]);                         // Free downloaded data
    }
    free(url_data);                                                 // Free array of pointers
    
    // Clean up task queues
    for (int i = 0; i < worker_count; i++) {
        free(queues[i].tasks);                                      // Free task arrays
        pthread_mutex_destroy(&queues[i].lock);                     // Destroy mutexes
    }

    // Print final statistics and cleanup
    print_statistics(&stats);
    free_shared_stats(&stats);
    curl_global_cleanup();                                          // Cleanup CURL
    return 0;
}