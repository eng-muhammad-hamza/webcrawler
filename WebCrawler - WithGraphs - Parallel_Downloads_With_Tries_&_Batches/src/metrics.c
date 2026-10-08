#include "metrics.h"
#include <stdio.h>
#include <string.h>
#include <float.h>

// Starts the timer by recording the current monotonic time in the metric
void start_timer(TimeMetric *metric) {
    clock_gettime(CLOCK_MONOTONIC, &metric->start_time);
}

// Ends the timer and calculates the duration in milliseconds
void end_timer(TimeMetric *metric) {
    clock_gettime(CLOCK_MONOTONIC, &metric->end_time);
    // Calculate seconds difference and convert to ms
    metric->duration_ms = (metric->end_time.tv_sec - metric->start_time.tv_sec) * 1000.0;
    // Add nanoseconds difference converted to ms
    metric->duration_ms += (metric->end_time.tv_nsec - metric->start_time.tv_nsec) / 1000000.0;
}

// Initializes a WorkerMetrics struct to default values
void init_worker_metrics(WorkerMetrics *metrics) {
    metrics->total_time = 0;
    metrics->task_count = 0;
    metrics->min_time = DBL_MAX;                    // Set min_time to max possible double
    metrics->max_time = 0;
}

// Updates worker metrics with a new task duration
void update_worker_metrics(WorkerMetrics *metrics, double duration) {
    metrics->total_time += duration;
    metrics->task_count++;
    if (duration < metrics->min_time) metrics->min_time = duration;
    if (duration > metrics->max_time) metrics->max_time = duration;
}

// Prints a single timing metric with its name and duration in ms
void print_metric(const char *name, TimeMetric *metric) {
    printf("[METRIC] %-20s: %.2f ms\n", name, metric->duration_ms);
}

// Prints statistics for a worker: task count, total, average, min, and max times
void print_worker_metrics(const char *name, WorkerMetrics *metrics) {
    printf("[WORKER] %-15s: Tasks: %d, Total: %.2fms, Avg: %.2fms, Min: %.2fms, Max: %.2fms\n",
           name, metrics->task_count, metrics->total_time,
           metrics->task_count > 0 ? metrics->total_time / metrics->task_count : 0,
           metrics->min_time, metrics->max_time);
}

// Initializes the SystemMetrics struct and all its sub-metrics
void init_metrics(SystemMetrics *metrics) {
    memset(metrics, 0, sizeof(SystemMetrics));      // Zero out the struct
    start_timer(&metrics->total_runtime);           // Start total runtime timer
    // Initialize all worker metrics to default values
    init_worker_metrics(&metrics->word_count_metrics);
    init_worker_metrics(&metrics->top_words_metrics);
    init_worker_metrics(&metrics->longest_word_metrics);
    init_worker_metrics(&metrics->sentence_count_metrics);
    init_worker_metrics(&metrics->char_freq_metrics);
    init_worker_metrics(&metrics->palindrome_metrics);
    init_worker_metrics(&metrics->word_len_dist_metrics);
    init_worker_metrics(&metrics->word_start_char_metrics);
    init_worker_metrics(&metrics->avg_word_len_metrics);
}

// Prints all system and worker metrics, including data statistics
void print_system_metrics(SystemMetrics *metrics) {
    end_timer(&metrics->total_runtime);             // Stop total runtime timer
    printf("\n=== System Metrics ===\n");
    print_metric("Total runtime", &metrics->total_runtime);
    print_metric("URL download", &metrics->url_download);
    print_metric("Thread distribution", &metrics->thread_distribution);
    print_metric("Thread execution", &metrics->thread_execution);
    printf("\n=== Worker Performance ===\n");
    print_worker_metrics("word_count", &metrics->word_count_metrics);
    print_worker_metrics("top_words", &metrics->top_words_metrics);
    print_worker_metrics("longest_word", &metrics->longest_word_metrics);
    print_worker_metrics("sentence_count", &metrics->sentence_count_metrics);
    print_worker_metrics("char_frequency", &metrics->char_freq_metrics);
    print_worker_metrics("palindromes", &metrics->palindrome_metrics);
    print_worker_metrics("word_len_dist", &metrics->word_len_dist_metrics);
    print_worker_metrics("word_start_char", &metrics->word_start_char_metrics);
    print_worker_metrics("avg_word_len", &metrics->avg_word_len_metrics);
    printf("\n=== Data Metrics ===\n");
    // Print total bytes downloaded
    printf("[METRIC] %-20s: %zu bytes\n", "Total downloaded", metrics->total_bytes_downloaded);
    // Print number of URLs processed
    printf("[METRIC] %-20s: %d\n", "URLs processed", metrics->urls_processed);
    // Print number of URLs failed
    printf("[METRIC] %-20s: %d\n", "URLs failed", metrics->urls_failed);
    // Print average download size per URL in KB
    printf("[METRIC] %-20s: %.2f KB/URL\n", "Avg download size", 
           metrics->urls_processed > 0 ? (double)metrics->total_bytes_downloaded / metrics->urls_processed / 1024 : 0);
}