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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define SERVER_IP "127.0.0.1"

// Function to perform arithmetic operation
int calculate(int a, int b, char op) {
    switch (op) {
        case '+': return a + b;
        case '-': return a - b;
        case '*': return a * b;
        case '/': return (b != 0) ? a / b : 0; // Avoid division by zero
        default: return 0;
    }
}

int main(void) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    
    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return -1;
    }
    
    // Set up server address structure
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    
    // Convert IPv4 address from text to binary form
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        return -1;
    }
    
    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        return -1;
    }
    
    printf("Connected to server at %s:%d\n", SERVER_IP, PORT);
    
    // Set socket to non-blocking mode
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    // Main client loop
    while (1) {
        printf("\nOptions:\n");
        printf("1. Request a task\n");
        printf("2. Exit\n");
        printf("Enter choice: ");
        
        int choice;
        scanf("%d", &choice);
        getchar(); // Consume newline
        
        if (choice == 1) {
            // Request a task
            send(sock, "GET_TASK", 8, 0);
            printf("Requesting task...\n");
            
            // Wait for server response
            memset(buffer, 0, BUFFER_SIZE);
            int received = 0;
            
            // Try to receive until we get data or timeout
            int timeout_counter = 0;
            while (!received && timeout_counter < 50) {
                ssize_t bytes_read = recv(sock, buffer, BUFFER_SIZE - 1, 0);
                
                if (bytes_read > 0) {
                    buffer[bytes_read] = '\0';
                    received = 1;
                } else if (bytes_read < 0) {
                    if (errno != EWOULDBLOCK && errno != EAGAIN) {
                        perror("recv");
                        exit(EXIT_FAILURE);
                    }
                } else {
                    // Connection closed by server
                    printf("Server closed connection\n");
                    exit(EXIT_FAILURE);
                }
                
                if (!received) {
                    usleep(100000); // 100ms
                    timeout_counter++;
                }
            }
            
            if (!received) {
                printf("Timeout waiting for server response\n");
                continue;
            }
            
            printf("Received: %s\n", buffer);
            
            // Check if there are tasks available
            if (strcmp(buffer, "No tasks available") == 0) {
                printf("No tasks available. Exiting...\n");
                send(sock, "exit", 4, 0);
                break;
            }
            
            // Check if we have a task
            if (strncmp(buffer, "Task: ", 6) == 0) {
                // Parse and calculate
                char task[BUFFER_SIZE];
                strcpy(task, buffer + 6); // Skip "Task: " prefix
                
                int a, b;
                char op;
                sscanf(task, "%d %c %d", &a, &op, &b);
                
                // Perform calculation
                int result = calculate(a, b, op);
                printf("Calculating %d %c %d = %d\n", a, op, b, result);
                
                // Send result back
                memset(buffer, 0, BUFFER_SIZE);
                sprintf(buffer, "RESULT %d", result);
                send(sock, buffer, strlen(buffer), 0);
                printf("Sent result: %s\n", buffer);
                
                // Wait for acknowledgment
                memset(buffer, 0, BUFFER_SIZE);
                received = 0;
                
                // Try to receive until we get data or timeout
                timeout_counter = 0;
                while (!received && timeout_counter < 50) {
                    ssize_t bytes_read = recv(sock, buffer, BUFFER_SIZE - 1, 0);
                    
                    if (bytes_read > 0) {
                        buffer[bytes_read] = '\0';
                        received = 1;
                    } else if (bytes_read < 0) {
                        if (errno != EWOULDBLOCK && errno != EAGAIN) {
                            perror("recv");
                            exit(EXIT_FAILURE);
                        }
                    } else {
                        // Connection closed by server
                        printf("Server closed connection\n");
                        exit(EXIT_FAILURE);
                    }
                    
                    if (!received) {
                        usleep(100000); // 100ms
                        timeout_counter++;
                    }
                }
                
                if (received) {
                    printf("Server response: %s\n", buffer);
                } else {
                    printf("Timeout waiting for server acknowledgment\n");
                }
            }
        } else if (choice == 2) {
            // Exit
            send(sock, "exit", 4, 0);
            printf("Disconnecting from server...\n");
            break;
        } else {
            printf("Invalid choice. Try again.\n");
        }
    }
    
    close(sock);
    return 0;
}