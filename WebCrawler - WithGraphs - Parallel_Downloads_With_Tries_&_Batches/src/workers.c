#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include "workers.h"
#include "thread_pool.h"

// Initialize the shared statistics structure and its fields
void init_shared_stats(SharedStats *stats, SystemMetrics *metrics) {
    pthread_mutex_init(&stats->lock, NULL); // Initialize mutex for thread safety
    stats->total_word_count = 0;            // Total number of words processed
    stats->longest_word[0] = '\0';          // Longest word found so far (empty initially)
    stats->top_word_count = 0;              // Number of top words (not used directly here)
    stats->all_words = malloc(INITIAL_WORD_CAPACITY * sizeof(WordFreq)); // Dynamic array for all words
    stats->all_words_count = 0;             // Number of unique words found
    stats->all_words_capacity = INITIAL_WORD_CAPACITY; // Capacity of all_words array
    stats->sentence_count = 0;              // Total number of sentences processed
    memset(stats->char_frequency, 0, sizeof(stats->char_frequency)); // Frequency of each character a-z
    stats->palindromes = NULL;              // Dynamic array for palindrome words
    stats->palindrome_count = 0;            // Number of unique palindromes found
    stats->palindrome_capacity = 0;         // Capacity of palindromes array
    memset(stats->word_length_dist, 0, sizeof(stats->word_length_dist)); // Distribution of word lengths
    memset(stats->word_start_char, 0, sizeof(stats->word_start_char));   // Distribution of starting chars
    stats->avg_word_length = 0.0;           // Average word length
    stats->metrics = metrics;               // Pointer to system metrics for timing
}

// Free memory allocated for shared statistics
void free_shared_stats(SharedStats *stats) {
    pthread_mutex_destroy(&stats->lock); // Destroy mutex
    free(stats->all_words);              // Free all_words array
    free(stats->palindromes);            // Free palindromes array
}

// Update the longest word found so far in a thread-safe way
static void update_longest_word(SharedStats *stats, const char *word) {
    pthread_mutex_lock(&stats->lock); // Lock for thread safety
    if (strlen(word) > strlen(stats->longest_word)) {
        strncpy(stats->longest_word, word, MAX_WORD_LENGTH - 1); // Update if longer
    }
    pthread_mutex_unlock(&stats->lock); // Unlock
}

// Worker function to count words in the given text
void* worker_word_count(void *arg) {
    ThreadTask *task = (ThreadTask *)arg;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start); // Start timing

    char *copy = strdup(task->data); // Make a copy for tokenization
    char *word = strtok(copy, " \t\r\n.,;:!?\"'()[]{}<>"); // Split into words
    size_t local_count = 0; // Local word count

    while (word) {
        if (strlen(word) > 0) local_count++; // Count non-empty words
        word = strtok(NULL, " \t\r\n.,;:!?\"'()[]{}<>");
    }

    pthread_mutex_lock(&task->stats->lock); // Lock to update shared stats
    task->stats->total_word_count += local_count; // Add to total word count
    clock_gettime(CLOCK_MONOTONIC, &end); // End timing
    double duration = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;
    update_worker_metrics(&task->stats->metrics->word_count_metrics, duration); // Update timing metrics
    pthread_mutex_unlock(&task->stats->lock);

    printf("[TASK] word_count processed URL %d (%.2f ms, %zu words)\n", 
           task->url_index, duration, local_count);
    free(copy); // Free memory
    return NULL;
}

// Worker function to find the longest word in the text
void* worker_longest_word(void *arg) {
    ThreadTask *task = (ThreadTask *)arg;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    char *copy = strdup(task->data); // Copy for tokenization
    char *word = strtok(copy, " \t\r\n.,;:!?\"'()[]{}<>"); // Split into words

    while (word) {
        if (strlen(word) > 0)
            update_longest_word(task->stats, word); // Update longest word if needed
        word = strtok(NULL, " \t\r\n.,;:!?\"'()[]{}<>");
    }

    pthread_mutex_lock(&task->stats->lock); // Lock for timing update
    clock_gettime(CLOCK_MONOTONIC, &end);
    double duration = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;
    update_worker_metrics(&task->stats->metrics->longest_word_metrics, duration);
    pthread_mutex_unlock(&task->stats->lock);
    printf("[TASK] longest_word processed URL %d (%.2f ms)\n", 
           task->url_index, duration);
    free(copy);
    return NULL;
}

// Helper function to find the index of a word in the all_words array
static int find_word_index(SharedStats *stats, const char *word) {
    for (int i = 0; i < stats->all_words_count; i++) {
        if (strcmp(stats->all_words[i].word, word) == 0) {
            return i; // Found
        }
    }
    return -1; // Not found
}

// Update the top words list with a new word occurrence
static void update_top_words(SharedStats *stats, const char *word) {
    pthread_mutex_lock(&stats->lock);

    // Grow the array if needed
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

// Comparison function for sorting words by frequency (descending)
static int compare_word_freq(const void *a, const void *b) {
    const WordFreq *wa = (const WordFreq *)a;
    const WordFreq *wb = (const WordFreq *)b;
    return wb->count - wa->count;
}

// Worker function to count and track the most frequent words
void* worker_top_words(void *arg) {
    ThreadTask *task = (ThreadTask *)arg;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    char *copy = strdup(task->data);
    char *word = strtok(copy, " \t\r\n.,;:!?\"'()[]{}<>");

    while (word) {
        // Convert word to lowercase for case-insensitive counting
        for (char *p = word; *p; ++p) *p = tolower(*p);
        if (strlen(word) > 0) {
            update_top_words(task->stats, word);
        }
        word = strtok(NULL, " \t\r\n.,;:!?\"'()[]{}<>");
    }

    pthread_mutex_lock(&task->stats->lock);
    clock_gettime(CLOCK_MONOTONIC, &end);
    double duration = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;
    update_worker_metrics(&task->stats->metrics->top_words_metrics, duration);
    pthread_mutex_unlock(&task->stats->lock);
    printf("[TASK] top_words processed URL %d (%.2f ms)\n", 
           task->url_index, duration);

    free(copy);
    return NULL;
}

// Worker function to count the number of sentences in the text
void* worker_sentence_count(void *arg) {
    ThreadTask *task = (ThreadTask *)arg;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    char *copy = strdup(task->data);
    size_t local_count = 0;

    // Count sentence-ending punctuation marks
    for (char *p = copy; *p; p++) {
        if (*p == '.' || *p == '?' || *p == '!') {
            local_count++;
            // Skip consecutive punctuation
            while (*(p+1) && (*(p+1) == '.' || *(p+1) == '?' || *(p+1) == '!')) {
                p++;
            }
        }
    }

    pthread_mutex_lock(&task->stats->lock);
    task->stats->sentence_count += local_count;
    clock_gettime(CLOCK_MONOTONIC, &end);
    double duration = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;
    update_worker_metrics(&task->stats->metrics->sentence_count_metrics, duration);
    pthread_mutex_unlock(&task->stats->lock);

    printf("[TASK] sentence_count processed URL %d (%.2f ms, %zu sentences)\n", 
           task->url_index, duration, local_count);
    free(copy);
    return NULL;
}

// Worker function to count the frequency of each character (a-z)
void* worker_char_frequency(void *arg) {
    ThreadTask *task = (ThreadTask *)arg;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    char *copy = strdup(task->data);
    int local_freq[ALPHABET_SIZE] = {0}; // Local frequency array

    for (char *p = copy; *p; p++) {
        if (isalpha(*p)) {
            char c = tolower(*p);
            local_freq[c - 'a']++;
        }
    }

    pthread_mutex_lock(&task->stats->lock);
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        task->stats->char_frequency[i] += local_freq[i];
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double duration = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;
    update_worker_metrics(&task->stats->metrics->char_freq_metrics, duration);
    pthread_mutex_unlock(&task->stats->lock);

    printf("[TASK] char_frequency processed URL %d (%.2f ms)\n", 
           task->url_index, duration);
    free(copy);
    return NULL;
}

// Helper function to check if a word is a palindrome (case-insensitive)
static int is_palindrome(const char *word) {
    int len = strlen(word);
    for (int i = 0; i < len/2; i++) {
        if (tolower(word[i]) != tolower(word[len-1-i])) {
            return 0; // Not a palindrome
        }
    }
    return 1; // Is a palindrome
}

// Worker function to detect and count palindrome words
void* worker_palindrome_detector(void *arg) {
    ThreadTask *task = (ThreadTask *)arg;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    char *copy = strdup(task->data);
    char *word = strtok(copy, " \t\r\n.,;:!?\"'()[]{}<>");

    while (word) {
        if (strlen(word) > 1 && is_palindrome(word)) {
            pthread_mutex_lock(&task->stats->lock);

            // Grow palindrome array if needed
            if (task->stats->palindrome_count >= task->stats->palindrome_capacity) {
                task->stats->palindrome_capacity = task->stats->palindrome_capacity == 0 ? 
                    INITIAL_WORD_CAPACITY : task->stats->palindrome_capacity * GROWTH_FACTOR;
                task->stats->palindromes = realloc(task->stats->palindromes, 
                    task->stats->palindrome_capacity * sizeof(WordFreq));
            }

            // Find or add the palindrome
            int found = 0;
            for (int i = 0; i < task->stats->palindrome_count; i++) {
                if (strcasecmp(task->stats->palindromes[i].word, word) == 0) {
                    task->stats->palindromes[i].count++;
                    found = 1;
                    break;
                }
            }

            if (!found) {
                strncpy(task->stats->palindromes[task->stats->palindrome_count].word, 
                       word, MAX_WORD_LENGTH - 1);
                task->stats->palindromes[task->stats->palindrome_count].count = 1;
                task->stats->palindrome_count++;
            }

            pthread_mutex_unlock(&task->stats->lock);
        }
        word = strtok(NULL, " \t\r\n.,;:!?\"'()[]{}<>");
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double duration = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;

    pthread_mutex_lock(&task->stats->lock);
    update_worker_metrics(&task->stats->metrics->palindrome_metrics, duration);
    pthread_mutex_unlock(&task->stats->lock);

    printf("[TASK] palindrome_detector processed URL %d (%.2f ms)\n", 
           task->url_index, duration);
    free(copy);
    return NULL;
}

// Worker function to compute the distribution of word lengths
void* worker_word_length_dist(void *arg) {
    ThreadTask *task = (ThreadTask *)arg;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    char *copy = strdup(task->data);
    char *word = strtok(copy, " \t\r\n.,;:!?\"'()[]{}<>");
    int local_dist[MAX_WORD_LEN_DIST] = {0}; // Local distribution array

    while (word) {
        int len = strlen(word);
        if (len > 0) {
            int index = (len > MAX_WORD_LEN_DIST) ? MAX_WORD_LEN_DIST - 1 : len - 1;
            local_dist[index]++;
        }
        word = strtok(NULL, " \t\r\n.,;:!?\"'()[]{}<>");
    }

    pthread_mutex_lock(&task->stats->lock);
    for (int i = 0; i < MAX_WORD_LEN_DIST; i++) {
        task->stats->word_length_dist[i] += local_dist[i];
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double duration = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;
    update_worker_metrics(&task->stats->metrics->word_len_dist_metrics, duration);
    pthread_mutex_unlock(&task->stats->lock);

    printf("[TASK] word_length_dist processed URL %d (%.2f ms)\n", 
           task->url_index, duration);
    free(copy);
    return NULL;
}

// Worker function to compute the distribution of starting characters of words
void* worker_word_start_char(void *arg) {
    ThreadTask *task = (ThreadTask *)arg;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    char *copy = strdup(task->data);
    char *word = strtok(copy, " \t\r\n.,;:!?\"'()[]{}<>");
    int local_start[ALPHABET_SIZE] = {0}; // Local array for starting chars

    while (word) {
        if (isalpha(word[0])) {
            char c = tolower(word[0]);
            local_start[c - 'a']++;
        }
        word = strtok(NULL, " \t\r\n.,;:!?\"'()[]{}<>");
    }

    pthread_mutex_lock(&task->stats->lock);
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        task->stats->word_start_char[i] += local_start[i];
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double duration = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;
    update_worker_metrics(&task->stats->metrics->word_start_char_metrics, duration);
    pthread_mutex_unlock(&task->stats->lock);

    printf("[TASK] word_start_char processed URL %d (%.2f ms)\n", 
           task->url_index, duration);
    free(copy);
    return NULL;
}

// Worker function to compute the average word length
void* worker_avg_word_length(void *arg) {
    ThreadTask *task = (ThreadTask *)arg;
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    char *copy = strdup(task->data);
    char *word = strtok(copy, " \t\r\n.,;:!?\"'()[]{}<>");
    size_t total_chars = 0;
    size_t word_count = 0;

    while (word) {
        if (strlen(word) > 0) {
            total_chars += strlen(word);
            word_count++;
        }
        word = strtok(NULL, " \t\r\n.,;:!?\"'()[]{}<>");
    }

    pthread_mutex_lock(&task->stats->lock);
    if (word_count > 0) {
        // Update running average using previous total and new data
        size_t total_words = task->stats->total_word_count + word_count;
        double current_total = task->stats->avg_word_length * task->stats->total_word_count;
        task->stats->avg_word_length = (current_total + total_chars) / total_words;
    }
    clock_gettime(CLOCK_MONOTONIC, &end);
    double duration = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;
    update_worker_metrics(&task->stats->metrics->avg_word_len_metrics, duration);
    pthread_mutex_unlock(&task->stats->lock);

    printf("[TASK] avg_word_length processed URL %d (%.2f ms)\n", 
           task->url_index, duration);
    free(copy);
    return NULL;
}

// Print all collected statistics in a formatted way
void print_statistics(const SharedStats *stats) {
    printf("\n=== Text Processing Statistics ===\n");
    printf("[STATS] Total words processed: %'zu\n", stats->total_word_count);
    printf("[STATS] Longest word: '%s' (%zu characters)\n", 
           stats->longest_word, strlen(stats->longest_word));
    printf("[STATS] Total sentences: %'zu\n", stats->sentence_count);
    printf("[STATS] Average word length: %.2f characters\n", stats->avg_word_length);

    // Print character frequency
    printf("\n[STATS] Character Frequency (a-z):\n");
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        printf("%c: %d\t", 'a' + i, stats->char_frequency[i]);
        if ((i + 1) % 6 == 0) printf("\n");
    }

    // Print word length distribution
    printf("\n\n[STATS] Word Length Distribution:\n");
    for (int i = 0; i < MAX_WORD_LEN_DIST; i++) {
        int upper = (i == MAX_WORD_LEN_DIST - 1) ? 999 : i + 1;
        int lower = i + 1;
        printf("%2d-%-3d: %d\t", lower, upper, stats->word_length_dist[i]);
        if ((i + 1) % 4 == 0) printf("\n");
    }

    // Print word start character distribution
    printf("\n\n[STATS] Words Starting With Each Letter:\n");
    for (int i = 0; i < ALPHABET_SIZE; i++) {
        printf("%c: %d\t", 'A' + i, stats->word_start_char[i]);
        if ((i + 1) % 6 == 0) printf("\n");
    }

    // Print palindromes if any were found
    if (stats->palindrome_count > 0) {
        printf("\n\n[STATS] Palindromes Found (%d unique):\n", stats->palindrome_count);
        for (int i = 0; i < stats->palindrome_count; i++) {
            printf("%s (%d)\t", stats->palindromes[i].word, stats->palindromes[i].count);
            if ((i + 1) % 5 == 0) printf("\n");
        }
    }

    // Print the top N most frequent words
    WordFreq *sorted_words = malloc(stats->all_words_count * sizeof(WordFreq));
    memcpy(sorted_words, stats->all_words, stats->all_words_count * sizeof(WordFreq));
    qsort(sorted_words, stats->all_words_count, sizeof(WordFreq), compare_word_freq);

    int count_to_show = stats->all_words_count > TOP_N_WORDS ? TOP_N_WORDS : stats->all_words_count;

    printf("\n\n[STATS] Top %d Words:\n", count_to_show);
    printf("%-20s %10s %12s\n", "Word", "Count", "Frequency");
    printf("----------------------------------------\n");

    for (int i = 0; i < count_to_show; i++) {
        double freq = (double)sorted_words[i].count / stats->total_word_count * 100;
        printf("%-20s %10d %10.2f%%\n", sorted_words[i].word, sorted_words[i].count, freq);
    }

    free(sorted_words);
}