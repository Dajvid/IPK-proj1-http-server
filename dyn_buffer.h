#include <stdio.h>
#include <stdlib.h>

#define IF_RETURN(cond, ret) {if (cond) {return ret;}}

struct buffer {
    char *data;
    size_t size;
    size_t used;
};

typedef struct buffer buffer;

typedef enum {
    BUF_SUCCESS,
    BUF_ERR_MEM,
} BUF_ERR;

BUF_ERR buf_init(buffer *buf, size_t initial_size);
bool buf_can_fit(buffer *buf, size_t len);
BUF_ERR buf_concat(buffer *buf, char *data, size_t len);
BUF_ERR buf_enlarge(buffer *buf);
BUF_ERR buf_printf(buffer *buf, char *fstring, ...);
BUF_ERR buf_append(buffer *buf, char c);
void buf_flush(buffer *buf);
void buf_destroy(buffer *buf);
