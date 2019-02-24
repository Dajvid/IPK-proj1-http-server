#include <stdio.h>
#include <stdlib.h>

/* enumeration of server error codes */
typedef enum {
    SUCCESS, /**< Operation was succesful */
    ERR_MEM, /**< Error in memory allocation */
    ERR_INT, /**< Internal error */
    BAD_REQ,
} SERVER_ERR;

/**
 * @brief Macro to simplify check of returned values, returns ret if condition cond isn't met.
 *
 * @param[in] cond Condition.
 * @param[in] ret Value that is returned if condition is evalued as false
 */
#define IF_RET(cond, ret) {if (cond) {return ret;}}

SERVER_ERR sys_com_to_stdin(char *command);
SERVER_ERR load_result(int fd, char **out);
