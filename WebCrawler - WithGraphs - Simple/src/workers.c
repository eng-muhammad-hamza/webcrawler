#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "workers.h"
#include "thread_pool.h"

void init_shared_stats(SharedStats *stats, SystemMetrics *metrics) {
    pthread_mutex_init(&stats->lock, NULL);
    stats->total_word_count = 0;
    stats->longest_word[0] = '\0';
    stats->top_word_count = 0;
    stats->all_words = malloc(INITIAL_WORD_CAPACITY * sizeof(WordFreq));
    stats->all_words_count = 0;
    stats->all_words_capacity = INITIAL_WORD_CAPACITY;
    stats->metrics = metrics;
}

void free_shared_stats(SharedStats *stats) {
    pthread_mutex_destroy(&stats->lock);                            // Destroy Mutex
    free(stats->all_words);                                         // Free the dynamic array
}

static void update_longest_word(SharedStats *stats, const char *word) {
    pthread_mutex_lock(&stats->lock);                               // Lock to prevent race conditions
    if (strlen(word) > strlen(stats->longest_word)) {
        strncpy(stats->longest_word, word, MAX_WORD_LENGTH - 1);    // Update longest word
    }
    pthread_mutex_unlock(&stats->lock);                             // Unlock
}

void* worker_word_count(void *arg) {
    ThreadTask *task = (ThreadTask *)arg;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    char *copy = strdup(task->data);
    char *word = strtok(copy, " \t\r\n.,;:!?\"'()[]{}<>");
    size_t local_count = 0;
    
    while (word) {
        if (strlen(word) > 0) local_count++;
        word = strtok(NULL, " \t\r\n.,;:!?\"'()[]{}<>");
    }

    pthread_mutex_lock(&task->stats->lock);
    task->stats->total_word_count += local_count;
    clock_gettime(CLOCK_MONOTONIC, &end);
    double duration = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;
    update_worker_metrics(&task->stats->metrics->word_count_metrics, duration);
    pthread_mutex_unlock(&task->stats->lock);

    printf("[TASK] word_count processed URL %d (%.2f ms, %zu words)\n", 
           task->url_index, duration, local_count);
    free(copy);
    return NULL;
}

void* worker_longest_word(void *arg) {
    ThreadTask *task = (ThreadTask *)arg;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    char *copy = strdup(task->data);                                // Copy for tokenization
    char *word = strtok(copy, " \t\r\n.,;:!?\"'()[]{}<>");          // Split into words

    while (word) {
        if (strlen(word) > 0)
            update_longest_word(task->stats, word);                 // Update longest word
        word = strtok(NULL, " \t\r\n.,;:!?\"'()[]{}<>");            // Next word
    }

    pthread_mutex_lock(&task->stats->lock);                               // Lock to prevent race conditions
    clock_gettime(CLOCK_MONOTONIC, &end);
    double duration = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;
    update_worker_metrics(&task->stats->metrics->longest_word_metrics, duration);
    pthread_mutex_unlock(&task->stats->lock);                               // Lock to prevent race conditions
    printf("[TASK] longest_word processed URL %d (%.2f ms)\n", 
           task->url_index, duration);
    free(copy);                                                     // Free the copy
    return NULL;
}

// Helper function to find a word in the all_words array
static int find_word_index(SharedStats *stats, const char *word) {
    for (int i = 0; i < stats->all_words_count; i++) {
        if (strcmp(stats->all_words[i].word, word) == 0) {
            return i;
        }
    }
    return -1;                                                      // Not found
}

static void update_top_words(SharedStats *stats, const char *word) {
    pthread_mutex_lock(&stats->lock);
    
    // Check if we need to grow the array
    if (stats->all_words_count >= stats->all_words_capacity) {
        stats->all_words_capacity *= GROWTH_FACTOR;
        stats->all_words = realloc(stats->all_words, 
                                  stats->all_words_capacity * sizeof(WordFreq));
    }
    
    // Find or add the word
    int word_index = find_word_index(stats, word);
    if (word_index == -1) {
        // New word - add to array
        strncpy(stats->all_words[stats->all_words_count].word, word, MAX_WORD_LENGTH - 1);
        stats->all_words[stats->all_words_count].count = 1;
        stats->all_words_count++;
    } else {
        // Existing word - increment count
        stats->all_words[word_index].count++;
    }
    
    pthread_mutex_unlock(&stats->lock);
}

// Comparison function for qsort (descending order by count)
static int compare_word_freq(const void *a, const void *b) {
    const WordFreq *wa = (const WordFreq *)a;
    const WordFreq *wb = (const WordFreq *)b;
    return wb->count - wa->count;
}

void* worker_top_words(void *arg) {
    ThreadTask *task = (ThreadTask *)arg;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    char *copy = strdup(task->data);
    char *word = strtok(copy, " \t\r\n.,;:!?\"'()[]{}<>");

    while (word) {
        // Convert word to lowercase
        for (char *p = word; *p; ++p) *p = tolower(*p);
        if (strlen(word) > 0) {
            update_top_words(task->stats, word);
        }
        word = strtok(NULL, " \t\r\n.,;:!?\"'()[]{}<>");
    }

    pthread_mutex_lock(&task->stats->lock);                               // Lock to prevent race conditions
    clock_gettime(CLOCK_MONOTONIC, &end);
    double duration = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;
    update_worker_metrics(&task->stats->metrics->top_words_metrics, duration);
    pthread_mutex_unlock(&task->stats->lock);                               // Lock to prevent race conditions
    printf("[TASK] top_words processed URL %d (%.2f ms)\n", 
           task->url_index, duration);

    free(copy);
    return NULL;
}

void print_statistics(const SharedStats *stats) {
    printf("\n=== Text Processing Statistics ===\n");
    printf("[STATS] Total words processed: %'zu\n", stats->total_word_count);
    printf("[STATS] Longest word: '%s' (%zu characters)\n", 
           stats->longest_word, strlen(stats->longest_word));
    
    WordFreq *sorted_words = malloc(stats->all_words_count * sizeof(WordFreq));
    memcpy(sorted_words, stats->all_words, stats->all_words_count * sizeof(WordFreq));
    qsort(sorted_words, stats->all_words_count, sizeof(WordFreq), compare_word_freq);

    int count_to_show = stats->all_words_count > TOP_N_WORDS ? TOP_N_WORDS : stats->all_words_count;
    
    printf("\n[STATS] Top %d Words:\n", count_to_show);
    printf("%-20s %10s %12s\n", "Word", "Count", "Frequency");
    printf("----------------------------------------\n");
    
    for (int i = 0; i < count_to_show; i++) {
        double freq = (double)sorted_words[i].count / stats->total_word_count * 100;
        printf("%-20s %10d %10.2f%%\n", sorted_words[i].word, sorted_words[i].count, freq);
    }
    
    free(sorted_words);
}