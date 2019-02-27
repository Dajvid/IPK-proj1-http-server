#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

#include "dyn_buffer.h"

BUF_ERR
buf_init(buffer *buf)
{
    buf->data = malloc(sizeof *buf->data * 2);

    if (!buf->data) {
        return BUF_ERR_MEM;
    }

    buf->size = 2;
    buf->used = 1;
    buf->data[0] = '\0';

    return BUF_SUCCESS;
}

bool
buf_can_fit(buffer *buf, size_t len)
{
    return((buf->size - buf->used) > len);
}

BUF_ERR
buf_concat(buffer *buf, char *data, size_t len)
{
    if (len == 0) {
        len = strlen(data);
    }

    while (!buf_can_fit(buf, len)) {
        if (buf_enlarge(buf) != BUF_SUCCESS) {
            return BUF_ERR_MEM;
        }
    }

    strcat(buf->data, data);
    buf->used += len;
    return BUF_SUCCESS;
}

BUF_ERR
buf_append(buffer *buf, char c)
{
    if (!buf_can_fit(buf, 1)) {
        if (buf_enlarge(buf) != BUF_SUCCESS) {
            return BUF_ERR_MEM;
        }
    }

    buf->data[buf->used - 1] = c;
    buf->data[buf->used] = '\0';
    buf->used++;
    return BUF_SUCCESS;
}

BUF_ERR
buf_enlarge(buffer *buf)
{
    char *new_data = realloc(buf->data, buf->size * 2);
    if (!new_data) {
        return BUF_ERR_MEM;
    }
    buf->size *= 2;
    buf->data = new_data;
    return BUF_SUCCESS;
}

BUF_ERR
buf_printf(buffer *buf, char *fstring, ...)
{
    int printed = 0, free_size = 0;
    BUF_ERR ret = BUF_SUCCESS;

	va_list arguments;
	va_start(arguments, fstring);
	va_end(arguments);

    if (!buf) {
        vfprintf(stdout, fstring, arguments);
        return BUF_SUCCESS;
    }

    /* enlarge buffer if it's really full */
    if (!buf_can_fit(buf, 1)) {
        ret = buf_enlarge(buf);
        IF_RETURN(ret != BUF_SUCCESS, ret);
    }

    /* try to write whole string to buffer */
    free_size = buf->size - buf->used + 1;
    printed = vsnprintf(&buf->data[buf->used - 1], free_size, fstring, arguments);

    /* if limit of bufer was reached enlarge and try to print again */
    while (printed + 1 > free_size) {
        ret = buf_enlarge(buf);
        IF_RETURN(ret != BUF_SUCCESS, ret);
        free_size = buf->size - buf->used + 1;
        va_start(arguments, fstring);
        va_end(arguments);
        printed = vsnprintf(&buf->data[buf->used - 1], free_size, fstring, arguments);
    }
    buf->used += printed;

    return BUF_SUCCESS;
}

void buf_flush(buffer *buf)
{
    buf->data[0] = '\0';
    buf->used = 1;
}

void buf_destroy(buffer *buf)
{
    free(buf->data);
}