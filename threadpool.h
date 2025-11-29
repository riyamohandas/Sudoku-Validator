#ifndef THREADPOOL_H
#define THREADPOOL_H

/*
 * threadpool.h
 *
 * Simple thread-pool API for POSIX (pthreads).
 *
 * This header pairs with a `threadpool.c` implementation that provides:
 *  - a fixed-size worker pool
 *  - a thread-safe FIFO task queue
 *  - functions to initialize, submit tasks, and shutdown the pool
 *
 * The design is intentionally minimal and fits the Sudoku validator example:
 * each task is a `board_task_t *` (allocated by caller and freed by worker).
 *
 * Usage (example):
 *   threadpool_init(n_workers);
 *   threadpool_submit(task);   // task is board_task_t*
 *   ...
 *   threadpool_shutdown();     // blocks until workers exit
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <stdbool.h>

/* ---------- Task type (specific to Sudoku project) ---------- */

/* Task representing a Sudoku board to validate.
 * The worker is responsible for freeing the task after processing it.
 */
typedef struct {
    int id;             /* user-visible task id */
    int board[9][9];    /* 9x9 Sudoku board */
} board_task_t;

/* ---------- Thread-pool opaque handle ---------- */
/* Users can keep an explicit handle if desired; the implementation may
 * manage a single global pool internally (matching earlier example).
 *
 * If you need multiple pools, change the implementation to return and
 * accept a pointer to threadpool_t from the API functions.
 */
typedef struct threadpool threadpool_t;

/* ---------- API ---------- */

/* Initialize the global thread-pool with `num_workers`. Must be called
 * before submitting tasks. Returns true on success, false on error.
 *
 * If you prefer an explicit handle, later we can change this to:
 *   threadpool_t *threadpool_create(int num_workers);
 */
bool threadpool_init(int num_workers);

/* Submit a task to the pool. The task pointer may be NULL to indicate a
 * sentinel; however, in typical use you should submit valid board_task_t*.
 *
 * Returns true on success, false on error (e.g. pool not initialized, OOM).
 *
 * Ownership: the caller transfers ownership of *task* to the pool/worker.
 * The worker must free(task) after processing.
 */
bool threadpool_submit(board_task_t *task);

/* Gracefully shutdown the pool:
 *  - enqueue one NULL sentinel per worker (causing each worker to exit)
 *  - join all worker threads
 *  - destroy internal synchronization primitives
 *
 * This call blocks until all workers have exited and resources are freed.
 */
void threadpool_shutdown(void);

/* Optional: get number of worker threads (0 if not initialized) */
int threadpool_worker_count(void);

/* Optional: convenience wrapper to create a task from a 2D array.
 * Returns a heap-allocated board_task_t* or NULL on error.
 * Caller must not free the returned pointer; the worker frees it.
 */
board_task_t *threadpool_create_board_task(int id, int board[9][9]);

/* Optional: Helper to free a task if submit fails and caller needs to clean up.
 * (If submit succeeds, the pool will free it.)
 */
void threadpool_free_task(board_task_t *task);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* THREADPOOL_H */
