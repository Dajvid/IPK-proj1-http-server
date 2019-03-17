#include "server.h"
#include "dyn_buffer.h"

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
        while (*iter != '\r') {
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
    buffer tmp;
    field = get_header_field_content(header, "Content-Type", &field_len);

    if (field_len == 0) {
        return TYPE_TEXT_PLAIN;
    }

    buf_init(&tmp, 64);
    buf_concat(&tmp, field, field_len);

    if (strstr("text/html,", tmp.data) || strstr("text/html\r\n", tmp.data) || strstr("*/*", tmp.data) || strstr("*/*\r\n", tmp.data)) {
        return TYPE_TEXT_PLAIN;
    }
    if (strstr("application/json,", tmp.data) || strstr("application/json\r\n", tmp.data)) {
        return TYPE_APPLICATION_JSON;
    } else {
        return TYPE_UNSUPPORTED;
    }
}

SERVER_ERR
parse_request_line(char **header_ptr, PATH *requested_path, long long int *ref_time, HTTP_VERSION *version)
{
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
        *ref_time = -1;
        *requested_path = REQ_LOAD;
        *header_ptr = &((*header_ptr)[6]);
    } else if (strncmp(*header_ptr, "/load?refresh=", 14) == 0) {
        *requested_path = REQ_LOAD;
        *header_ptr = &((*header_ptr)[14]);
        *ref_time = strtoll(*header_ptr, header_ptr, 10);
        if ((*header_ptr)[0] != ' ') {
            return ERR_BAD_REQ;
        }
        *header_ptr = &((*header_ptr)[1]);
    } else {
        *requested_path = REQ_UNKNOWN;
        return ERR_BAD_PATH;
    }

    if (strncmp(*header_ptr, "HTTP/1.1\r\n", 10) == 0) {
        *header_ptr = &((*header_ptr)[9]);
        *version = HTTP11;
        return SUCCESS;
    } else if (strncmp(*header_ptr, "HTTP/1.0\r\n", 10) == 0) {
        *header_ptr = &((*header_ptr)[9]);
        *version = HTTP10;
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
get_hostname(char **res, int fd, RESPONSE_TYPE res_type)
{
    SERVER_ERR ret = SUCCESS;
    char *hostname;
    buffer json_buf;

    ret = sys_com_to_stdin("uname -n");
    IF_RET(ret != SUCCESS, ret);
    load_result(fd, &hostname);

    if (res_type == TYPE_APPLICATION_JSON) {
        buf_init(&json_buf, 32);
        buf_printf(&json_buf, "{\"hostname\":\"%s\"}", hostname);
        *res = json_buf.data;
        free(hostname);
    } else {
        *res = hostname;
    }

    return ret;
}

SERVER_ERR
parse_idle(int fd, long double *idle, long double *non_idle)
{
    long double idle_vals[10];
    SERVER_ERR ret = SUCCESS;
    char *end_ptr, *idle_line;

    ret = sys_com_to_stdin("head -n 1 /proc/stat | cut -c 6-");
    IF_RET(ret != SUCCESS, ret);
    load_result(fd, &idle_line);
    end_ptr = idle_line;

    for (int i = USER_IDLE; i <= GUEST_NICE_IDLE; i++) {
        idle_vals[i] = strtold(end_ptr, &end_ptr);
        if (i != GUEST_NICE_IDLE) {
            IF_RET(end_ptr[0] != ' ', ERR_INT);
            end_ptr = &end_ptr[1];
        } else {
            IF_RET(end_ptr[0] != '\0', ERR_INT);
        }
    }

    *non_idle = idle_vals[USER_IDLE] + idle_vals[NICE_IDLE] + idle_vals[SYSTEM_IDLE] + idle_vals[IRQ_IDLE] + idle_vals[SOFT_IRQ_IDLE] + idle_vals[STEAL_IDLE];
    *idle = idle_vals[IDLE_IDLE] + idle_vals[IOWAIT_IDLE];

    free(idle_line);
    return SUCCESS;
}

SERVER_ERR
get_cpu_usage(char **res, int fd, RESPONSE_TYPE res_type)
{
    SERVER_ERR ret = SUCCESS;
    long double prev_idle, prev_non_idle, idle, non_idle, usage, totald, idled;
    buffer output_buf;

    ret = parse_idle(fd, &prev_idle, &prev_non_idle);
    IF_RET(ret != SUCCESS, ret);
    sleep(1);
    ret = parse_idle(fd, &idle, &non_idle);
    IF_RET(ret != SUCCESS, ret);

    totald = idle + non_idle - prev_idle - prev_non_idle;
    idled = idle - prev_idle;

    usage = (totald - idled) / totald;
    buf_init(&output_buf, 16);
    if (res_type == TYPE_APPLICATION_JSON) {
        buf_printf(&output_buf, "{\"load\":\"%.2Lf%%\"}", usage);
    } else {
        buf_printf(&output_buf, "%.2Lf%%", usage);
    }

    *res = output_buf.data;
    return SUCCESS;
}

SERVER_ERR
load_header(int fd, char **out)
{
    char *end_ptr, *content, loaded = 0, last_loaded = 0;
    int content_lenght = 0;
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
    buf_destroy(&command_buffer);
    return SUCCESS;
}

void
terminate_handler()
{
    terminate = true;
}

void
unexpectedly_closed_handler()
{
    fprintf(stderr, "WARNING: client closed connection unexpectedly\n");
}

bool
is_keep_alive(char *header)
{
    char *field = NULL;
    int field_len = 0;

    field = get_header_field_content(header, "Connection", &field_len);
    IF_RET(field_len == 0, false);
    return (strncmp(field, "keep-alive", field_len) == 0);
}

SERVER_ERR
serve_client(int client_fd, int in_fd)
{
    char *cpu_name = NULL, *header = NULL, *hostname = NULL, *cpu_load = NULL, *host = NULL, *header_start = NULL;
    buffer response_header, response_payload, response_header_fields, whole_response_buf;
    SERVER_ERR ret = SUCCESS;
    long long int ref_time;
    PATH path = REQ_UNKNOWN;
    RESPONSE_TYPE response_type = TYPE_TEXT_PLAIN;
    int host_len = 0;
    ssize_t writen_len;
    bool keep_alive = false;
    HTTP_VERSION version;

    buf_init(&response_header, 64);
    buf_init(&response_payload, 64);
    buf_init(&response_header_fields, 64);
    buf_init(&whole_response_buf, 256);

    if (client_fd > 0) {
        buf_flush(&response_header);
        buf_flush(&response_header_fields);
        buf_flush(&response_payload);

        ret = load_header(client_fd, &header);
        IF_GOTO(ret != SUCCESS, cleanup);
        header_start = header;
        ret = parse_request_line(&header, &path, &ref_time, &version);

        /* asemble response */
        /* request line is valid, send OK respons with corresponding payload */
        if (ret == SUCCESS) {
            response_type = get_response_type(header);
            if (response_type == TYPE_UNSUPPORTED) {
                buf_concat(&response_header, BAD_TYPE_HEADER, 0);
            } else {
                buf_concat(&response_header, OK_HEADER, 0);

                if (path == REQ_HOSTNAME) {
                    get_hostname(&hostname, in_fd, response_type);
                    buf_concat(&response_payload, hostname, 0);
                } else if (path == REQ_CPU_NAME) {
                    get_cpu_name(&cpu_name, in_fd, response_type);
                    buf_concat(&response_payload, cpu_name, 0);
                } else if (path == REQ_LOAD) {
                    get_cpu_usage(&cpu_load, in_fd, response_type);
                    buf_concat(&response_payload, cpu_load, 0);
                    if (ref_time >= 0) {
                        host = get_header_field_content(header, "Host", &host_len);
                        buf_printf(&response_header_fields, "Refresh: %lli; url=http://", ref_time);
                        buf_concat(&response_header_fields, host, host_len);
                        buf_printf(&response_header_fields, "/load?refresh=%lli\r\n", ref_time);
                    }
                }
                if ((keep_alive = is_keep_alive(header))) {
                    buf_printf(&response_header_fields, "Keep-Alive: timeout=%d\r\n", DEFULT_TIMEOUT);
                }
                buf_printf(&response_header_fields, "Content-Type: %s\nContent-Length: %d\r\n\r\n",
                            RESPONSE_TYPE_STRING[response_type], buf_get_len(&response_payload));
            }
        /* request uses unsupported or unknown method */
        } else if (ret == ERR_BAD_METHOD) {
            buf_concat(&response_header, BAD_REQUEST_HEADER, 0);

        /* request requires invalid path */
        } else if (ret == ERR_BAD_PATH) {
            buf_concat(&response_header, NOT_FOUND_HEADER, 0);
        } else if (ret == ERR_BAD_REQ || ret == ERR_BAD_REQ) {
            buf_concat(&response_header, BAD_REQUEST_HEADER, 0);
        }

        /* send response at once */
        buf_concat(&whole_response_buf, buf_get_data(&response_header), buf_get_len(&response_header));
        buf_concat(&whole_response_buf, buf_get_data(&response_header_fields), buf_get_len(&response_header_fields));
        buf_concat(&whole_response_buf, buf_get_data(&response_payload), buf_get_len(&response_payload));

        printf("%s", whole_response_buf.data);
        writen_len = write(client_fd, buf_get_data(&whole_response_buf), buf_get_len(&whole_response_buf));
        if (writen_len == -1) {
            ret = ERR_UNABLE_TO_RESPOND;
        } else {
            ret = (version == HTTP10 && !keep_alive) ? CLOSE : SUCCESS;
        }

        /* free resources */
        free(header_start);
        header_start = NULL;
        free(hostname);
        hostname = NULL;
        free(cpu_name);
        cpu_name = NULL;
        free(cpu_load);
        cpu_load = NULL;
    }

cleanup:
    buf_destroy(&response_header);
    buf_destroy(&response_header_fields);
    buf_destroy(&response_payload);
    buf_destroy(&whole_response_buf);

    return ret;
}


int
main(int argc, char **argv)
{
    int port, rc, server_socket, pipe_fd[2];
    unsigned int client_len;
    char *endptr = NULL;
    struct sockaddr_in sa, sa_client;
    SERVER_ERR ret = SUCCESS;
    fd_set active_fd_set, read_fd_set;
    int enable = 1, ret_int = 0;
    bool timeouted[FD_SETSIZE] = {};
    struct timeval timeout;

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

    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("ERROR: in setsockopt");
        return EXIT_FAILURE;
    }

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

    FD_ZERO(&active_fd_set);
    FD_SET(server_socket, &active_fd_set);

    /* set signal interceptors */
    signal(SIGINT, terminate_handler);
    signal(SIGPIPE, unexpectedly_closed_handler);
    /* set initial timeout value */
    timeout.tv_sec = DEFULT_TIMEOUT;
    timeout.tv_usec = 0;
    /* busy waiting for client to connect */
    while (!terminate) {
        /* reinitialize fd set */
        read_fd_set = active_fd_set;
        /* TODO add timeout */
        ret_int = select(FD_SETSIZE, &read_fd_set, NULL, NULL, &timeout);
        if (ret_int < 0 && !terminate) {
            perror("ERROR: select");
            return EXIT_FAILURE;
        }
        if (!terminate) {
            if (ret_int == 0) {
                for (int i = 0; i < FD_SETSIZE; i++) {
                    if (FD_ISSET(i, &active_fd_set)) {
                        if (timeouted[i]) {
                            if (i != server_socket) {
                                close(i);
                                FD_CLR(i, &active_fd_set);
                            }
                        } else {
                            timeouted[i] = true;
                            timeout.tv_sec = DEFULT_TIMEOUT;
                            timeout.tv_usec = 0;
                        }
                    }
                }
            } else {
                for (int i = 0; i < FD_SETSIZE; i++) {
                    if (FD_ISSET(i, &read_fd_set)) {
                        /* request for new connection */
                        if (i == server_socket) {
                            int new_socket;
                            client_len = sizeof(sa_client);
                            new_socket = accept(server_socket, (struct sockaddr *)&sa_client, &client_len);
                            if (new_socket < 0) {
                                perror("ERROR: accept");
                            }
                            FD_SET(new_socket, &active_fd_set);
                        /* new request from connected client or timeout */
                        } else {
                            ret = serve_client(i, pipe_fd[0]);
                            if (ret == ERR_UNABLE_TO_RESPOND || ret == CLOSE) {
                                close(i);
                                FD_CLR(i, &active_fd_set);
                            }
                        }
                    }
                }
            }
        }
    }
    close(pipe_fd[0]);
    for (int i = 0; i < FD_SETSIZE; i++) {
        if (FD_ISSET(i, &active_fd_set)) {
            close(i);
        }
    }

    return EXIT_SUCCESS;
}
