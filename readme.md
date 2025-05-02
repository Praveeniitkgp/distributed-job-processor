This project implements a distributed job processing system using a **non-blocking Task Queue Server** and **multiple Worker Clients**. The system assigns simple arithmetic tasks to worker clients who compute and return results.

## Features

- Central task queue read from config file
- Worker clients request and execute tasks
- Server handles multiple clients using `fcntl()` + `O_NONBLOCK`
- Forked child processes for parallel handling
- Prevents zombies via `SIGCHLD` handling
- Safe disconnection and error handling

## Files

- `server.c`: Task queue server implementation
- `client.c`: Worker client implementation
- `tasks.txt`: Sample task file
- `Makefile`: Build instructions

## How to Run

```bash
make        # Compiles server and client

./server tasks.txt     # Run the task queue server
./client               # Start a worker client
