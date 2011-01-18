#ifndef BUF_H
#define BUF_H

typedef struct _buf_t* buf_t;

buf_t buf_new(size_t size);
void buf_free(buf_t buf);

/* Return the number of bytes available for writing, totally */
size_t buf_wavail(buf_t buf);
/* Return the number of bytes available for reading, totally */
size_t buf_ravail(buf_t buf);

/* Return a pointer to part of the writeable part of the buffer,
 * available is set to the number of bytes writable in the returned ptr. */
char* buf_wptr(buf_t buf, size_t* avail);
/* Return number of bytes writable after the write ptr been moved size bytes */
size_t buf_wmove(buf_t buf, size_t size);

/* Return a pointer to part of the readable part of the buffer,
 * available is set to the number of bytes readable in the returned ptr. */
const char* buf_rptr(buf_t buf, size_t* avail);
/* Return number of bytes readable after the read ptr been moved size bytes */
size_t buf_rmove(buf_t buf, size_t size);

/* Try to skip size bytes, returns the number of bytes actually skipped */
size_t buf_skip(buf_t buf, size_t size);

/* Try to write size byte of data into the buffer, returns the number written */
size_t buf_write(buf_t buf, const void* data, size_t size);
/* Try to read size byte of the buffer into data, returns the number read */
size_t buf_read(buf_t buf, void* data, size_t size);
/* Try to peek at size byte of the buffer into data, returns the number
 * available */
size_t buf_peek(buf_t buf, void* data, size_t size);

/* This is buf_write, except that it writes to the start of rptr and not
 * the wptr and does not move the rptr - so more like buf_peek but it writes */
size_t buf_replace(buf_t buf, const void* data, size_t size);

buf_t buf_resize(buf_t buf, size_t newsize);
size_t buf_size(buf_t buf);

/* Move all readable data to the start of the buffer.
 * Returns false if not needed (ie it was already there). */
bool buf_rrotate(buf_t buf);

#endif /* BUF_H */
