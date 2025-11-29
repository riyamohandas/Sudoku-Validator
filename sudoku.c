

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#define MAX_LINE 512


typedef struct {
    int id;             
    int board[9][9];
} board_task_t;

typedef struct task_node {
    board_task_t *task;          
    struct task_node *next;
} task_node_t;

/* Task queue with mutex + cond */
typedef struct {
    task_node_t *head;
    task_node_t *tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} task_queue_t;

/* ---------- Globals ---------- */

task_queue_t queue;
int worker_count = 4;
pthread_t *workers = NULL;

/* ---------- Queue functions ---------- */

void queue_init(task_queue_t *q) {
    q->head = q->tail = NULL;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->cond, NULL);
}

void queue_destroy(task_queue_t *q) {
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->cond);
}

/* Enqueue task (NULL allowed as sentinel) */
void enqueue_task(task_queue_t *q, board_task_t *task) {
    task_node_t *node = malloc(sizeof(task_node_t));
    if (!node) { perror("malloc"); exit(1); }
    node->task = task;
    node->next = NULL;

    pthread_mutex_lock(&q->mutex);
    if (q->tail == NULL) {
        q->head = q->tail = node;
    } else {
        q->tail->next = node;
        q->tail = node;
    }
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->mutex);
}

board_task_t *dequeue_task(task_queue_t *q) {
    pthread_mutex_lock(&q->mutex);
    while (q->head == NULL) {
        pthread_cond_wait(&q->cond, &q->mutex);
    }
    task_node_t *node = q->head;
    q->head = node->next;
    if (q->head == NULL) q->tail = NULL;
    pthread_mutex_unlock(&q->mutex);

    board_task_t *task = node->task;
    free(node);
    return task;
}

/* ---------- Validator for single board (rows, cols, 3x3) ---------- */

bool validate_board_internal(int board[9][9], char *reason, size_t reason_len) {
    // Check rows
    for (int r = 0; r < 9; ++r) {
        int seen[9] = {0};
        for (int c = 0; c < 9; ++c) {
            int v = board[r][c];
            if (v <= 0 || v > 9) {
                snprintf(reason, reason_len, "Invalid number %d at row %d col %d", v, r+1, c+1);
                return false;
            }
            if (seen[v-1]) {
                snprintf(reason, reason_len, "Duplicate %d in row %d", v, r+1);
                return false;
            }
            seen[v-1] = 1;
        }
    }

    // Check columns
    for (int c = 0; c < 9; ++c) {
        int seen[9] = {0};
        for (int r = 0; r < 9; ++r) {
            int v = board[r][c];
            if (v <= 0 || v > 9) {
                snprintf(reason, reason_len, "Invalid number %d at row %d col %d", v, r+1, c+1);
                return false;
            }
            if (seen[v-1]) {
                snprintf(reason, reason_len, "Duplicate %d in column %d", v, c+1);
                return false;
            }
            seen[v-1] = 1;
        }
    }

    // Check 3x3 squares
    int idx = 0;
    for (int br = 0; br < 3; ++br) {
        for (int bc = 0; bc < 3; ++bc) {
            int seen[9] = {0};
            for (int r = br*3; r < br*3 + 3; ++r) {
                for (int c = bc*3; c < bc*3 + 3; ++c) {
                    int v = board[r][c];
                    if (v <= 0 || v > 9) {
                        snprintf(reason, reason_len, "Invalid number %d in 3x3 block %d at row %d col %d", v, idx+1, r+1, c+1);
                        return false;
                    }
                    if (seen[v-1]) {
                        snprintf(reason, reason_len, "Duplicate %d in 3x3 block %d (top-left row %d col %d)", v, idx+1, br*3+1, bc*3+1);
                        return false;
                    }
                    seen[v-1] = 1;
                }
            }
            ++idx;
        }
    }

    // All checks passed
    snprintf(reason, reason_len, "OK");
    return true;
}

/* ---------- Worker thread ---------- */

void *worker_loop(void *arg) {
    (void)arg;
    for (;;) {
        board_task_t *task = dequeue_task(&queue);
        if (task == NULL) {
            // NULL sentinel: exit worker
            break;
        }

        char reason[128] = {0};
        bool ok = validate_board_internal(task->board, reason, sizeof(reason));
        if (ok) {
            printf("[Worker %lu] Board %d: VALID\n", (unsigned long)pthread_self(), task->id);
        } else {
            printf("[Worker %lu] Board %d: INVALID -> %s\n", (unsigned long)pthread_self(), task->id, reason);
        }

        free(task); // free task after processing
    }
    return NULL;
}

/* ---------- Input parsing ---------- */

/* Trim newline end */
static void trim_newline(char *s) {
    size_t L = strlen(s);
    if (L == 0) return;
    if (s[L-1] == '\n') s[L-1] = '\0';
}

/* Read one board from stdin into 'out'. Returns true on success. */
bool read_board_from_stdin(int out[9][9], int board_index) {
    char line[MAX_LINE];
    printf("Enter board %d: type 9 rows each with 9 integers separated by spaces.\n", board_index);
    for (int r = 0; r < 9; ++r) {
        while (1) {
            printf(" Row %d: ", r+1);
            if (!fgets(line, sizeof(line), stdin)) {
                fprintf(stderr, "Input error or EOF\n");
                return false;
            }
            trim_newline(line);
            if (strlen(line) == 0) {
                // ignore empty line and re-prompt
                continue;
            }
            // parse tokens
            int vals[9];
            int count = 0;
            char *tok = strtok(line, " \t");
            bool parse_error = false;
            while (tok != NULL && count < 9) {
                errno = 0;
                char *endptr = NULL;
                long v = strtol(tok, &endptr, 10);
                if (endptr == tok || *endptr != '\0' || errno != 0) {
                    parse_error = true;
                    break;
                }
                vals[count++] = (int)v;
                tok = strtok(NULL, " \t");
            }
            if (parse_error || count != 9) {
                printf("  Invalid row input — please enter exactly 9 integers (e.g. \"6 2 4 5 3 9 1 8 7\"). Try again.\n");
                continue; // re-prompt same row
            }
            // copy to board
            for (int c = 0; c < 9; ++c) out[r][c] = vals[c];
            break; // next row
        }
    }
    return true;
}

/* ---------- Main: create pool, read user boards, submit tasks, shutdown ---------- */

int main(int argc, char *argv[]) {
    if (argc >= 2) {
        int n = atoi(argv[1]);
        if (n > 0) worker_count = n;
    }

    printf("Thread-pool Sudoku validator\n");
    printf("Workers: %d\n", worker_count);

    queue_init(&queue);

    workers = malloc(sizeof(pthread_t) * worker_count);
    if (!workers) { perror("malloc"); return 1; }

    for (int i = 0; i < worker_count; ++i) {
        if (pthread_create(&workers[i], NULL, worker_loop, NULL) != 0) {
            perror("pthread_create");
            return 1;
        }
    }

    /* Ask user how many boards to validate */
    int board_count = 0;
    while (1) {
        printf("How many Sudoku boards do you want to validate? ");
        char buf[64];
        if (!fgets(buf, sizeof(buf), stdin)) {
            fprintf(stderr, "Input error or EOF\n");
            return 1;
        }
        trim_newline(buf);
        if (strlen(buf) == 0) continue;
        char *endptr = NULL;
        long v = strtol(buf, &endptr, 10);
        if (endptr == buf || *endptr != '\0' || v <= 0) {
            printf("Please enter a positive integer.\n");
            continue;
        }
        board_count = (int)v;
        break;
    }

    for (int i = 1; i <= board_count; ++i) {
        int board[9][9];
        if (!read_board_from_stdin(board, i)) {
            fprintf(stderr, "Failed to read board %d; exiting.\n", i);
            // enqueue sentinels and cleanup workers before exit
            for (int w = 0; w < worker_count; ++w) enqueue_task(&queue, NULL);
            for (int w = 0; w < worker_count; ++w) pthread_join(workers[w], NULL);
            free(workers);
            queue_destroy(&queue);
            return 1;
        }
        board_task_t *task = malloc(sizeof(board_task_t));
        if (!task) { perror("malloc"); exit(1); }
        task->id = i;
        memcpy(task->board, board, sizeof(board));
        enqueue_task(&queue, task);
    }

    /* After submitting all tasks, send `NULL` sentinel for each worker so they exit */
    for (int i = 0; i < worker_count; ++i) {
        enqueue_task(&queue, NULL);
    }

    /* Wait for workers to finish */
    for (int i = 0; i < worker_count; ++i) {
        pthread_join(workers[i], NULL);
    }

    free(workers);
    queue_destroy(&queue);

    printf("All tasks completed. Exiting.\n");
    return 0;
}
