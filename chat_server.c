#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/shm.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h> // for boolean values

#define SHM_SIZE 1024
#define RESPONSE_TIMEOUT 20 // timeout in seconds for server

pthread_mutex_t mutex;
pthread_cond_t full;
pthread_cond_t empty;
int shmid; // Shared memory's ID
void *shared_memory;
bool has_new_message = false; // Indicates new message in shared memory

/* Add custom data to shared memory. */
typedef struct
{
    int flag;                    // 0 = empty, 1 = client message, 2 = server response
    pid_t client_pid;            // Client's process ID
    char message[SHM_SIZE - 12]; // Message content
} shared_data_t;

/* Access shared memory and read a message */
void *receive_msg(void *threadarg)
{
    char server_response[SHM_SIZE];
    while (true)
    {
        pthread_mutex_lock(&mutex);

        // Wait until there is a new message
        while (!has_new_message)
        {
            pthread_cond_wait(&full, &mutex);
        }
        shared_data_t *sh_data = (shared_data_t *)shared_memory; // Read from shared memory

        // Check if the message flag indicates it's a client message
        if (sh_data->flag == 1)
        {
            printf("New message from client (PID: %d): %s\n", sh_data->client_pid, sh_data->message);
            printf("Reply to client? (y/n): ");
            char choice;
            fgets(server_response, sizeof(server_response), stdin);
            choice = server_response[0];

            if (choice == 'y' || choice == 'Y')
            {
                // Prepare server response
                snprintf(server_response, SHM_SIZE, "Server Response: Hello, process %d", sh_data->client_pid);
                // Write server response to shared memory
                strncpy(sh_data->message, server_response, SHM_SIZE - 12); // Store response
                printf("Sent response: %s\n", server_response);
                pthread_cond_signal(&empty); // Notify the client that the server has responded
            }
            else
            {
                printf("No response sent.\n");
            }
            // Clear the shared memory
            memset(sh_data->message, 0, SHM_SIZE - 12);
            sh_data->flag = 0; // Mark shared memory as empty

            // Signal that the shared memory is now empty
            pthread_cond_signal(&empty);
        }
        pthread_mutex_unlock(&mutex);
    }
    pthread_exit(NULL);
}

/* Monitor for new messages in shared memory */
void *monitor_memory(void *threadarg)
{
    while (true)
    {
        pthread_mutex_lock(&mutex);

        shared_data_t *sh_data = (shared_data_t *)shared_memory;

        // Check if shared memory has new messages
        if (sh_data->flag == 1 && !has_new_message)
        {
            has_new_message = true;
            pthread_cond_signal(&full); // Notify the receive_msg thread
        }

        pthread_mutex_unlock(&mutex);

        usleep(1000); // Small delay to reduce CPU usage
    }

    pthread_exit(NULL);
}

/* Chat system server */
int main(int argc, char *argv[])
{
    printf("\nWelcome to the OS Chat Server");

    // Create the mutex
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&full, NULL);
    pthread_cond_init(&empty, NULL);

    // Generate unique key
    key_t key = ftok("shmfile", 65);

    // Create shared memory
    shmid = shmget(key, 1024, 0666 | IPC_CREAT);
    if (shmid == -1)
    {
        perror("Shared memory error: shmget");
        return 1;
    }

    shared_memory = shmat(shmid, NULL, 0);
    if (shared_memory == (void *)-1)
    {
        perror("Shared memory error: shmat");
        return 1;
    }

    // Clear shared memory initially
    memset(shared_memory, 0, SHM_SIZE);

    // Create threads waiting for clients to connect
    pthread_t server_thread, monitor_thread;
    pthread_create(&server_thread, NULL, receive_msg, NULL);
    pthread_create(&monitor_thread, NULL, monitor_memory, NULL);

    char input[20];
    const char exit_command[] = ".exit";
    // Server CLI
    printf("\nNow waiting for messages.");
    printf("\nType '.exit' to exit.\n");

    while (true) // Waiting for the "exit" message
    {
        fgets(input, sizeof(input), stdin);
        // Remove the newline character if present from fgets
        input[strcspn(input, "\n")] = '\0';

        // Compare input with ".exit"
        if (strcmp(input, exit_command) == 0)
        {
            printf("\nExiting...\n");
            break; // Terminate from the infinite loop and execute the code below
        }
    }

    // Clean up threads, shared memory, and mutex
    pthread_cancel(server_thread);
    pthread_cancel(monitor_thread);
    pthread_join(server_thread, NULL);
    pthread_join(monitor_thread, NULL);
    shmctl(shmid, IPC_RMID, NULL);
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&full);
    pthread_cond_destroy(&empty);
    return 0;
}
