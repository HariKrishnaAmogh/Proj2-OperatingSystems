# Project 2 — Multithreaded Producer-Consumer Pipeline
 
**Student:** Amogh Hari Krishna  
**Course:** CMSC 421: Introduction to Operating Systems  
**Instructor:** Maya Larson  
**Last Update:** October 14, 2025
 
---
 
> [!WARNING]
> **This project cannot and should not be run outside of its intended environment.**
> It relies on Linux kernel-space POSIX message queues and POSIX threads (`pthreads`) that require a Linux kernel to function. It will not build or run correctly on macOS or Windows.
>
> Source files have been stripped of certain implementation details to comply with course academic integrity policies. This repo is intended as a portfolio reference only.
 
---
 
## Overview
 
This project implements a three-stage multithreaded producer-consumer pipeline in C. Each stage runs as an independent thread and communicates with adjacent stages via Linux kernel-space POSIX message queues, which act as circular FIFO buffers. The pipeline simulates a realistic message-passing system with synchronized production, validation, translation, and consumption of data.
 
---
 
## Pipeline Architecture
 
```
[Stage 1: Producer]──Mailbox 1──▶ [Stage 2: Consumer/Producer] ──Mailbox 2──▶ [Stage 3: Consumer]
```
 
- **Stage 1** generates raw READ/WRITE requests and sends them into Mailbox 1
- **Stage 2** receives from Mailbox 1, translates READ/WRITE to `0`/`1`, logs to `stage2.log`, and forwards into Mailbox 2 using message priority
- **Stage 3** receives completed messages from Mailbox 2 and writes final output to `stage3.log`
 
---
 
## Key Concepts
 
- **POSIX Threads (`pthreads`)** — each pipeline stage runs as a concurrent thread
- **Mutexes & Condition Variables** — used in Stage 1 to prevent race conditions during production and to coordinate thread startup/teardown
- **POSIX Message Queues** — kernel-managed, priority-aware FIFO queues used as the mailboxes between stages
- **Simulated Compute Workload** — Stage 2 runs a bubble sort (`bsort()`) to simulate processing time
- **Timestamped Logging** — all stages use `ctime()` for consistent, clean log formatting across `stage2.log` and `stage3.log`
 
---
 
## Project Structure
 
```
/usr/src/proj2/
├── proj2.c         # Main source — all three pipeline stages
├── stage2.log      # Output log from Stage 2
├── stage3.log      # Output log from Stage 3
├── output.txt      # Captured console output
├── Worklog         # Development log
└── README.md
```
 
---
 
## Build & Run
 
> ⚠️ **Linux VM environment required.** These steps are documented for reference only.
 
**Build:**
```bash
gcc -o proj2 proj2.c -lpthread -lrt
```
 
**Run:**
```bash
./proj2
```
 
---
 
## Commit History
 
| Date | Message |
|------|---------|
| Oct 14, 2025 | Finished Project 2 \| Includes all files: stage2.log, stage3.log, proj2.c, output.txt, worklog |
| Oct 14, 2025 | Implemented Stage 2 & Stage 3 and updated worklog |
| Oct 11, 2025 | Implemented Stage 1 Producer function in proj2.c and updated worklog |
 
---
 
## Notes
 
- Development spanned October 11–14, 2025 across two focused sessions
- Each stage was isolated and tested independently before integrating the full pipeline
- A key debugging challenge involved a log formatting mismatch in Stage 2 (READ/WRITE strings vs. 0/1 integers) that was caught by comparing against expected sample output
- Linux message queues handle internal priority sorting and synchronization automatically — Stage 2 leverages this by passing each message's priority directly as the MQ priority value
- Source files have been partially redacted to comply with CMSC 421 academic integrity guidelines — this repo is a portfolio showcase, not a complete runnable submission
- See [`Worklog`](./Worklog) for a session-by-session breakdown of hours and tasks