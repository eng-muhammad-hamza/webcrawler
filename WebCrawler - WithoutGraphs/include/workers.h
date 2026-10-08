#ifndef WORKERS_H
#define WORKERS_H

#include <stddef.h>
#include <pthread.h>

#define MAX_WORD_LENGTH 100                         // Maximum allowed length for a word (prevents buffer overflows)
#define TOP_N_WORDS 10                              // Number of top words to track
#define INITIAL_WORD_CAPACITY 100                   // Initial size of the dynamic words array
#define GROWTH_FACTOR 2                             // Factor by which the array grows when full

typedef struct {
    char word[MAX_WORD_LENGTH];                     // Stores a word
    int count;                                      // Stores its frequency
} WordFreq;

typedef struct {
    pthread_mutex_t lock;                           // Mutex for thread-safe access
    size_t total_word_count;                        // Total words processed
    char longest_word[MAX_WORD_LENGTH];             // Longest word found
    WordFreq *all_words;                            // Dynamic array of all unique words
    int all_words_count;                            // Current number of unique words
    int all_words_capacity;                         // Current capacity of the dynamic array
    WordFreq top_words[TOP_N_WORDS];                // Top N most frequent words
    int top_word_count;                             // Number of top words stored
} SharedStats;

void init_shared_stats(SharedStats *stats);         // Initialize SharedStats
void free_shared_stats(SharedStats *stats);         // Free allocated memory
void print_statistics(const SharedStats *stats);    // Print results

// Worker function types
void* worker_word_count(void *arg);                 // Counts total words
void* worker_top_words(void *arg);                  // Tracks top words
void* worker_longest_word(void *arg);               // Finds the longest word

#endif