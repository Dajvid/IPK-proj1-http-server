#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdbool.h>

#include "server.h"
#include "dyn_buffer.h"

int pipe_fd[2];

char *
get_header_field_content(char *header, char *field, int *length)
{
    char *iter, *found;
    *length = 0;
    buffer temp;

    buf_init(&temp, 16);
    buf_append(&temp, '\n');
    buf_concat(&temp, field, strlen(field));
    found = strstr(header, temp.data);

    if (found) {
        found = &found[strlen(field) + 3];
        iter = found;
        while (*iter != '\n') {
            (*length)++;
            iter++;
        }
    }

    buf_destroy(&temp);
    return found;
}

RESPONSE_TYPE
get_response_type(char *header)
{
    int field_len = 0;
    char *field = NULL;

    field = get_header_field_content(header, "Content-Type", &field_len);

    if (field_len == 0) {
        return TYPE_TEXT_PLAIN;
    }
    if (strncmp("application/json", field, field_len) == 0) {
        return TYPE_APPLICATION_JSON;
    } else {
        return TYPE_TEXT_PLAIN;
    }
}

SERVER_ERR
parse_request_line(char **header_ptr, PATH *requested_path)
{
    buffer temp;

    buf_init(&temp, 16);

    if (strncmp(*header_ptr, "GET ", 4) != 0) {
        return ERR_BAD_METHOD;
    }

    *header_ptr = &((*header_ptr)[4]);
    if (strncmp(*header_ptr, "/hostname ", 10) == 0) {
        *requested_path = REQ_HOSTNAME;
        *header_ptr = &((*header_ptr)[10]);
    } else if (strncmp(*header_ptr, "/cpu-name ", 10) == 0) {
        *requested_path = REQ_CPU_NAME;
        *header_ptr = &((*header_ptr)[10]);
    } else if (strncmp(*header_ptr, "/load ", 6) == 0) {
        *requested_path = REQ_LOAD;
        *header_ptr = &((*header_ptr)[6]);
    } else {
        *requested_path = REQ_UNKNOWN;
        return ERR_BAD_PATH;
    }

    if (strncmp(*header_ptr, "HTTP/1.1\r\n", 10) == 0 || strncmp(*header_ptr, "HTTP/1.0\r\n", 10) == 0) {
        *header_ptr = &((*header_ptr)[10]);
        return SUCCESS;
    } else {
        return ERR_BAD_VERSION;
    }
}

SERVER_ERR
load_result(int fd, char **out)
{
    char c;
    buffer res_buf;

    IF_RET(buf_init(&res_buf, 64) != BUF_SUCCESS, ERR_MEM);
    read(fd, &c, 1);
    while (c != '\n') {
        buf_append(&res_buf, c);
        read(fd, &c, 1);
    }

    *out = res_buf.data;
    return SUCCESS;
}

SERVER_ERR
get_cpu_name(char **res, int fd, RESPONSE_TYPE res_type)
{
    SERVER_ERR ret = SUCCESS;
    char *cpu_name;
    buffer json_buf;

    ret = sys_com_to_stdin("grep -m 1 \"model name\" /proc/cpuinfo | cut -c 14-");
    IF_RET(ret != SUCCESS, ret);
    load_result(fd, &cpu_name);

    if (res_type == TYPE_APPLICATION_JSON) {
        buf_init(&json_buf, 32);
        buf_printf(&json_buf, "{\"cpu-name\":\"%s\"}", cpu_name);
        *res = json_buf.data;
        free(cpu_name);
    } else {
        *res = cpu_name;
    }

    return ret;
}

SERVER_ERR
load_header(int fd, char **out)
{
    char *end_ptr, *content, loaded, last_loaded = 0;
    int content_lenght = 0;
    SERVER_ERR ret = SUCCESS;
    buffer header_buf, tmp_buf;

    buf_init(&header_buf, 128);
    read(fd, &loaded, 1);
    buf_append(&header_buf, loaded);

    while (loaded != '\r' || last_loaded != '\n') {
        if (loaded == 0) {
            buf_destroy(&header_buf);
            return ERR_BAD_REQ;
        }

        last_loaded = loaded;
        read(fd, &loaded, 1);
        buf_append(&header_buf, loaded);
    }

    read(fd, &loaded, 1);
    content = get_header_field_content(header_buf.data, "Content-Length", &content_lenght);
    if (content_lenght != 0) {
        buf_init(&tmp_buf, 16);
        buf_concat(&tmp_buf, content, content_lenght);
        content_lenght = strtol(tmp_buf.data, &end_ptr, 10);
        buf_destroy(&tmp_buf);
        if (end_ptr[0] != '\0') {
            buf_destroy(&header_buf);
            return ERR_BAD_REQ;
        }
        lseek(fd, content_lenght, SEEK_CUR);
    }

    *out = header_buf.data;
    return SUCCESS;
}

// char *
// get_hostname()
// {

// }

SERVER_ERR
sys_com_to_stdin(char *command)
{
    pid_t pid = getpid();

    buffer command_buffer;
    IF_RET(buf_init(&command_buffer, 64) != BUF_SUCCESS, ERR_MEM);
    IF_RET(buf_concat(&command_buffer , command, strlen(command)) != BUF_SUCCESS, ERR_MEM);
    IF_RET(buf_concat(&command_buffer, ">/proc/", 0) != BUF_SUCCESS, ERR_MEM);
    IF_RET(buf_printf(&command_buffer, "%d", pid) != BUF_SUCCESS, ERR_MEM);
    IF_RET(buf_concat(&command_buffer, "/fd/0\n", 0) != BUF_SUCCESS, ERR_MEM);
    system(command_buffer.data);
    return SUCCESS;
}

int main(int argc, char **argv)
{
    int port, server_socket, rc, comm_socket;
    unsigned int client_len;
    char *endptr, *cpu_name, *header;
    buffer response_header, response_payload, response_header_fields;
    struct sockaddr_in sa, sa_client;
    PATH path;
    SERVER_ERR ret = SUCCESS;
    RESPONSE_TYPE response_type = TYPE_TEXT_PLAIN;

    pipe(pipe_fd);
    dup2(pipe_fd[1], 0);

    /* argument parsing */
    if (argc < 2) {
        fprintf(stderr, "No port number specified.\n");
        return EXIT_FAILURE;
    }
    port = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0') {
        fprintf(stderr, "Port number must be specified as integer %s is not an integer\n", argv[1]);
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

    buf_init(&response_header, 64);
    buf_init(&response_payload, 64);
    buf_init(&response_header_fields, 64);
    /* busy waiting for client to connect */
    while (1) {
        comm_socket = accept(server_socket, (struct sockaddr *)&sa_client, &client_len);
        if (comm_socket > 0) {
            buf_flush(&response_header);
            buf_flush(&response_header_fields);
            buf_flush(&response_payload);
            // char *hello = "HTTP/1.1 200 OK\nContent-Type: text/plain\nContent-Length: 10\n\nIt's alive";

            ret = load_header(comm_socket, &header);
            ret = parse_request_line(&header, &path);
            if (ret == SUCCESS) {
                response_type = get_response_type(header);
                buf_concat(&response_header, OK_HEADER, strlen(OK_HEADER));

                if (path == REQ_HOSTNAME) {

                } else if (path == REQ_CPU_NAME) {
                    get_cpu_name(&cpu_name, pipe_fd[0], response_type);
                    buf_concat(&response_payload, cpu_name, 0);
                    buf_printf(&response_header_fields, "Content-Type: %s\nContent-Length: %d\n\n", RESPONSE_TYPE_STRING[response_type], buf_get_len(&response_payload));
                } else if (path == REQ_LOAD) {

                }

            } else if (ret == ERR_BAD_METHOD) {
                buf_concat(&response_header, BAD_REQUEST_HEADER, strlen(BAD_REQUEST_HEADER));
            } else if (ret == ERR_BAD_PATH) {
                buf_concat(&response_header, NOT_FOUND_HEADER, strlen(NOT_FOUND_HEADER));
            }
            printf("%s\n", header);
            write(comm_socket, response_header.data, buf_get_len(&response_header));
            write(comm_socket, response_header_fields.data, buf_get_len(&response_header_fields));
            write(comm_socket, response_payload.data, buf_get_len(&response_payload));
        }

        close(comm_socket);
    }

    return EXIT_SUCCESS;
}
