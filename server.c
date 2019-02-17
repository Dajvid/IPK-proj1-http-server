#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    int port, server_socket, rc, comm_socket;
    unsigned int client_len;
    char *endptr;
    struct sockaddr_in sa, sa_client;

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
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("ERROR in socket");
        return EXIT_FAILURE;
    }
    printf("%d\n", port);

    memset((char *)&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    sa.sin_port = htons(port);

    if ((rc = bind(server_socket, (const struct sockaddr *)&sa, sizeof(sa))) < 0) {
        perror("ERROR: bind");
        return(EXIT_FAILURE);
    }

    if (listen(server_socket, 1) < 0) {
        perror("ERROR: listen");
        return(EXIT_FAILURE);
    }

    while (1) {
        comm_socket = accept(server_socket, (struct sockaddr *)&sa_client, &client_len);
        printf("comm_socket - %d\n", comm_socket);
        if (comm_socket > 0) {
            char *hello = "HTTP/1.1 200 OK\nContent-Type: text/plain\nContent-Length: 10\n\nIt's alive";
            char buffer[1024] = {0};
            read(comm_socket , buffer, 1024);
            printf("%s\n", buffer);
            write(comm_socket, hello, strlen(hello));
        }
    }


    return EXIT_SUCCESS;
}