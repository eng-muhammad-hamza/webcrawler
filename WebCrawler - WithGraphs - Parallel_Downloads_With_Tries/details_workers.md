# **Text Processing Workers - Code Explanation**

## **1. Introduction**
This implementation provides a set of parallel text processing workers that analyze various linguistic features from input text. Key features:

- **Thread-safe statistics collection** using mutex-protected shared data
- **Multiple analysis dimensions** (word counts, character frequencies, palindromes, etc.)
- **Dynamic memory management** for growing datasets
- **Performance metrics** tracking for each worker type

## **2. Key Data Structures**

### **WordFreq**
```c
typedef struct {
    char word[MAX_WORD_LENGTH];  // Stores the word text
    int count;                   // Frequency count
} WordFreq;
```
- Tracks word occurrences and frequencies

### **SharedStats**
```c
typedef struct {
    pthread_mutex_t lock;        // Central synchronization mutex
    size_t total_word_count;     // Aggregate word count
    char longest_word[MAX_WORD_LENGTH];  // Tracks longest word found
    WordFreq *all_words;         // Dynamic array of unique words
    int all_words_count;         // Count of unique words
    int all_words_capacity;      // Current array capacity
    WordFreq top_words[TOP_N_WORDS];  // Fixed array for top words
    // ... (additional statistical fields)
    SystemMetrics *metrics;      // Performance tracking
} SharedStats;
```
- Central repository for all analysis results
- Uses mutex for thread-safe updates
- Combines fixed and dynamic storage approaches

## **3. Core Worker Functions**

### **Initialization & Cleanup**
```c
void init_shared_stats(SharedStats *stats, SystemMetrics *metrics)
```
- Initializes all fields and allocates initial memory
- Sets up mutex for thread safety

```c
void free_shared_stats(SharedStats *stats)
```
- Releases all dynamically allocated memory
- Destroys mutex

### **Common Processing Pattern**
All workers follow this general structure:
1. Start timer
2. Make thread-safe copy of input data
3. Perform local analysis
4. Lock mutex and update shared stats
5. Record metrics and unlock
6. Clean up and return

### **Key Workers**

#### **Word Counter**
```c
void* worker_word_count(void *arg)
```
- Tokenizes text and counts words
- Updates total word count

#### **Longest Word Finder**
```c
void* worker_longest_word(void *arg)
```
- Compares word lengths
- Updates longest word record

#### **Top Words Tracker**
```c
void* worker_top_words(void *arg)
```
- Maintains dynamic array of unique words
- Case-insensitive counting
- Uses realloc for array growth

#### **Sentence Counter**
```c
void* worker_sentence_count(void *arg)
```
- Counts sentence-ending punctuation
- Handles consecutive punctuation marks

#### **Character Frequency**
```c
void* worker_char_frequency(void *arg)
```
- Tracks a-z frequency (case-insensitive)
- Uses efficient array indexing

#### **Palindrome Detector**
```c
void* worker_palindrome_detector(void *arg)
```
- Checks words for palindrome property
- Maintains separate dynamic array

#### **Word Length Distribution**
```c
void* worker_word_length_dist(void *arg)
```
- Tracks counts by word length
- Uses fixed-size array with overflow bucket

#### **Word Start Character**
```c
void* worker_word_start_char(void *arg)
```
- Counts initial letters (a-z)
- Case-insensitive

#### **Average Word Length**
```c
void* worker_avg_word_length(void *arg)
```
- Computes running average
- Weighted by total words

## **4. Critical Implementation Details**

### **Thread Safety**
- All shared data access protected by mutex
- Local analysis before critical sections
- Minimal time spent in locked sections

### **Memory Management**
- Dynamic arrays grow exponentially (GROWTH_FACTOR = 2)
- Initial capacities defined for reasonable defaults
- All allocations properly freed

### **Performance Tracking**
- Monotonic clock for precise timing
- Metrics updated per-task
- Duration calculated in milliseconds

### **Text Processing**
- Consistent tokenization across workers
- Handles common punctuation and whitespace
- Case normalization where appropriate

## **5. Statistics Reporting**

```c
void print_statistics(const SharedStats *stats)
```
- Formats and prints all collected data
- Includes:
  - Basic counts (words, sentences)
  - Character and word distributions
  - Palindrome findings
  - Top word frequencies
- Uses columnar formatting for readability
- Properly handles empty/partial results

## **6. Key Optimizations**

1. **Local Analysis** - Workers do most processing before locking
2. **Batched Updates** - Aggregate changes before critical sections
3. **Efficient Storage** - Uses arrays for frequency counts
4. **Growth Strategy** - Exponential array resizing
5. **Case Handling** - Early normalization for consistency