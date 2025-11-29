# 🧩 Multithreaded Sudoku Validator using C

A simple multithreaded **Sudoku solution validator** built using **POSIX threads (pthreads)** and a custom **thread-pool**.  
The program allows the user to input one or more Sudoku boards and validates them concurrently using worker threads.

---

## 🚀 Features

- Validates **rows**, **columns**, and **3×3 subgrids**
- Uses a **thread-pool** instead of creating many threads manually
- Supports **multiple Sudoku boards** through user input
- Demonstrates key **Operating Systems concepts**:
  - Multithreading  
  - Mutexes  
  - Condition variables  
  - Task queues  
  - Concurrent scheduling  

---

## 🛠️ Build Instructions

Compile using GCC or G++:

```bash
gcc main.c threadpool.c -o sudoku -pthread
