#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

struct message {
    char source[50];
    char target[50];
    char msg[200];
};

void terminate(int sig) {
    printf("Exiting....\n");
    fflush(stdout);
    exit(0);
}

int main() {
    int server;
    int target;
    int dummyfd;
    struct message req;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, terminate);

    server = open("serverFIFO", O_RDONLY);
    dummyfd = open("serverFIFO", O_WRONLY); // keep open to avoid EOF

    while (1) {
        if (read(server, &req, sizeof(struct message)) <= 0) continue;

        printf("Received a request from %s to send the message %s to %s.\n",
               req.source, req.msg, req.target);

        char fifoName[64];
        sprintf(fifoName, "%s", req.target);

        target = open(fifoName, O_WRONLY);
        if (target == -1) {
            perror("open target FIFO");
            continue;
        }

        write(target, &req, sizeof(struct message));
        close(target);
    }

    close(server);
    close(dummyfd);
    return 0;
}