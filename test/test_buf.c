#include "common.h"

#include <stdio.h>

#include "buf.h"

int main(int argc, char** argv)
{
    unsigned int tot = 0, cnt = 0;

    

    fprintf(stdout, "OK %u/%u\n", cnt, tot);

    return cnt == tot ? EXIT_SUCCESS : EXIT_FAILURE;
}
