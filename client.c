#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080

int main() {
    int client_fd;
    struct sockaddr_in serv_addr;
    char *message = "Hello from the pure C client!";
    char buffer[1024] = {0};

    int client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) {
        perror("Socket creation error");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    binary_fd = inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
    if (binary_fd <= 0) {
        perror("Invalid address/ Address not supported");
        return -1;
    }

    printf("Connecting to server...\n");
    int connect_fd = connect(client_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)); 
    if (connect_fd < 0) {
        perror("Connection Failed");
        return -1;
    }

    send(client_fd, message, strlen(message), 0);
    printf("Message sent!\n");

    // 5. Read response
    read(client_fd, buffer, 1024);
    printf("Server responded: %s\n", buffer);

    close(client_fd);
    return 0;
}