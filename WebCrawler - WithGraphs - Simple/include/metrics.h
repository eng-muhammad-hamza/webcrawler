#ifndef METRICS_H
#define METRICS_H

#include <time.h>
#include <stdio.h>
#include <float.h>

typedef struct {
    struct timespec start_time;
    struct timespec end_time;
    double duration_ms;
} TimeMetric;

typedef struct {
    double total_time;
    int task_count;
    double min_time;
    double max_time;
} WorkerMetrics;

typedef struct {
    TimeMetric total_runtime;
    TimeMetric url_download;
    TimeMetric thread_distribution;
    TimeMetric thread_execution;
    size_t total_bytes_downloaded;
    int urls_processed;
    int urls_failed;
    
    WorkerMetrics word_count_metrics;
    WorkerMetrics top_words_metrics;
    WorkerMetrics longest_word_metrics;
} SystemMetrics;

void start_timer(TimeMetric *metric);
void end_timer(TimeMetric *metric);
void init_worker_metrics(WorkerMetrics *metrics);
void update_worker_metrics(WorkerMetrics *metrics, double duration);
void print_metric(const char *name, TimeMetric *metric);
void print_worker_metrics(const char *name, WorkerMetrics *metrics);
void init_metrics(SystemMetrics *metrics);
void print_system_metrics(SystemMetrics *metrics);

#endif