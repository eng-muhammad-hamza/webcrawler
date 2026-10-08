#include "metrics.h"
#include <stdio.h>
#include <string.h>

void start_timer(TimeMetric *metric) {
    clock_gettime(CLOCK_MONOTONIC, &metric->start_time);
}

void end_timer(TimeMetric *metric) {
    clock_gettime(CLOCK_MONOTONIC, &metric->end_time);
    metric->duration_ms = (metric->end_time.tv_sec - metric->start_time.tv_sec) * 1000.0;
    metric->duration_ms += (metric->end_time.tv_nsec - metric->start_time.tv_nsec) / 1000000.0;
}

void init_worker_metrics(WorkerMetrics *metrics) {
    metrics->total_time = 0;
    metrics->task_count = 0;
    metrics->min_time = DBL_MAX;
    metrics->max_time = 0;
}

void update_worker_metrics(WorkerMetrics *metrics, double duration) {
    metrics->total_time += duration;
    metrics->task_count++;
    if (duration < metrics->min_time) metrics->min_time = duration;
    if (duration > metrics->max_time) metrics->max_time = duration;
}

void print_metric(const char *name, TimeMetric *metric) {
    printf("[METRIC] %-20s: %.2f ms\n", name, metric->duration_ms);
}

void print_worker_metrics(const char *name, WorkerMetrics *metrics) {
    printf("[WORKER] %-15s: Tasks: %d, Total: %.2fms, Avg: %.2fms, Min: %.2fms, Max: %.2fms\n",
           name, metrics->task_count, metrics->total_time,
           metrics->task_count > 0 ? metrics->total_time / metrics->task_count : 0,
           metrics->min_time, metrics->max_time);
}

void init_metrics(SystemMetrics *metrics) {
    memset(metrics, 0, sizeof(SystemMetrics));
    start_timer(&metrics->total_runtime);
    init_worker_metrics(&metrics->word_count_metrics);
    init_worker_metrics(&metrics->top_words_metrics);
    init_worker_metrics(&metrics->longest_word_metrics);
}

void print_system_metrics(SystemMetrics *metrics) {
    end_timer(&metrics->total_runtime);
    
    printf("\n=== System Metrics ===\n");
    print_metric("Total runtime", &metrics->total_runtime);
    print_metric("URL download", &metrics->url_download);
    print_metric("Thread distribution", &metrics->thread_distribution);
    print_metric("Thread execution", &metrics->thread_execution);
    
    printf("\n=== Worker Performance ===\n");
    print_worker_metrics("word_count", &metrics->word_count_metrics);
    print_worker_metrics("top_words", &metrics->top_words_metrics);
    print_worker_metrics("longest_word", &metrics->longest_word_metrics);
    
    printf("\n=== Data Metrics ===\n");
    printf("[METRIC] %-20s: %zu bytes\n", "Total downloaded", metrics->total_bytes_downloaded);
    printf("[METRIC] %-20s: %d\n", "URLs processed", metrics->urls_processed);
    printf("[METRIC] %-20s: %d\n", "URLs failed", metrics->urls_failed);
    printf("[METRIC] %-20s: %.2f KB/URL\n", "Avg download size", 
           metrics->urls_processed > 0 ? (double)metrics->total_bytes_downloaded / metrics->urls_processed / 1024 : 0);
}