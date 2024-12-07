#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/shm.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h> // for boolean values
#include <time.h>

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

// Function declarations
void *receive_msg(void *threadarg);
void *monitor_memory(void *threadarg);
void log_message(const char *filename, const char *message, int type, pid_t client_pid);
void log_startup(const char *filename);
void view_log_file(const char *filename);

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
            printf("\nNew message from client (PID: %d): %s\n", sh_data->client_pid, sh_data->message);
            log_message("chat_log.txt", sh_data->message, sh_data->flag, sh_data->client_pid);
            fflush(stdout);
            // Unlock the mutex to allow the main thread to read input
            pthread_mutex_unlock(&mutex);
            printf("Enter a response for PID %d: ", sh_data->client_pid);
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
                log_message("chat_log.txt", shared_input, 2, sh_data->client_pid);
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

/* Append a message to the log text file. */
void log_message(const char *filename, const char *message, int type, pid_t client_pid){
    FILE *file = fopen(filename, "a");
    if (file){
        if (type == 1) { //
            fprintf(file, "Server received from PID %d: %s\n", client_pid, message);
        }
        else if (type == 2) {
            fprintf(file, "Server sent to PID %d: %s\n", client_pid, message);
        }
        else {
            fprintf(file, "%s\n", message);
        }
        fclose(file);
    } else {
        perror("File open error");
    }
}

/* Create a log file for storing all messages received for the session. */
void log_startup(const char *filename)
{
    FILE *file = fopen(filename, "w"); // write mode, delete and recreate
    if (file)
    {
        // Get current date and time
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char date[26];
        strftime(date, 26, "%Y-%m-%d %H:%M:%S", tm_info);

        // Log the server start message
        fprintf(file, "Server started at %s\n", date);
        fclose(file);
    }
    else
    {
        perror("File open error");
    }
}

// Function to view the content of the log file
void view_log_file(const char *filename) {
    FILE *file = fopen(filename, "r"); // Open the file in read mode
    if (!file) {
        perror("Error opening file");
        return;
    }

    char line[1024]; // Buffer to hold lines from the file
    while (fgets(line, sizeof(line), file)) {
        printf("%s", line); // Print each line
    }
    fclose(file); // Close the file after reading
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
    key_t key = ftok("shmmessage", 65);

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

    // Log server startup
    log_startup("chat_log.txt");

    char input[SHM_SIZE];
    const char exit_command[] = ".exit";
    // Server CLI
    printf("\nNow waiting for messages.");
    printf("\nType '.exit' to exit.\n");
    printf("Type '.view' to view message log\n");

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

        if (strcmp(input, ".view") == 0) {
        printf("Viewing log file:\n");
        view_log_file("chat_log.txt");
        continue;
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
