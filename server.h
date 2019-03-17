#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>

/* enumeration of server error codes */
typedef enum {
    SUCCESS = EXIT_SUCCESS,                /**< Operation was succesful */
    ERR_MEM,                               /**< Error in memory allocation */
    ERR_INT,                               /**< Internal error */
    ERR_BAD_REQ,                           /**< Invalid request */
    ERR_BAD_METHOD,                        /**< Unsupported or unknown request method */
    ERR_BAD_VERSION,                       /**< Unknown HTTP version */
    ERR_BAD_PATH,                          /**< Unknown path */
    ERR_UNABLE_TO_RESPOND,
    CLOSE,
} SERVER_ERR;

typedef enum {
    HTTP10,
    HTTP11,
} HTTP_VERSION;

typedef enum {
    TYPE_TEXT_PLAIN         = 0,           /**< Response is requested in plain text (default option) */
    TYPE_APPLICATION_JSON   = 1,           /**< Response is requested in json */
    TYPE_UNSUPPORTED        = 3,
} RESPONSE_TYPE;

static const char *RESPONSE_TYPE_STRING[2] = {
    "text/plain",
    "application/json",
};

typedef enum {
    REQ_UNKNOWN,                           /**< Unknown path */
    REQ_HOSTNAME,                          /**< Hostname should be returned */
    REQ_CPU_NAME,                          /**< CPU name should be returned */
    REQ_LOAD,                              /**< CPU load should be returned */
} PATH;

bool volatile terminate = false;

typedef enum {
    USER_IDLE               = 0,
    NICE_IDLE               = 1,
    SYSTEM_IDLE             = 2,
    IDLE_IDLE               = 3,
    IOWAIT_IDLE             = 4,
    IRQ_IDLE                = 5,
    SOFT_IRQ_IDLE           = 6,
    STEAL_IDLE              = 7,
    GUEST_IDLE              = 8,
    GUEST_NICE_IDLE         = 9,
} IDLE_NAMES;

#define NOT_FOUND_HEADER "HTTP/1.1 404 Not Found\r\n\r\n"
#define BAD_REQUEST_HEADER "HTTP/1.1 400 Bad Request\r\n\r\n"
#define OK_HEADER "HTTP/1.1 200 OK\r\n"
#define BAD_TYPE_HEADER "HTTP/1.1 406 Not Acceptable\r\n\r\n"
#define DEFULT_TIMEOUT 5
/**
 * @brief Macro to simplify check of returned values, returns ret if condition cond isn't met.
 *
 * @param[in] cond Condition.
 * @param[in] ret Value that is returned if condition is evalued as false
 */
#define IF_RET(cond, ret) {if (cond) {return ret;}}
#define IF_GOTO(cond, label) {if (cond) {goto label;}}
SERVER_ERR sys_com_to_stdin(char *command);
SERVER_ERR load_result(int fd, char **out);
char *get_header_field_content(char *header, char *field, int *length);
RESPONSE_TYPE get_response_type(char *header);
SERVER_ERR parse_request_line(char **header_ptr, PATH *requested_path, long long int *ref_time, HTTP_VERSION *version);
SERVER_ERR get_cpu_name(char **res, int fd, RESPONSE_TYPE res_type);
SERVER_ERR get_hostname(char **res, int fd, RESPONSE_TYPE res_type);
SERVER_ERR parse_idle(int fd, long double *idle, long double *non_idle);
SERVER_ERR get_cpu_usage(char **res, int fd, RESPONSE_TYPE res_type);
SERVER_ERR load_header(int fd, char **out);
SERVER_ERR serve_client(int client_fd, int in_fd);
bool iskeep_alive(char *header);
