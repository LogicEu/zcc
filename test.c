#include <zio.h>
#include <zassert.h>
#include <zcc.h>
#include <zsolver.h>

int main(int argc, char** argv)
{
    if (argc < 2) {
        zcc_log("Missing input expression.\n");
        return 1;
    }

    long n = zcc_solve(argv[1]);
    zcc_log("%s\n%ld\n", argv[1], n);
    return 0;
}
