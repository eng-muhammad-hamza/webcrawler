#ifndef METRICS_H
#define METRICS_H

#include <time.h>
#include <stdio.h>
#include <float.h>

// Structure to store timing information for a metric
typedef struct {
    struct timespec start_time;                 // Start time of the metric
    struct timespec end_time;                   // End time of the metric
    double duration_ms;                         // Duration in milliseconds
} TimeMetric;

// Structure to store aggregated metrics for worker tasks
typedef struct {
    double total_time;                          // Total time spent by the worker
    int task_count;                             // Number of tasks completed
    double min_time;                            // Minimum time taken for a task
    double max_time;                            // Maximum time taken for a task
} WorkerMetrics;

// Structure to store all system-wide metrics
typedef struct {
    TimeMetric total_runtime;                   // Total runtime of the system
    TimeMetric url_download;                    // Time taken to download URLs
    TimeMetric thread_distribution;             // Time for thread distribution
    TimeMetric thread_execution;                // Time for thread execution
    size_t total_bytes_downloaded;              // Total bytes downloaded
    int urls_processed;                         // Number of URLs processed
    int urls_failed;                            // Number of URLs failed
    WorkerMetrics word_count_metrics;           // Metrics for word count tasks
    WorkerMetrics top_words_metrics;            // Metrics for top words tasks
    WorkerMetrics longest_word_metrics;         // Metrics for longest word tasks
    WorkerMetrics sentence_count_metrics;       // Metrics for sentence count tasks
    WorkerMetrics char_freq_metrics;            // Metrics for character frequency tasks
    WorkerMetrics unique_words_metrics;         // Metrics for unique words tasks
    WorkerMetrics palindrome_metrics;           // Metrics for palindrome tasks
    WorkerMetrics word_len_dist_metrics;        // Metrics for word length distribution tasks
    WorkerMetrics word_start_char_metrics;      // Metrics for word start character tasks
    WorkerMetrics avg_word_len_metrics;         // Metrics for average word length tasks
} SystemMetrics;

// Start the timer for a metric
void start_timer(TimeMetric *metric);
// End the timer for a metric and calculate duration
void end_timer(TimeMetric *metric);
// Initialize worker metrics to default values
void init_worker_metrics(WorkerMetrics *metrics);
// Update worker metrics with a new duration
void update_worker_metrics(WorkerMetrics *metrics, double duration);
// Print a single time metric
void print_metric(const char *name, TimeMetric *metric);
// Print all worker metrics
void print_worker_metrics(const char *name, WorkerMetrics *metrics);
// Initialize all system metrics to default values
void init_metrics(SystemMetrics *metrics);
// Print all system metrics
void print_system_metrics(SystemMetrics *metrics);

#endif