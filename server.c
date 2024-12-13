#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include "common.h"

#define PORT 8080
#define MAX_CLIENTS 100

pthread_mutex_t mutex;

struct client {
    char name[256];
    char lastMessage[1024];
    int isActive;
    int socket;
    pthread_t thread;
};

int clients_size;
struct client clients[MAX_CLIENTS];

void removeClient(struct client* _client) {
    _client->isActive = 0;
    for (int i = 0; i < clients_size; i++) {
        if (&clients[i] == _client) {
            for (int j = i; j < clients_size - 1; j++) {
                clients[j] = clients[j + 1];
            }
            clients_size--;
            break;
        }
    }
}

char* clientListString() {
    static char retVal[1024];
    strcpy(retVal, "\nUser List:\n");
    for (int i = 0; i < clients_size; i++) {
        if (clients[i].isActive) {
            strcat(retVal, clients[i].name);
            strcat(retVal, "\n");
        }
    }
    return retVal;
}

struct client* getClient(char* name) {
    for (int i = 0; i < clients_size; i++) {
        if (strcmp(clients[i].name, name) == 0 && clients[i].isActive) {
            return &clients[i];
        }
    }
    return NULL;
}

int getRandomBoolean() {
    srand(time(0));
    return rand() % 2 == 0;
}

void broadcast_message(const char* message, int sender_socket) {
    pthread_mutex_lock(&mutex);
    struct _data* msg_data = parse_data((char*)message);
    
    if (msg_data->type == MESG) {
        char recipient[256], content[768];
        // Extract recipient from the message
        if (sscanf(msg_data->data, "%[^|]|%[^\n]", recipient, content) == 2) {
            // Find recipient and send only to them
            for (int i = 0; i < clients_size; i++) {
                if (clients[i].isActive && strcmp(clients[i].name, recipient) == 0) {
                    send(clients[i].socket, message, strlen(message), 0);
                    printf("Message forwarded to %s\n", recipient);
                    break;
                }
            }
        }
    }
    
    free(msg_data);
    pthread_mutex_unlock(&mutex);
}

void* ServerClient(void* threadVal) {
    struct client* _client = (struct client*)threadVal;
    char buffer[1024] = {0};
    int valread;

    valread = read(_client->socket, buffer, 1024);

    struct _data* currdata = parse_data(buffer);

    if (currdata->type == CONN && !_client->isActive) {
        char* client_list = clientListString();
        char* msg = createMESGMessage("Server", client_list);
        _client->isActive = 1;
        strcpy(_client->name, currdata->data);
        printf("%s connected to server.\n", _client->name);
        send(_client->socket, msg, strlen(msg), 0);

        char notification[1024];
        sprintf(notification, "%s joined the group.", _client->name);
        char* notification_msg = createMESGMessage("Server", notification);

        for (int i = 0; i < clients_size; i++) {
            if (clients[i].socket != _client->socket && clients[i].isActive) {
                send(clients[i].socket, notification_msg, strlen(notification_msg), 0);
            }
        }
    }

    while ((valread = read(_client->socket, buffer, 1024)) > 0) {
        pthread_mutex_lock(&mutex);

        struct _data* currdata = parse_data(buffer);

        if (currdata->type == MESG) {
            struct _message* currmessage = parse_message(currdata->data);

            struct client* reciever = getClient(currmessage->reciever_name);
            if (reciever != NULL) {
                char* msg_to_send = createMESGMessage(_client->name, currmessage->message);
                strcpy(reciever->lastMessage, msg_to_send);

                if (getRandomBoolean()) {
                    msg_to_send = corruptMessage(msg_to_send);
                }

                send(reciever->socket, msg_to_send, strlen(msg_to_send), 0);
                printf("%s mesaj yolladı: %s.\n", _client->name, currmessage->reciever_name);
            } else {
                printf("%s aktif değil.\n", currmessage->reciever_name);
            }
        } else if (currdata->type == MERR) {
            send(_client->socket, _client->lastMessage, strlen(_client->lastMessage), 0);
        } else if (currdata->type == GONE) {
            printf("%s serverla bağlantısı kesildi.\n", _client->name);
            close(_client->socket);
            _client->isActive = 0;
            removeClient(_client);

            char notification[1024];
            sprintf(notification, "%s gruptan ayrıldı.", _client->name);

            for (int i = 0; i < clients_size; i++) {
                if (clients[i].socket != _client->socket && clients[i].isActive) {
                    send(clients[i].socket, notification, strlen(notification), 0);
                }
            }

            break;
        }

        memset(buffer, 0, 1024);
        pthread_mutex_unlock(&mutex);
    }
    return 0;
}

int main(int argc, char const* argv[]) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    clients_size = 0;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server started\n");

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        pthread_mutex_lock(&mutex);

        struct client tmpClient;
        clients[clients_size++] = tmpClient;

        clients[clients_size - 1].socket = new_socket;

        pthread_create(&clients[clients_size - 1].thread, NULL, ServerClient, &clients[clients_size - 1]);
        pthread_mutex_unlock(&mutex);
    }

    return 0;
}