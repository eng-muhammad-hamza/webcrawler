#ifndef WORKERS_H
#define WORKERS_H

#include <stddef.h>
#include <pthread.h>
#include "metrics.h"

#define MAX_WORD_LENGTH 100           // Maximum length for a word (including null terminator)
#define TOP_N_WORDS 10                // Number of top frequent words to track
#define INITIAL_WORD_CAPACITY 100     // Initial capacity for dynamic word arrays
#define GROWTH_FACTOR 2               // Factor by which arrays grow when resized
#define ALPHABET_SIZE 26              // Number of letters in the English alphabet
#define MAX_WORD_LEN_DIST 20          // Maximum word length to track in distribution

typedef struct {
    char word[MAX_WORD_LENGTH];       // The word itself
    int count;                        // Frequency of the word
} WordFreq;

typedef struct {
    pthread_mutex_t lock;             // Mutex for synchronizing access to stats
    size_t total_word_count;          // Total number of words processed
    char longest_word[MAX_WORD_LENGTH]; // The longest word found
    WordFreq *all_words;              // Dynamic array of all unique words and their counts
    int all_words_count;              // Number of unique words in all_words
    int all_words_capacity;           // Capacity of the all_words array
    WordFreq top_words[TOP_N_WORDS];  // Array of top N most frequent words
    int top_word_count;               // Number of words currently in top_words
    size_t sentence_count;            // Total number of sentences processed
    int char_frequency[ALPHABET_SIZE]; // Frequency of each letter (a-z, case-insensitive)
    WordFreq *palindromes;            // Dynamic array of palindrome words and their counts
    int palindrome_count;             // Number of palindrome words found
    int palindrome_capacity;          // Capacity of the palindromes array
    int word_length_dist[MAX_WORD_LEN_DIST]; // Distribution of word lengths (1 to MAX_WORD_LEN_DIST)
    int word_start_char[ALPHABET_SIZE]; // Count of words starting with each letter (a-z)
    double avg_word_length;           // Average word length
    SystemMetrics *metrics;           // Pointer to system metrics
} SharedStats;

// Initializes the SharedStats structure and its members
void init_shared_stats(SharedStats *stats, SystemMetrics *metrics);
// Frees any dynamically allocated memory in SharedStats
void free_shared_stats(SharedStats *stats);
// Prints the collected statistics in a readable format
void print_statistics(const SharedStats *stats);

// Worker Downloader function: Downloads the data in the parallel
void* worker_url_downloader(void *arg);

// Worker thread function: counts total words
void* worker_word_count(void *arg);
// Worker thread function: finds top N frequent words
void* worker_top_words(void *arg);
// Worker thread function: finds the longest word
void* worker_longest_word(void *arg);
// Worker thread function: counts sentences
void* worker_sentence_count(void *arg);
// Worker thread function: computes character frequency
void* worker_char_frequency(void *arg);
// Worker thread function: detects palindrome words
void* worker_palindrome_detector(void *arg);
// Worker thread function: computes word length distribution
void* worker_word_length_dist(void *arg);
// Worker thread function: counts words starting with each letter
void* worker_word_start_char(void *arg);
// Worker thread function: calculates average word length
void* worker_avg_word_length(void *arg);

#endif