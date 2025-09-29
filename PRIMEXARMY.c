#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <sys/socket.h>

// Configuration for the official NIST Time Protocol (RFC 868)
#define EPOCH_DIFF 2208988800UL       // Time difference between 1900 (Time Protocol) and 1970 (UNIX) epochs

// Structure to pass unique data (like an ID) and shared configuration to each thread
struct thread_data {
    int thread_id;
    char *ip_address;
    int port;
};

// --- Thread Execution Function ---
void *get_time_from_server(void *arg) {
    struct thread_data *data = (struct thread_data *)arg;
    int sock;
    struct sockaddr_in server_addr;
    uint32_t time_value_network;
    time_t current_time;

    // The printf statements can be removed or simplified in a production environment 
    // to reduce output noise. We keep them here for debugging/monitoring.
    printf("Thread %d: Attempting to connect to %s:%d...\n", data->thread_id, data->ip_address, data->port);

    // 1. Create a safe, connection-oriented TCP Socket (SOCK_STREAM)
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Thread Error: Socket creation failed");
        pthread_exit(NULL);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(data->port);

    // Convert IP string to network address structure
    if (inet_pton(AF_INET, data->ip_address, &server_addr.sin_addr) <= 0) {
        perror("Thread Error: Invalid address/ Address not supported");
        close(sock);
        pthread_exit(NULL);
    }

    // 2. Establish connection
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Thread Error: Connection failed");
        close(sock);
        pthread_exit(NULL);
    }

    // 3. Receive the 4-byte time value from the server
    if (read(sock, &time_value_network, sizeof(time_value_network)) < 0) {
        perror("Thread Error: Read failed");
        close(sock);
        pthread_exit(NULL);
    }

    // 4. Convert to host byte order and adjust the time epoch
    current_time = (time_t)(ntohl(time_value_network) - EPOCH_DIFF);

    printf("Thread %d: ✅ Success from %s! Time received: %s", data->thread_id, data->ip_address, ctime(&current_time));

    // 5. Clean up
    close(sock);
    pthread_exit(NULL);
}

// --- Main Function ---
int main(int argc, char *argv[]) {
    // Expected arguments: ./PRIMEXARMY <IP_ADDRESS> <PORT> <NUM_THREADS>
    if (argc != 4) {
        // Output to stderr so Python's subprocess doesn't capture it easily
        fprintf(stderr, "Usage: %s <IP_ADDRESS> <PORT> <NUM_THREADS>\n", argv[0]);
        return 1;
    }

    // Extracting arguments
    char *ip_address = argv[1];
    int port = atoi(argv[2]);
    int num_threads = atoi(argv[3]);

    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Error: Invalid port number: %d\n", port);
        return 1;
    }
    if (num_threads <= 0) {
        fprintf(stderr, "Error: Number of threads must be positive.\n");
        return 1;
    }

    pthread_t *thread_ids = malloc(num_threads * sizeof(pthread_t));
    if (thread_ids == NULL) {
        perror("Failed to allocate thread ID array");
        return 1;
    }

    printf("--- Starting %d network threads to query %s:%d ---\n", num_threads, ip_address, port);

    struct thread_data **all_thread_data = malloc(num_threads * sizeof(struct thread_data *));

    for (int i = 0; i < num_threads; i++) {
        struct thread_data *data = malloc(sizeof(struct thread_data));
        if (data == NULL) {
             perror("Failed to allocate thread data");
             // Cleanup already created threads
             for(int j=0; j<i; j++) pthread_join(thread_ids[j], NULL);
             break;
        }
        
        data->thread_id = i + 1;
        data->ip_address = ip_address;
        data->port = port;
        all_thread_data[i] = data;

        if (pthread_create(&thread_ids[i], NULL, get_time_from_server, (void *)data) != 0) {
            perror("Thread creation failed");
            free(data);
            all_thread_data[i] = NULL;
        }
    }

    // Wait for all threads to finish
    for (int i = 0; i < num_threads; i++) {
        if (all_thread_data[i] != NULL) {
             pthread_join(thread_ids[i], NULL);
             free(all_thread_data[i]);
        }
    }

    free(all_thread_data);
    free(thread_ids);
    printf("--- All operations finished. ---\n");
    return 0;
}