#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char* argv[])
{
    // pipe[0] for read, pipe[1] for write.
    int pipe_base[2];
    pipe(pipe_base);

    if (fork() > 0) {
        close(pipe_base[0]);
        for (int i = 2; i <= 35; i++) {
            write(pipe_base[1], &i, sizeof(int));
        }
        close(pipe_base[1]);
        wait((int*)0);
    }
    else {
        int min;
        close(pipe_base[1]);

        while (read(pipe_base[0], &min, sizeof(int))) {
            printf("prime %d\n", min);
            int pipe_primes[2];
            pipe(pipe_primes);
            int i;
            while (read(pipe_base[0], &i, sizeof(int))) {
                if (i % min != 0) {
                    write(pipe_primes[1], &i, sizeof(int));
                }
            }
            close(pipe_primes[1]);

            if (fork() == 0) {
                pipe_base[0] = dup(pipe_primes[0]);
                close(pipe_primes[0]);
            }
            else {
                close(pipe_primes[0]);
                wait((int*)0);
            }
        }
    }
    exit(0);
}