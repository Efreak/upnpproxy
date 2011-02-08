/*
 * Copyright (C) 2011, Joel Klinghed.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

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
