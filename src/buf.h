#ifndef BUF_H
#define BUF_H

typedef struct _buf_t* buf_t;

buf_t buf_new(size_t size);
void buf_free(buf_t buf);

size_t buf_wavail(buf_t buf);
size_t buf_ravail(buf_t buf);

char* buf_wptr(buf_t buf, size_t* avail);
size_t buf_wmove(buf_t buf, size_t size);

const char* buf_rptr(buf_t buf, size_t* avail);
size_t buf_rmove(buf_t buf, size_t size);

size_t buf_skip(buf_t buf, size_t size);

size_t buf_write(buf_t buf, const void* data, size_t size);
size_t buf_read(buf_t buf, void* data, size_t size);
size_t buf_peek(buf_t buf, void* data, size_t size);

/* This is buf_write, except that it writes to the start of rptr and not
 * the wptr and does not move the rptr - so more like buf_peek but it writes */
size_t buf_replace(buf_t buf, const void* data, size_t size);

#endif /* BUF_H */
