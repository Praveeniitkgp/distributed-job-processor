/*/=====================================
Assignment 5 Submission
Name: Praven Kumar
Roll number: 22CS10054
=====================================
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

#define MAX_TASKS 100
#define MAX_TASK_LEN 50
#define MAX_CLIENTS 10
#define PORT 8080
#define BUFFER_SIZE 1024

// Task structure
typedef struct {
    char task[MAX_TASK_LEN];
    int assigned;  // 0: not assigned, 1: assigned but not completed
    int completed; // 0: not completed, 1: completed
    int client_pid; // PID of client assigned to this task
} Task;

// Client structure
typedef struct {
    int sock_fd;
    int pid;       // Child process PID
    int has_task;  // 0: no task, 1: has a task
    int task_id;   // Index of assigned task
} Client;

// Global variables
Task tasks[MAX_TASKS];
Client clients[MAX_CLIENTS];
int task_count = 0;
int client_count = 0;

// Function prototypes
void load_tasks(const char* filename);
void handle_client(int client_sock, int client_index);
void handle_sigchld(int sig);
int find_available_task();
int add_client(int sock_fd);
void remove_client(int index);
int find_client_by_pid(pid_t pid);

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <task_file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Initialize task and client arrays
    memset(tasks, 0, sizeof(tasks));
    memset(clients, 0, sizeof(clients));

    // Load tasks from file
    load_tasks(argv[1]);
    printf("Loaded %d tasks\n", task_count);

    // Set up SIGCHLD handler to avoid zombie processes
    struct sigaction sa;
    sa.sa_handler = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    // Create server socket
    int server_fd;
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Server address structure
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind the socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_fd, 5) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server started on port %d\n", PORT);

    // Set server socket to non-blocking
    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

    // Main server loop
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock;

        // Accept connections (non-blocking)
        client_sock = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            if (errno != EWOULDBLOCK && errno != EAGAIN) {
                perror("accept");
            }
        } else {
            printf("New connection from %s:%d\n", 
                   inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            
            // Add client to our array
            int client_index = add_client(client_sock);
            if (client_index == -1) {
                // Too many clients
                char *msg = "Server is busy. Try again later.\n";
                send(client_sock, msg, strlen(msg), 0);
                close(client_sock);
                continue;
            }

            // Set client socket to non-blocking
            int flags = fcntl(client_sock, F_GETFL, 0);
            fcntl(client_sock, F_SETFL, flags | O_NONBLOCK);

            // Fork a child process to handle this client
            pid_t pid = fork();
            if (pid < 0) {
                perror("fork");
                remove_client(client_index);
                close(client_sock);
            } else if (pid == 0) {
                // Child process
                close(server_fd); // Child doesn't need server socket
                
                // Handle client communication
                handle_client(client_sock, client_index);
                
                // Clean up and exit
                close(client_sock);
                exit(EXIT_SUCCESS);
            } else {
                // Parent process
                clients[client_index].pid = pid;
                close(client_sock); // Parent doesn't need client socket
            }
        }

        // Brief pause to reduce CPU usage
        usleep(10000); // 10ms
    }

    // Clean up
    close(server_fd);
    return 0;
}

// Load tasks from file
void load_tasks(const char* filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening task file");
        exit(EXIT_FAILURE);
    }

    char buffer[MAX_TASK_LEN];
    while (fgets(buffer, MAX_TASK_LEN, file) && task_count < MAX_TASKS) {
        // Remove newline if present
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n') {
            buffer[len-1] = '\0';
        }
        
        strcpy(tasks[task_count].task, buffer);
        tasks[task_count].assigned = 0;
        tasks[task_count].completed = 0;
        tasks[task_count].client_pid = -1;
        task_count++;
    }

    fclose(file);
}

// Handle client requests
void handle_client(int client_sock, int client_index) {
    char buffer[BUFFER_SIZE] = {0};
    ssize_t bytes_read;

    while (1) {
        // Clear buffer
        memset(buffer, 0, BUFFER_SIZE);
        
        // Read client request (non-blocking)
        bytes_read = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_read < 0) {
            if (errno != EWOULDBLOCK && errno != EAGAIN) {
                perror("recv");
                break;
            }
            // No data available, continue loop
            usleep(10000); // 10ms pause
            continue;
        } else if (bytes_read == 0) {
            // Client disconnected
            printf("Client disconnected\n");
            break;
        } else {
            // Process client message
            buffer[bytes_read] = '\0';
            printf("Received: %s\n", buffer);

            // Remove newline character if present
            size_t len = strlen(buffer);
            if (len > 0 && buffer[len-1] == '\n') {
                buffer[len-1] = '\0';
            }

            if (strcmp(buffer, "GET_TASK") == 0) {
                // Check if client already has a task
                if (clients[client_index].has_task) {
                    char *msg = "Error: You already have an unfinished task\n";
                    send(client_sock, msg, strlen(msg), 0);
                    continue;
                }

                // Find an available task
                int task_id = find_available_task();
                if (task_id != -1) {
                    // Assign task to client
                    tasks[task_id].assigned = 1;
                    tasks[task_id].client_pid = getpid();
                    clients[client_index].has_task = 1;
                    clients[client_index].task_id = task_id;

                    // Send task to client
                    char task_msg[BUFFER_SIZE];
                    sprintf(task_msg, "Task: %s", tasks[task_id].task);
                    send(client_sock, task_msg, strlen(task_msg), 0);
                } else {
                    // No tasks available
                    send(client_sock, "No tasks available", 18, 0);
                }
            } else if (strncmp(buffer, "RESULT ", 7) == 0) {
                // Process result
                if (!clients[client_index].has_task) {
                    char *msg = "Error: No task was assigned\n";
                    send(client_sock, msg, strlen(msg), 0);
                    continue;
                }

                int task_id = clients[client_index].task_id;
                tasks[task_id].completed = 1;
                printf("Task '%s' completed with result: %s\n", 
                       tasks[task_id].task, buffer + 7);

                // Mark client as not having a task
                clients[client_index].has_task = 0;
                clients[client_index].task_id = -1;
                
                // Acknowledge receipt of result
                send(client_sock, "Result received", 15, 0);
            } else if (strcmp(buffer, "exit") == 0) {
                // Client wishes to disconnect
                printf("Client requested to exit\n");
                send(client_sock, "Goodbye", 8, 0);
                break;
            } else {
                // Unknown command
                char *msg = "Unknown command. Use GET_TASK, RESULT <value>, or exit\n";
                send(client_sock, msg, strlen(msg), 0);
            }
        }
    }
}

// SIGCHLD handler to prevent zombie processes
void handle_sigchld(int sig) {
    // Unused parameter - add (void) to suppress warning
    (void)sig;
    
    pid_t pid;
    int status;
    
    // Wait for all terminated children
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        printf("Child process %d terminated\n", pid);
        
        // Find the client associated with this PID
        int client_index = find_client_by_pid(pid);
        if (client_index != -1) {
            // If client had a task, mark it as available again
            if (clients[client_index].has_task) {
                int task_id = clients[client_index].task_id;
                tasks[task_id].assigned = 0;
                tasks[task_id].client_pid = -1;
            }
            
            // Remove client from our array
            remove_client(client_index);
        }
    }
}

// Find an available task
int find_available_task() {
    for (int i = 0; i < task_count; i++) {
        if (!tasks[i].assigned && !tasks[i].completed) {
            return i;
        }
    }
    return -1; // No tasks available
}

// Add a new client to our array
int add_client(int sock_fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].sock_fd == 0) {
            clients[i].sock_fd = sock_fd;
            clients[i].pid = 0;
            clients[i].has_task = 0;
            clients[i].task_id = -1;
            client_count++;
            return i;
        }
    }
    return -1; // No space for more clients
}

// Remove a client from our array
void remove_client(int index) {
    if (index >= 0 && index < MAX_CLIENTS) {
        clients[index].sock_fd = 0;
        clients[index].pid = 0;
        clients[index].has_task = 0;
        clients[index].task_id = -1;
        client_count--;
    }
}

// Find client by PID
int find_client_by_pid(pid_t pid) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].pid == pid) {
            return i;
        }
    }
    return -1; // Not found
}