#include <stdio.h>
#include <stdlib.h>

/* enumeration of server error codes */
typedef enum {
    SUCCESS = EXIT_SUCCESS,                /**< Operation was succesful */
    ERR_MEM,                               /**< Error in memory allocation */
    ERR_INT = EXIT_FAILURE,                /**< Internal error */
    ERR_BAD_REQ,                           /**< Invalid request */
    ERR_BAD_METHOD,                        /**< Unsupported or unknown request method */
    ERR_BAD_VERSION,                       /**< Unknown HTTP version */
    ERR_BAD_PATH,                          /**< Unknown path */
} SERVER_ERR;

typedef enum {
    TYPE_TEXT_PLAIN,                       /**< Response is requested in plain text (default option) */
    TYPE_APPLICATION_JSON,                 /**< Response is requested in json */
} RESPONSE_TYPE;

typedef enum {
    REQ_UNKNOWN,                           /**< Unknown path */
    REQ_HOSTNAME,                          /**< Hostname should be returned */
    REQ_CPU_NAME,                          /**< CPU name should be returned */
    REQ_LOAD,                              /**< CPU load should be returned */
} PATH;

#define NOT_FOUND_HEADER "HTTP/1.1 404 Not Found\n\n"
#define BAD_REQUEST_HEADER "HTTP/1.1 400 Bad Request\n\n"
#define OK_HEADER "HTTP/1.1 200 OK\n"

/**
 * @brief Macro to simplify check of returned values, returns ret if condition cond isn't met.
 *
 * @param[in] cond Condition.
 * @param[in] ret Value that is returned if condition is evalued as false
 */
#define IF_RET(cond, ret) {if (cond) {return ret;}}

SERVER_ERR sys_com_to_stdin(char *command);
SERVER_ERR load_result(int fd, char **out);
