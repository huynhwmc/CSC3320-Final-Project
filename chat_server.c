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
char shared_input[SHM_SIZE];  // Buffer for user input
bool server_running = true;
bool has_new_message = false; // Indicates new message in shared memory
bool waiting_for_response = false; // Indicates the server is waiting for a response

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
    while (server_running)
    {
        pthread_mutex_lock(&mutex);

        // Wait until there is a new message
        while (server_running && !has_new_message)
        {
            pthread_cond_wait(&full, &mutex);
        }
        if (!server_running)
        {
            pthread_mutex_unlock(&mutex);
            break;
        }
        shared_data_t *sh_data = (shared_data_t *)shared_memory; // Read from shared memory

        // Check if the message flag indicates it's a client message
        if (sh_data->flag == 1)
        {
            printf("New message from client (PID: %d): %s\n", sh_data->client_pid, sh_data->message);
            fflush(stdout);
            // Unlock the mutex to allow the main thread to read input
            pthread_mutex_unlock(&mutex);
            printf("\nEnter a response for PID %d: ", sh_data->client_pid);
            fflush(stdout);
            waiting_for_response = true;
            // Wait for a response or timeout
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += RESPONSE_TIMEOUT;

            pthread_mutex_lock(&mutex);

            int res = pthread_cond_timedwait(&empty, &mutex, &ts);

            if (res == 0 && strlen(shared_input) > 0) {
                // Prepare server response
                snprintf(sh_data->message, SHM_SIZE - 12, "%s", shared_input);
                sh_data->flag = 2; // Mark as server response
                printf("Sent response: %s\n", shared_input);
                memset(shared_input, 0, sizeof(shared_input));
            }
            else {
                printf("No response sent.\n");
            }

            pthread_cond_signal(&full); // Notify the client that a response is ready

            has_new_message = false;
            waiting_for_response = false;
            usleep(100000);
            sh_data->flag = 0; // Mark shared memory as empty
            memset(sh_data->message, 0, SHM_SIZE - 12);
            
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
    while (server_running)
    {
        pthread_mutex_lock(&mutex);

        shared_data_t *sh_data = (shared_data_t *)shared_memory;
        // Check if shared memory has new messages
        if (server_running && sh_data->flag == 1 && !has_new_message)
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

    char input[SHM_SIZE];
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
            pthread_mutex_lock(&mutex);
            server_running = false;
            pthread_cond_broadcast(&full); // Wake up all threads
            pthread_cond_broadcast(&empty);
            pthread_mutex_unlock(&mutex);
            break; // Terminate from the infinite loop and execute cleanup
        }

        pthread_mutex_lock(&mutex);

        if (waiting_for_response)
        {
            // Copy input to shared buffer for the receive_msg thread
            strncpy(shared_input, input, sizeof(shared_input) - 1);
            pthread_cond_signal(&empty); // Notify the receive_msg thread
        }

        pthread_mutex_unlock(&mutex);
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
