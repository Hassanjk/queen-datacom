#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "common.h"

#define CRC_POLYNOMIAL 0xEDB88320

char* createMESGMessage(const char* sender, const char* message) {
    char* retval = (char*) malloc(2048); // Increase buffer size
    char recipient[256] = {0};
    char content[768] = {0};
    
    // Parse the message to extract recipient and content
    if (sscanf(message, "%255[^|]|%767[^\n]", recipient, content) == 2) {
        // Format: MESG|recipient|sender|content
        int written = snprintf(retval, 2047, "MESG|%s|%s|%s", 
                             recipient,
                             sender ? sender : "",
                             content);
        
        if (written >= 0 && written < 2047) {
            uint8_t parity = simple_parity_check(retval + 5);
            uint8_t crcVal = calculateCRC((const uint8_t*)(retval + 5), strlen(retval + 5));

            retval[written] = parity;
            retval[written + 1] = crcVal;
            retval[written + 2] = '\0';
        } else {
            // Message too long, truncate or handle error
            strcpy(retval, "MERR");
        }
    } else {
        strcpy(retval, "MERR");
    }

    return retval;
}

char* createCONNMessage(const char* name){
    char* retval = (char*) malloc(1024);
    snprintf(retval, 1024, "CONN|%s", name);
    return retval;
}

char* createMERRMessage(){
    char* retval = (char*) malloc(1024);
    snprintf(retval, 1024, "MERR");
    return retval;
}

char* createGONEMessage(){
    char* retval = (char*) malloc(1024);
    snprintf(retval, 1024, "GONE");
    return retval;
}

char* corruptMessage(char* message) {
    srand(time(NULL));
    int randomIndex = rand() % strlen(message);
    message[randomIndex] = (message[randomIndex] == '0') ? '1' : '0';
    return message;
}

struct _data* parse_data(char* buffer){
    struct _data* retval = (struct _data*)malloc(sizeof(struct _data));
    char* token = strtok(buffer, "|");
    if(strcmp(token, "CONN") == 0){
        retval->type = CONN;
        token = strtok(NULL, "");
        strcpy(retval->data, token);
    }
    else if(strcmp(token, "MESG") == 0){
        retval->type = MESG;
        token = strtok(NULL, "");
        strcpy(retval->data, token);
        retval->CRC = retval->data[strlen(retval->data) - 1];
        retval->data[strlen(retval->data) - 1] = '\0';
        retval->parity = retval->data[strlen(retval->data) - 1];
        retval->data[strlen(retval->data) - 1] = '\0';
    }
    else if(strcmp(token, "MERR") == 0){
        retval->type = MERR;
    }
    else if(strcmp(token, "GONE") == 0){
        retval->type = GONE;
    }
    return retval;
}

struct _message* parse_message(char* msg) {
    struct _message* retval = (struct _message*) malloc(sizeof(struct _message));
    char* token = strtok(msg, "|");
    if (token != NULL) {
        strcpy(retval->reciever_name, token);
        token = strtok(NULL, "");  // Get rest of message
        if (token != NULL) {
            strcpy(retval->message, token);
        } else {
            strcpy(retval->message, "");
        }
    }
    return retval;
}

uint8_t simple_parity_check(const char* data){
    int parity = 0;
    for (int i = 0; i < strlen(data); i++) {
        parity += (data[i] & 1);
    }
    return (parity % 2 == 0);
}

uint8_t calculateCRC(const uint8_t *data, size_t size) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? CRC_POLYNOMIAL : 0);
        }
    }
    return crc ^ 0xFFFFFFFF;
}