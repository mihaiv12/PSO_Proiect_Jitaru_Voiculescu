#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

pthread_mutex_t console_lock;

void* simulare_primire_agenti(void* arg) {
    while (1) {
        sleep(35);
        pthread_mutex_lock(&console_lock);
        printf("\nagent conectat\n");
        printf(">");
        fflush(stdout);
        pthread_mutex_unlock(&console_lock);
    }
    return NULL;
}

void* simulare_heartbeat_conexiune(void* arg) {
    while (1) {
        sleep(15);
        pthread_mutex_lock(&console_lock);
        printf("\nheartbeat conexiune\n");
        printf(">");
        fflush(stdout);
        pthread_mutex_unlock(&console_lock);
    }
    return NULL;
}