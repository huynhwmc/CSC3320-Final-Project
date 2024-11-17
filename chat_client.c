#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <string.h>
#include <sys/wait.h>

/* Chat client */
int main(int argc, char *argv[])
{
    char **strings = NULL; // Holds the message in dynamic memory
    int i = 0;
    char str[200], ch;

    // User enters a message
    printf("\nEnter a message: ");

    printf("\n Enter Your message (press @ to terminate) \n");
    while ((ch = fgetc(stdin)) != '@')
    {
        str[i++] = ch;
    }
    str[i] = '\0';

    int shmid;
    void *shared_memory;
    char buff[100];

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

    // Allocate dynamic memory for the message

    // Connect to the chat server using shared memory

    // Use mutexes to ensure no other process is accessing or updating that memory

    // After the message has been successfully sent, free the allocated memory
    return 0;
}