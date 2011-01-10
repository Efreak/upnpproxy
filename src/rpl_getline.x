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
