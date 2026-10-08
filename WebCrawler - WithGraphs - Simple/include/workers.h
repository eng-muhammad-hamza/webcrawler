#ifndef WORKERS_H
#define WORKERS_H

#include <stddef.h>
#include <pthread.h>
#include "metrics.h"

#define MAX_WORD_LENGTH 100
#define TOP_N_WORDS 10
#define INITIAL_WORD_CAPACITY 100
#define GROWTH_FACTOR 2

typedef struct {
    char word[MAX_WORD_LENGTH];
    int count;
} WordFreq;

typedef struct {
    pthread_mutex_t lock;
    size_t total_word_count;
    char longest_word[MAX_WORD_LENGTH];
    WordFreq *all_words;
    int all_words_count;
    int all_words_capacity;
    WordFreq top_words[TOP_N_WORDS];
    int top_word_count;
    SystemMetrics *metrics;  // Added metrics reference
} SharedStats;

void init_shared_stats(SharedStats *stats, SystemMetrics *metrics);
void free_shared_stats(SharedStats *stats);
void print_statistics(const SharedStats *stats);

void* worker_word_count(void *arg);
void* worker_top_words(void *arg);
void* worker_longest_word(void *arg);

#endif