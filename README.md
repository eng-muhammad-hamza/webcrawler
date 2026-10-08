# c-webcrawler

[![Language: C](https://img.shields.io/badge/Language-C-00599C.svg?style=flat-square&logo=c&logoColor=white)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Standard: C99 / C11](https://img.shields.io/badge/Standard-C99%20%2F%20C11-blue.svg?style=flat-square)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))
[![Platform: Linux / POSIX / WSL](https://img.shields.io/badge/Platform-Linux%20%7C%20POSIX%20%7C%20WSL-FCC624.svg?style=flat-square&logo=linux&logoColor=black)](https://www.kernel.org/)
[![Networking: libcurl](https://img.shields.io/badge/Networking-libcurl-073551.svg?style=flat-square&logo=curl&logoColor=white)](https://curl.se/libcurl/)
[![Concurrency: POSIX Threads](https://img.shields.io/badge/Concurrency-POSIX%20pthreads-informational.svg?style=flat-square)](https://man7.org/linux/man-pages/man7/pthreads.7.html)
[![Analytics: Python 3.8+](https://img.shields.io/badge/Analytics-Python%203.8%2B-3776AB.svg?style=flat-square&logo=python&logoColor=white)](https://www.python.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg?style=flat-square)](LICENSE)

A high-throughput, modular multithreaded web crawler and concurrent text-processing engine written in C. The application retrieves target URLs over HTTP via `libcurl`, manages bounded network and compute workloads through POSIX thread pools, and pipes retrieved payloads into concurrent analytical worker routines for linguistic and structural text analysis.

The repository includes five architectural iterations showing the progression from sequential single-thread downloads to bounded batch parallel downloads with retry backoff and automated performance metric visualizers.

---

## Architectural Progression

The project is structured into five distinct implementations, demonstrating incremental concurrency and reliability improvements:

| Directory | Download Model | Concurrency Limits & Network Controls | Worker Routines | Telemetry & Visualizations |
| :--- | :--- | :--- | :--- | :--- |
| `WebCrawler - WithoutGraphs` | Sequential (single-thread) | Blocking sequential `curl_easy_perform` | 3 core workers | Standard console stdout |
| `WebCrawler - WithGraphs - Simple` | Sequential (single-thread) | Blocking sequential `curl_easy_perform` | 3 core workers | `metrics.csv`, history logging, matplotlib dashboard |
| `WebCrawler - WithGraphs - Parallel_Downloads` | Concurrent (pthread per URL) | Unbounded thread per URL | 9 comprehensive workers | Worker-level timings, multi-panel dashboard |
| `WebCrawler - WithGraphs - Parallel_Downloads_With_Tries` | Concurrent (pthread per URL) | Unbounded thread per URL + 3 retries with backoff + HTTP Range headers | 9 comprehensive workers | Detailed timing breakdown per worker, historical tracking |
| `WebCrawler - WithGraphs - Parallel_Downloads_With_Tries_&_Batches` | Concurrent Batch Pool | Bounded connection pool (max 15 concurrent threads) + 3 retries + HTTP Range headers | 9 comprehensive workers | Full runtime telemetry, multi-run trend analysis |

---

## System Architecture

```
                    [ Input URL List (urls.txt) ]
                                  |
                                  v
                 +---------------------------------+
                 |       Download Subsystem        |
                 |  - Bounded thread pool (pthreads)|
                 |  - libcurl memory write buffers |
                 |  - Exponential backoff retries  |
                 |  - HTTP Range header clamping   |
                 +---------------------------------+
                                  |
                           Payload Buffers
                                  |
                                  v
                 +---------------------------------+
                 |   Dynamic Thread Dispatcher     |
                 |  - Configurable thread budget   |
                 |  - Fair distribution across     |
                 |    analytical worker queues     |
                 +---------------------------------+
                                  |
             +--------------------+--------------------+
             |                    |                    |
             v                    v                    v
      [ Task Queue 1 ]     [ Task Queue 2 ]     [ Task Queue N ]
             |                    |                    |
             v                    v                    v
      ( Worker Pool 1 )    ( Worker Pool 2 )    ( Worker Pool N )
             \                    |                   /
              +-------------------+------------------+
                                  | Mutex-Protected
                                  v
                 +---------------------------------+
                 |       Shared Aggregations       |
                 |    & Monotonic System Metrics   |
                 +---------------------------------+
                                  |
                     +------------+------------+
                     |                         |
                     v                         v
              [ Console Report ]       [ metrics.csv ]
                                               |
                                               v
                                      [ Python Visualizer ]
                                      - Trend lines
                                      - Worker time allocations
```

### Concurrency and Thread Model

1. **Network Ingestion (Producer)**:
   - Early iterations download URLs sequentially, while advanced iterations execute parallel downloads via dedicated pthread routines.
   - The batches variant restricts active connections to a bounded thread pool (`MAX_DOWNLOAD_THREADS = 15`) to prevent socket exhaustion and local resource saturation.
   - Implements bounded byte downloads via `CURLOPT_RANGE` (`0 - chunk_bytes`), network timeout thresholds (`CURLOPT_TIMEOUT`), and low-speed abort limits (`CURLOPT_LOW_SPEED_LIMIT`).
   - Failed requests undergo retry attempts with fixed interval backoff (`MAX_RETRIES = 3`).

2. **Work Scheduling (Dispatcher)**:
   - Total user-supplied thread budget is distributed evenly across analytical worker categories via `distribute_threads()`.
   - Dedicated task queues (`TaskQueue`) buffer `ThreadTask` structs for each analytical routine.

3. **Analytical Processing (Consumers)**:
   - Consumer threads dequeue tasks under mutex protection.
   - Processing logic tokenizes and aggregates data locally before acquiring the central mutex (`SharedStats.lock`), minimizing critical section contention.

4. **Telemetry Clocking**:
   - High-resolution monotonic timers (`clock_gettime(CLOCK_MONOTONIC, ...)`) record start, end, and duration measurements for each operational phase and worker routine.

---

## Analytical Worker Suite

The extended variants execute 9 parallel text processing workers:

1. **Word Counter (`worker_word_count`)**: Counts total non-empty tokens across whitespace and standard punctuation delimiters.
2. **Top N Words (`worker_top_words`)**: Dynamically allocates and sorts word frequency records to identify the top 10 most frequent words across all documents.
3. **Longest Word (`worker_longest_word`)**: Evaluates token lengths and tracks the longest single word discovered.
4. **Sentence Counter (`worker_sentence_count`)**: Scans for terminal sentence punctuation (`.`, `!`, `?`), accounting for consecutive delimiter runs.
5. **Character Frequency (`worker_char_frequency`)**: Maintains a case-insensitive direct-index distribution table for the English alphabet (`a-z`).
6. **Palindrome Detector (`worker_palindrome_detector`)**: Evaluates symmetrical tokens and logs occurrences in a dynamically expanding results array.
7. **Word Length Distribution (`worker_word_length_dist`)**: Bins token lengths into discrete slots from 1 through 20+ characters.
8. **Word Starting Character (`worker_word_start_char`)**: Analyzes initial alphabetical characters across all tokens.
9. **Average Word Length (`worker_avg_word_length`)**: Tracks running aggregate length and computes the global mean word length.

---

## Core Data Structures

- **`MemoryBlock`**: Dynamically allocated memory buffer passed as user data to libcurl's write callback (`write_callback`), handling dynamic reallocation as data packets stream in.
- **`DownloadTask`**: Context bundle passed to download threads containing target URL string, output buffer pointer, maximum byte limit, attempt counters, and mutex references.
- **`ThreadTask`**: Queue entry encapsulation containing payload string reference, function pointer (`WorkerFunction`), target stats handle, and source URL index.
- **`TaskQueue`**: Synchronized task container using a dynamic array, task counter, capacity limit, and a dedicated `pthread_mutex_t`.
- **`SharedStats`**: Central aggregation container holding token counts, dynamic arrays for unique words and palindromes, fixed-size frequency tables, and an internal mutex.
- **`SystemMetrics`**: Hardware clock telemetry struct logging nanosecond intervals for network downloads, dispatcher distribution, and worker execution times.

---

## Prerequisites

### C Toolchain and Libraries
- GCC or Clang with C99/C11 support
- GNU Make
- libcurl development headers (`libcurl4-openssl-dev` on Debian/Ubuntu, `curl-devel` on RHEL/Fedora)
- POSIX Threads (`pthreads`, included in `glibc`)

```bash
# Debian / Ubuntu / WSL
sudo apt-get update
sudo apt-get install build-essential libcurl4-openssl-dev

# Fedora / RHEL
sudo dnf install gcc make libcurl-devel
```

### Python Runtime (For Metrics & Visualization)
- Python 3.8+
- Required packages: `pandas`, `matplotlib`, `seaborn`

```bash
pip install pandas matplotlib seaborn
```

---

## Building and Running

Each iteration contains its own `Makefile`, header definitions, source files, and test URL inputs.

### 1. Build

Navigate to the desired variant directory and execute `make`:

```bash
cd "WebCrawler - WithGraphs - Parallel_Downloads_With_Tries_&_Batches"
make
```

This compiles object files into `bin/` and outputs the executable `bin/web_crawler`.

To remove compiled binaries and intermediate object files:

```bash
make clean
```

### 2. Execution Syntax

```bash
./bin/web_crawler <urls_file> <num_threads> <chunk_bytes>
```

#### Parameters:
- `<urls_file>`: Path to a newline-delimited plain-text file containing target HTTP/HTTPS URLs.
- `<num_threads>`: Total number of worker threads to allocate for analytical processing.
- `<chunk_bytes>`: Maximum number of bytes to download per URL (enforces `Range: bytes=0-<chunk_bytes>`).

#### Example Run:

```bash
./bin/web_crawler urls.txt 18 1048576
```

Sample output:
```text
============================================================
           MULTITHREADED WEB CRAWLER BENCHMARK              
============================================================
[CONFIG] Threads: 18 | Max Chunk: 1048576 bytes
[STATUS] Read 18 URLs from urls.txt

[PHASE] Concurrent Download Stage (Batch Pool: 15)
[DOWNLOAD] 001: https://www.gutenberg.org/cache/epub/76267/pg76267.txt [OK] 1048576 bytes (1 attempts)
[DOWNLOAD] 002: https://www.gutenberg.org/cache/epub/76263/pg76263.txt [OK] 1048576 bytes (1 attempts)
...
[STATUS] Downloads completed: 18 success, 0 failed

[PHASE] Distributing 18 threads
[THREADS] Distribution:
  word_count     : 2 threads
  top_words      : 2 threads
  longest_word   : 2 threads
  sentence_count : 2 threads
  char_frequency : 2 threads
  palindrome_det : 2 threads
  word_len_dist  : 2 threads
  word_start_char: 2 threads
  avg_word_len   : 2 threads

[PHASE] Processing tasks
[STATUS] All tasks completed

=== Final Results ===
Total Words Processed    : 1,489,120
Total Sentences Processed: 84,210
Average Word Length      : 4.82 chars
Longest Word             : characteristically

=== System Metrics ===
URL Download Phase       : 3120.45 ms
Thread Distribution Phase: 0.04 ms
Thread Execution Phase   : 412.18 ms
Total Runtime            : 3532.67 ms
```

---

## Metrics and Visualizations

For versions with graphing enabled (`WithGraphs`), the crawler exports performance data upon completion:
- `metrics.csv`: Snapshot of the most recent execution, including network throughput, execution phase durations, and individual worker runtime totals.
- `metrics_history.csv`: Append-only time-series ledger documenting changes in performance across subsequent executions.

To generate the analytical charts:

```bash
# Generate per-phase breakdown and worker execution charts for the current run
python3 visualize_metrics.py

# Generate multi-run historical trend analysis across recorded runs
python3 analyze_runs.py
```

### Generated Artifacts
- `enhanced_metrics_dashboard.png`: Multi-panel visualization detailing system phase allocations, download transfer rates, worker execution time breakdown, and word length distributions.
- `comprehensive_trend_analysis.png`: Time-series plots illustrating total runtime variance, network download stability, and processing throughput across multiple benchmark runs.

---

## Repository Structure

```
.
├── .gitignore
├── README.md
├── WebCrawler - WithoutGraphs/
│   ├── Makefile
│   ├── urls.txt
│   ├── include/
│   │   ├── thread_pool.h
│   │   └── workers.h
│   └── src/
│       ├── main.c
│       ├── thread_pool.c
│       └── workers.c
├── WebCrawler - WithGraphs - Simple/
│   ├── Makefile
│   ├── urls.txt
│   ├── metrics.csv
│   ├── metrics_history.csv
│   ├── analyze_runs.py
│   ├── visualize_metrics.py
│   ├── include/
│   │   ├── metrics.h
│   │   ├── thread_pool.h
│   │   └── workers.h
│   └── src/
│       ├── main.c
│       ├── metrics.c
│       ├── thread_pool.c
│       └── workers.c
├── WebCrawler - WithGraphs - Parallel_Downloads/
│   ├── ... (Full 9-worker suite, parallel downloads)
├── WebCrawler - WithGraphs - Parallel_Downloads_With_Tries/
│   ├── ... (Parallel downloads with retry logic and backoff)
└── WebCrawler - WithGraphs - Parallel_Downloads_With_Tries_&_Batches/
    ├── Makefile
    ├── urls.txt
    ├── analyze_runs.py
    ├── visualize_metrics.py
    ├── include/
    │   ├── metrics.h
    │   ├── thread_pool.h
    │   └── workers.h
    └── src/
        ├── main.c
        ├── metrics.c
        ├── thread_pool.c
        └── workers.c
```

---

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
