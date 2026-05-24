#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 8080
#define BUFFER_SIZE 1024

// Shared buffer to store inputs
int fd_buffer[BUFFER_SIZE];
int reader_pos = 0; // Thread to read inputs 
int parser_pos = 0; // Thread to parse inputs from buffer
int count = 0; // Tracks number of pending sockets
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cv_reader = PTHREAD_COND_INITIALIZER;
pthread_cond_t cv_parser = PTHREAD_COND_INITIALIZER;

/*
    Function to check for vulnerabities in prompts
    Returns 1 if a vulnerability is detected, 0 if clean 
*/
int scan_payload(const char *payload) {
    // High speed substring search. 
    // In production, this would be Aho-Corasick tree.
    const char *blacklisted_signatures[] = {
        "sk-proj-",       // Standard OpenAI API Key prefix
        "CONFIDENTIAL",   // Internal document leak
        "password=",      // Hardcoded credentials
        "SSN:",           // PII leak
        NULL
    };

    // Compare text in prompt to vulnerable patterns
    for (int i = 0; blacklisted_signatures[i] != NULL; i++) {
        if (strstr(payload, blacklisted_signatures[i]) != NULL) {
            printf("\n[BLOCKED] Triggered on signature: %s\n", blacklisted_signatures[i]);
            return 1; // Dirty, return 1
        }
    }
    return 0; // Clean
}

// Parser thread to parse values from buffer
void *parser(void *arg) {
    char http_buffer[4096];

    while (1) {
        // Lock and Wait for a pending socket
        pthread_mutex_lock(&mtx);
        while (count == 0) {
            pthread_cond_wait(&cv_parser, &mtx);
        }

        // Pop the client socket FD from the queue
        int client_fd = fd_buffer[parser_pos];
        parser_pos = (parser_pos + 1) % BUFFER_SIZE;
        count--;

        // Signal reader thread that buffer space freed up
        pthread_cond_signal(&cv_reader);
        pthread_mutex_unlock(&mtx);

        memset(http_buffer, 0, sizeof(http_buffer)); // Process the Network Connection
        int valread = read(client_fd, http_buffer, sizeof(http_buffer) - 1);

        if (valread > 0) {
            // Find where HTTP Headers end and the JSON Body begins
            char *body = strstr(http_buffer, "\r\n\r\n");
            
            if (body != NULL) {
                body += 4; // Move pointer past the \r\n\r\n line breaks
                
                int is_dirty = scan_payload(body);

                if (is_dirty) {
                    const char *block_res = "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                    send(client_fd, block_res, strlen(block_res), 0);
                } else {
                    const char *ok_res = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                    send(client_fd, ok_res, strlen(ok_res), 0);
                }
            }
        }

        close(client_fd);
    }
    return NULL;
}

// Reader thread to read values from connection
void *reader(void *arg) {
    int server_fd = *(int *)arg;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    while (1) {
        // Block and wait for a user to send an AI request
        // accept() is outside mutex, don't lock the queue while waiting
        int client_fd = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (client_fd < 0) {
            perror("Accept failed");
            continue;
        }

        // Lock the queue
        pthread_mutex_lock(&mtx);
        while (count == BUFFER_SIZE) {
            pthread_cond_wait(&cv_reader, &mtx);
        }

        // Push the new client socket FD into the queue
        fd_buffer[reader_pos] = client_fd;
        reader_pos = (reader_pos + 1) % BUFFER_SIZE;
        count++;

        // Wake up one of the sleeping parser threads
        pthread_cond_signal(&cv_parser);
        pthread_mutex_unlock(&mtx);
    }
    return NULL;
}

int main() {
    int server_fd;
    struct sockaddr_in address;

    // Socket Initialization
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    // Allows instant restart of the server without standard errors
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 8192);
    printf("Airlock Proxy listening for AI traffic on port %d...\n", PORT);

    pthread_t read_t, parse_t1, parse_t2; // Initialize threads

    // Pass the server socket to the single reader thread
    pthread_create(&read_t, NULL, reader, &server_fd);

    // Spawn multiple worker threads to handle concurrent AI requests
    pthread_create(&parse_t1, NULL, parser, NULL);
    pthread_create(&parse_t2, NULL, parser, NULL);

    pthread_join(read_t, NULL);
    pthread_join(parse_t1, NULL);
    pthread_join(parse_t2, NULL);

    return 0;
}