#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/shm.h>
#include <string.h>
#include <sys/wait.h>

pthread_mutex_t mutex;
pthread_cond_t full;
pthread_cond_t empty;
char message[200];
int shmid; // Shared memory's ID
void *shared_memory;
char buff[100];

void *receive_msg(void *threadarg) {
    // TODO: Access shared memory and read the message
    
    pthread_exit(NULL);
}

/* Chat system server */
int main(int argc, char *argv[])
{
    printf("\nWelcome to the OS Chat Server");

    // Create threads waiting for clients to connect
    pthread_t serverThread;
    // Create the mutex
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&full, NULL);
    pthread_cond_init(&empty, NULL);
    
    char input[20];
    char word[] = ".exit";
    char *result;

    // Start receiving messages from clients
    // Create shared memory
        shmid = shmget((key_t)2345, 1024, 0666 | IPC_CREAT);
        if (shmid == -1)
        {
            perror("shmget");
            return 1;
        }
        
        shared_memory = shmat(shmid, NULL, 0);
        if (shared_memory == (void *)-1)
        {
            perror("shmat");
            return 1;
        }
    pthread_create(&serverThread, NULL, receive_msg, NULL);
    pthread_join(serverThread, NULL);

    // Server CLI
    printf("\nNow waiting for messages.");
    printf("\nType '.exit' to exit.\n");

    while (1) // While true, waiting for the "exit" message
    {
        fgets(input, sizeof(input), stdin);
        // Remove the newline character if present from fgets
        input[strcspn(input, "\n")] = '\0';

        // Compare input with ".exit"
        if (strcmp(input, word) == 0)
        {
            printf("\nExiting...\n");
            break; // Terminate from the infinite loop and execute the code below
        }
    }

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&full);
    pthread_cond_destroy(&empty);
    return 0;
}
