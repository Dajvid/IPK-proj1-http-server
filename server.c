#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>

int main(int argc, char **argv)
{
    int port, sockfd;
    char *endptr;

    /* argument parsing */
    if (argc < 2) {
        fprintf(stderr, "No port number specified.\n");
        return EXIT_FAILURE;
    }

    port = strtol(argv[1], &endptr, 10);

    if (*endptr != '\0') {
        fprintf(stderr, "Port number must be specified as integer %s id not an integer\n", argv[1]);
        return EXIT_FAILURE;
    }

    /* socket creation */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd <= 0) {
        perror("ERROR in socket");
        return EXIT_FAILURE;
    }



    return EXIT_SUCCESS;
}