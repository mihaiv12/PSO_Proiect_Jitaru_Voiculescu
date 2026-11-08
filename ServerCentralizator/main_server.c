#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

#include "simulare.h"

#define MAX_AGENTS 3
#define BUFFER_SIZE 256

typedef struct {
    int id;
    int active;
    char name[32];
} Agent;

Agent agents[MAX_AGENTS] = {
    {1, 1, "Agent1"},
    {2, 1, "Agent2"},
    {3, 1, "Agent3"}
};

void interpretare_comanda(int target_id, const char *cmd) {
    if (target_id == 0) {
        for (int i = 0; i < MAX_AGENTS; i++) {
            if (agents[i].active) {
                printf("se trimite '%s' catre %s\n", cmd, agents[i].name);
            }
        }
    } else {
        for (int i = 0; i < MAX_AGENTS; i++) {
            if (agents[i].id == target_id && agents[i].active) {
                printf("se trimite '%s' catre %s\n", cmd, agents[i].name);
                break;
            }
        }
    }
}

int main() {
    pthread_t t_asincron, t_heartbeat;

    pthread_mutex_init(&console_lock, NULL);

    printf("server centralizator\n");
    printf("> ");
    fflush(stdout);

    pthread_create(&t_asincron, NULL, simulare_primire_agenti, NULL);
    pthread_create(&t_heartbeat, NULL, simulare_heartbeat_conexiune, NULL);

    char comanda[BUFFER_SIZE];
    int target_id;

    while (1) {
        if (fgets(comanda, BUFFER_SIZE, stdin) == NULL) break;
        comanda[strlen(comanda) - 1] = 0;

        if (strcmp(comanda, "exit") == 0) {
            printf("exiting server...\n");
            break;
        }
        if (strncmp(comanda, "Agent", 5) == 0 || strcmp(comanda, "Broadcast") == 0) {
            if (strcmp(comanda, "Broadcast") == 0) {
                target_id = 0;
            } else {
                target_id = comanda[5] - '0';
            }
            
            printf("> ");
            fflush(stdout);

            if (fgets(comanda, BUFFER_SIZE, stdin) == NULL) break;
            comanda[strcspn(comanda, "\n")] = 0;

            if (strcmp(comanda, "exit") == 0) {
                printf("exiting server...\n");
                break;
            }

            interpretare_comanda(target_id, comanda);
        }

        printf("> ");
        fflush(stdout);
    }

    pthread_cancel(t_asincron);
    pthread_cancel(t_heartbeat);
    pthread_mutex_destroy(&console_lock);

    return 0;
}