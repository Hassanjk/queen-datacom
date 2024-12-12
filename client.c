#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include "common.h"

#define PORT 8080

pthread_mutex_t mutex;
FILE* logfile;

void log_message(const char* message) {
    time_t now;
    time(&now);
    char* date = ctime(&now);
    date[strlen(date) - 1] = '\0';  // Remove newline

    pthread_mutex_lock(&mutex);
    fprintf(logfile, "[%s] %s\n", date, message);
    fflush(logfile);
    pthread_mutex_unlock(&mutex);
}

void* receive_messages(void* socket_ptr) {
    int sock = *(int*)socket_ptr;
    char buffer[1024] = {0};
    int valread;

    while ((valread = read(sock, buffer, 1024)) > 0) {
        struct _data* received_data = parse_data(buffer);
        if (received_data->type == MESG) {
            // Verify message integrity
            uint8_t parity = simple_parity_check(received_data->data);
            uint8_t crc = calculateCRC((const uint8_t*)received_data->data, strlen(received_data->data));
            
            if (parity != received_data->parity || crc != received_data->CRC) {
                // Message corrupted, request resend
                char* merr = createMERRMessage();
                send(sock, merr, strlen(merr), 0);
                free(merr);
                log_message("Corrupted message detected, requesting resend");
            } else {
                printf("Received: %s\n", received_data->data);
                log_message("Message received successfully");
            }
        }
        memset(buffer, 0, 1024);
        free(received_data);
    }
    return NULL;
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};
    
    // Initialize mutex and open log file
    pthread_mutex_init(&mutex, NULL);
    logfile = fopen("client_log.txt", "a");
    if (logfile == NULL) {
        printf("Failed to open log file\n");
        return -1;
    }

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        log_message("Socket creation error");
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    // Convert IPv4 address from text to binary
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        log_message("Invalid address/Address not supported");
        return -1;
    }

    // Connect to server
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        log_message("Connection Failed");
        return -1;
    }

    log_message("Connected to server");

    // Get username
    char username[256];
    printf("Enter your username: ");
    fgets(username, 256, stdin);
    username[strcspn(username, "\n")] = 0;

    // Send connection message
    char* conn_msg = createCONNMessage(username);
    send(sock, conn_msg, strlen(conn_msg), 0);
    free(conn_msg);
    log_message("Connection message sent");

    // Create receive thread
    pthread_t receive_thread;
    pthread_create(&receive_thread, NULL, receive_messages, &sock);

    // Main communication loop
    while (1) {
        printf("Enter recipient and message (format: recipient|message) or 'quit': ");
        fgets(buffer, 1024, stdin);
        buffer[strcspn(buffer, "\n")] = 0;  // Remove newline

        if (strcmp(buffer, "quit") == 0) {
            char* gone_msg = createGONEMessage();
            send(sock, gone_msg, strlen(gone_msg), 0);
            free(gone_msg);
            break;
        }

        char* msg = createMESGMessage(username, buffer);
        send(sock, msg, strlen(msg), 0);
        free(msg);
        log_message("Message sent");

        memset(buffer, 0, sizeof(buffer));
    }

    // Cleanup
    close(sock);
    fclose(logfile);
    pthread_mutex_destroy(&mutex);
    log_message("Connection closed");

    return 0;
}