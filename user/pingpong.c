#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char* argv[])
{
    int  pipePing[2];
    int  pipePong[2];
    char receivedByte = 'B';

    pipe(pipePing);
    pipe(pipePong);

    // pipe[0] for read, pipe[1] for write.
    int pid = fork();
    if (pid == 0) {
        char buf_son[1];

        close(pipePing[1]);
        read(pipePing[0], buf_son, sizeof(buf_son));
        close(pipePing[0]);

        printf("%d: received ping\n", getpid());

        close(pipePong[0]);
        write(pipePong[1], &receivedByte, 1);
        close(pipePong[1]);

        exit(0);
    }
    else {
        char buf_parent[1];

        close(pipePing[0]);
        write(pipePing[1], &receivedByte, 1);
        close(pipePing[1]);

        close(pipePong[1]);
        read(pipePong[0], buf_parent, sizeof(buf_parent));
        close(pipePong[0]);

        printf("%d: received pong\n", getpid());

        exit(0);
    }
}