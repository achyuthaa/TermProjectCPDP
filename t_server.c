#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define MAX_CLIENTS 10

struct client {
    int sock_fd;
    int remaining_bytes;
};


struct client clients[MAX_CLIENTS];
int num_clients = 0;

// ... rest of the code ...

// Serve clients in a separate thread
void* serve_clients(void* arg) {
    int client_index = (int)arg;
    struct client *client = &clients[client_index];
    char buffer[1024] = {0};
    char *hello = "HTTP/1.1 200 OK\nContent-Type: text/plain\nContent-Length: 12\n\nHello world!";
    int valread;

    while (client->remaining_bytes > 0) {
        valread = read(client->sock_fd, buffer, 1024);
        if (valread <= 0) {
            // Client disconnected or error occurred
            printf("Client disconnected or error occurred, closing socket fd %d\n", client->sock_fd);
            close(client->sock_fd);
            // Remove client from list
            pthread_exit(NULL);
        }
        client->remaining_bytes -= strlen(hello);
        printf("Client request received: %s\n", buffer);
        send(client->sock_fd, hello, strlen(hello), 0);
    }

    // Remove client from list
    close(client->sock_fd);
    for (int i = client_index; i < num_clients-1; i++) {
        clients[i] = clients[i+1];
    }
    num_clients--;

    pthread_exit(NULL);
}



// FIFO scheduling policy
int get_next_client_FIFO() {
    int i, next_client = -1;
    for (i = 0; i < num_clients; i++) {
        if (clients[i].remaining_bytes > 0) {
            next_client = i;
            break;
        }
    }
    if (next_client == -1 && num_clients > 0) {
        // All clients have been served, start over
        next_client = 0;
    }
    return next_client;
}

// SFF scheduling policy
int get_next_client_SFF() {
    int i, next_client = -1, min_remaining_bytes = 0;
    for (i = 0; i < num_clients; i++) {
        if (clients[i].remaining_bytes > 0) {
            if (next_client == -1 || clients[i].remaining_bytes < min_remaining_bytes) {
                next_client = i;
                min_remaining_bytes = clients[i].remaining_bytes;
            }
        }
    }
    if (next_client == -1 && num_clients > 0) {
        // All clients have been served, start over
        next_client = 0;
    }
    return next_client;
}

// Round Robin scheduling policy
int get_next_client_RR() {
    static int current_client = 0;
    int next_client = -1;
    if (num_clients > 0) {
        do {
            current_client = (current_client + 1) % num_clients;
        } while (clients[current_client].remaining_bytes <= 0);
        next_client = current_client;
    }
    return next_client;
}

int main(int argc, char const *argv[]) {
    int server_fd, new_socket, valread, i, j;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};
    char *hello = "HTTP/1.1 200 OK\nContent-Type: text/plain\nContent-Length: 12\n\nHello world!";

    // Create server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set server socket options
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Bind server socket to port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        exit;
 }

printf("Server listening on port %d\n", PORT);

// Main loop to handle incoming connections and serve requests
while (1) {
    // Accept incoming connection
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
    {
        perror("accept failed");
        continue;
    }

    // Add new client to list
    if (num_clients < MAX_CLIENTS) {
        clients[num_clients].sock_fd = new_socket;
        clients[num_clients].remaining_bytes = strlen(hello);
        num_clients++;
    } else {
        printf("Max number of clients reached, rejecting new connection\n");
        close(new_socket);
        continue;
    }

    printf("New client connected, socket fd is %d\n", new_socket);

    // Serve clients in the order determined by the selected scheduling policy
    while (1) {
        int next_client;
        switch (1) {
            case 0:
                next_client = get_next_client_FIFO();
                break;
            case 1:
                next_client = get_next_client_SFF();
                break;
            case 2:
                next_client = get_next_client_RR();
                break;
            default:
                next_client = -1;
        }
        if (next_client == -1) {
            // No more clients to serve
            break;
        }

        // Serve next client
        struct client *client = &clients[next_client];
        valread = read(client->sock_fd, buffer, 1024);
        if (valread <= 0) {
            // Client disconnected or error occurred
            printf("Client disconnected or error occurred, closing socket fd %d\n", client->sock_fd);
            close(client->sock_fd);
            // Remove client from list
            for (i = next_client; i < num_clients-1; i++) {
                clients[i] = clients[i+1];
            }
            num_clients--;
            continue;
        }
        client->remaining_bytes -= strlen(hello);
        printf("Client request received: %s\n", buffer);
        send(client->sock_fd, hello, strlen(hello), 0);
    }
}
}
return 0;
}
