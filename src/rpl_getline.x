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

int rpl_getline(char** buf, size_t* buflen, FILE* fh)
{
    int ret = 0;
    size_t size = *buflen;
    char* b = *buf;

    if (b == NULL || size < 2)
    {
        size = 128;
        b = realloc(b, size);
        if (b == NULL)
        {
            return -1;
        }
    }

    size -= 2;

#if HAVE_GETC_UNLOCKED
    flockfile(fh);
#endif

    for (;;)
    {
        int c;
#if HAVE_GETC_UNLOCKED
        c = getc_unlocked(fh);
#else
        c = getc(fh);
#endif
        if (c == EOF)
        {
            b[ret] = '\0';
            if (ret == 0)
            {
                ret = -1;
            }
            break;
        }

        if (ret == size)
        {
            size_t ns = size * 2;
            char* tmp;
            tmp = realloc(b, ns + 2);
            if (tmp == NULL)
            {
                if (b != *buf)
                {
                    free(b);
                    b = NULL;
                }
                ret = -1;
                break;
            }
            size = ns;
            b = tmp;
        }
        b[ret++] = (char)c;

        if (c == '\r')
        {
#if HAVE_GETC_UNLOCKED
            c = getc_unlocked(fh);
#else
            c = getc(fh);
#endif
            if (c != '\n')
            {
                if (c != EOF)
                {
#if HAVE_UNGETC_UNLOCKED
                    ungetc_unlocked(c, fh);
#elif HAVE_GETC_UNLOCKED
                    funlockfile(fh);
                    ungetc(c, fh);
                    flockfile(fh);
#else
                    ungetc(c, fh);
#endif
                }
            }
            b[ret - 1] = '\n';
            b[ret] = '\0';
            break;
        }
        if (c == '\n')
        {
            b[ret] = '\0';
            break;
        }
    }

    *buf = b;
    *buflen = size + 2;

#if HAVE_GETC_UNLOCKED
    funlockfile(fh);
#endif
    return ret;
}
