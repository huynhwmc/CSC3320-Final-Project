#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

/* Chat system server */
int main(int argc, char *argv[])
{
    printf("\nWelcome to the OS Chat Server");
    printf("\nType '.exit' to exit.");
    // Create threads waiting for clients to connect

    pthread_t threads[5];

    char input[20];
    char word[] = ".exit";
    char *result;

    // Using strstr() to find the word
    pthread_create(&threads[0], NULL, receive_msg, NULL);
    pthread_join(threads[0], NULL);

    while (1) // While true
    {
        // Wait for a client to connect and receive a message
        //
        fgets(input, 20, stdin);
        result = strstr(input, word);
        if (result != NULL)
        {
            printf("Exiting...");
            return 0;
        }
    }

    // TODO Dynamic memory test, for messages
    // This pointer will hold the
    // base address of the block created
    int *ptr;
    int n, i;

    // Get the number of elements for the array
    printf("Enter number of elements:");
    scanf("%d", &n);
    printf("Entered number of elements: %d\n", n);

    // Dynamically allocate memory using malloc()
    ptr = (int *)malloc(n * sizeof(int));

    // Check if the memory has been successfully
    // allocated by malloc or not
    if (ptr == NULL)
    {
        printf("Memory not allocated.\n");
        exit(0);
    }
    else
    {
        // Memory has been successfully allocated
        printf("Memory successfully allocated using malloc.\n");

        // Get the elements of the array
        for (i = 0; i < n; ++i)
        {
            ptr[i] = i + 1;
        }

        // Print the elements of the array
        printf("The elements of the array are: ");
        for (i = 0; i < n; ++i)
        {
            printf("%d, ", ptr[i]);
        }
    }

    // free the allocated memory
    free(ptr);
    printf("\nAllocated memory successfully freed");

    // Set 'ptr' NULL to avoid the accidental use
    ptr = NULL;

    return 0;
}

void *receive_msg(void *threadarg) {

    pthread_exit(NULL);
}