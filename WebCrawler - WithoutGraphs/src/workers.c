#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "workers.h"

typedef struct {
    const char *data;     // Input text to process
    SharedStats *stats;   // Shared statistics structure
} WorkerArgs;

void init_shared_stats(SharedStats *stats) {
    pthread_mutex_init(&stats->lock, NULL);                         // Initialize mutex
    stats->total_word_count = 0;                                    // Reset word count
    stats->longest_word[0] = '\0';                                  // Empty longest word
    stats->top_word_count = 0;                                      // No top words yet
    
    // Initialize dynamic array for words
    stats->all_words = malloc(INITIAL_WORD_CAPACITY * sizeof(WordFreq));
    stats->all_words_count = 0;                                     // No words stored yet
    stats->all_words_capacity = INITIAL_WORD_CAPACITY;              // Initial capacity
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
    WorkerArgs *args = (WorkerArgs *)arg;                           // Cast argument
    char *word;
    char *copy = strdup(args->data);                                // Create a modifiable copy
    word = strtok(copy, " \t\r\n.,;:!?\"'()[]{}<>");                // Tokenize (split into words)

    size_t local_count = 0;
    while (word) {
        if (strlen(word) > 0) local_count++;                        // Count non-empty words
        word = strtok(NULL, " \t\r\n.,;:!?\"'()[]{}<>");            // Next word
    }

    // Safely update total word count
    pthread_mutex_lock(&args->stats->lock);
    args->stats->total_word_count += local_count;
    pthread_mutex_unlock(&args->stats->lock);

    free(copy);                                                     // Free the copied string
    return NULL;
}

void* worker_longest_word(void *arg) {
    WorkerArgs *args = (WorkerArgs *)arg;
    char *copy = strdup(args->data);                                // Copy for tokenization
    char *word = strtok(copy, " \t\r\n.,;:!?\"'()[]{}<>");          // Split into words

    while (word) {
        if (strlen(word) > 0)
            update_longest_word(args->stats, word);                 // Update longest word
        word = strtok(NULL, " \t\r\n.,;:!?\"'()[]{}<>");            // Next word
    }

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
    WorkerArgs *args = (WorkerArgs *)arg;
    char *copy = strdup(args->data);
    char *word = strtok(copy, " \t\r\n.,;:!?\"'()[]{}<>");

    while (word) {
        // Convert word to lowercase
        for (char *p = word; *p; ++p) *p = tolower(*p);
        if (strlen(word) > 0) {
            update_top_words(args->stats, word);
        }
        word = strtok(NULL, " \t\r\n.,;:!?\"'()[]{}<>");
    }

    free(copy);
    return NULL;
}

void print_statistics(const SharedStats *stats) {
    // First, sort all words by frequency to get the top words
    WordFreq *sorted_words = malloc(stats->all_words_count * sizeof(WordFreq));
    memcpy(sorted_words, stats->all_words, stats->all_words_count * sizeof(WordFreq));
    qsort(sorted_words, stats->all_words_count, sizeof(WordFreq), compare_word_freq);
    
    // Determine how many words to show (up to TOP_N_WORDS)
    int count_to_show = stats->all_words_count;
    if (count_to_show > TOP_N_WORDS) {
        count_to_show = TOP_N_WORDS;
    }

    printf("\n=== Final Statistics ===\n");
    printf("Total Words Counted: %zu\n", stats->total_word_count);
    printf("Longest Word Found : %s\n", stats->longest_word);
    printf("Top %d Words:\n", count_to_show);
    for (int i = 0; i < count_to_show; i++) {
        printf("  %s (%d)\n", sorted_words[i].word, sorted_words[i].count);
    }
    
    free(sorted_words);
}