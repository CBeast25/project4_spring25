#include <stdio.h>
#include <stdlib.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>

#define N 13

extern char **environ;
char uName[20];

char *allowed[N] = {"cp", "touch", "mkdir", "ls", "pwd", "cat", "grep", "chmod", "diff", "cd", "exit", "help", "sendmsg"};

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

void sendmsg(char *user, char *target, char *msg) {
    int serverfd;
    struct message m;

    strcpy(m.source, user);
    strcpy(m.target, target);
    strcpy(m.msg, msg);

    serverfd = open("serverFIFO", O_WRONLY);
    if (serverfd == -1) {
        perror("open serverFIFO");
        return;
    }

    write(serverfd, &m, sizeof(struct message));
    close(serverfd);
}

void* messageListener(void *arg) {
    int fd;
    struct message m;
    char fifoName[64];

    sprintf(fifoName, "%s", uName);
    fd = open(fifoName, O_RDONLY);
    if (fd == -1) {
        perror("open user FIFO");
        pthread_exit(NULL);
    }

    while (1) {
        if (read(fd, &m, sizeof(struct message)) > 0) {
            printf("\nIncoming message from %s: %s\n", m.source, m.msg);
            fprintf(stderr, "rsh>");
            fflush(stdout);
        }
    }

    close(fd);
    pthread_exit(NULL);
}

int isAllowed(const char* cmd) {
    for (int i = 0; i < N; i++) {
        if (strcmp(cmd, allowed[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    pid_t pid;
    char **cargv;
    char *path;
    char line[256];
    int status;
    posix_spawnattr_t attr;

    if (argc != 2) {
        printf("Usage: ./rsh <username>\n");
        exit(1);
    }

    signal(SIGINT, terminate);
    strcpy(uName, argv[1]);

    // Create listener thread
    pthread_t tid;
    if (pthread_create(&tid, NULL, messageListener, NULL) != 0) {
        perror("pthread_create");
        exit(1);
    }

    while (1) {
        fprintf(stderr, "rsh>");
        if (fgets(line, 256, stdin) == NULL) continue;
        if (strcmp(line, "\n") == 0) continue;

        line[strlen(line) - 1] = '\0';

        char cmd[256];
        char line2[256];
        strcpy(line2, line);
        strcpy(cmd, strtok(line, " "));

        if (!isAllowed(cmd)) {
            printf("NOT ALLOWED!\n");
            continue;
        }

        if (strcmp(cmd, "sendmsg") == 0) {
            char *targetUser = strtok(NULL, " ");
            if (!targetUser) {
                printf("sendmsg: you have to specify target user\n");
                continue;
            }

            char *msgBody = strtok(NULL, "");
            if (!msgBody) {
                printf("sendmsg: you have to enter a message\n");
                continue;
            }

            sendmsg(uName, targetUser, msgBody);
            continue;
        }

        if (strcmp(cmd, "exit") == 0) break;

        if (strcmp(cmd, "cd") == 0) {
            char *targetDir = strtok(NULL, " ");
            if (strtok(NULL, " ") != NULL) {
                printf("-rsh: cd: too many arguments\n");
            } else {
                chdir(targetDir);
            }
            continue;
        }

        if (strcmp(cmd, "help") == 0) {
            printf("The allowed commands are:\n");
            for (int i = 0; i < N; i++) {
                printf("%d: %s\n", i + 1, allowed[i]);
            }
            continue;
        }

        // Handle external command
        cargv = (char**)malloc(sizeof(char*));
        cargv[0] = (char *)malloc(strlen(cmd) + 1);
        path = (char *)malloc(strlen(cmd) + 1);
        strcpy(path, cmd);
        strcpy(cargv[0], cmd);

        char *attrToken = strtok(line2, " ");
        attrToken = strtok(NULL, " ");
        int n = 1;
        while (attrToken != NULL) {
            n++;
            cargv = (char**)realloc(cargv, sizeof(char*) * n);
            cargv[n - 1] = (char *)malloc(strlen(attrToken) + 1);
            strcpy(cargv[n - 1], attrToken);
            attrToken = strtok(NULL, " ");
        }
        cargv = (char**)realloc(cargv, sizeof(char*) * (n + 1));
        cargv[n] = NULL;

        posix_spawnattr_init(&attr);
        if (posix_spawnp(&pid, path, NULL, &attr, cargv, environ) != 0) {
            perror("spawn failed");
            exit(EXIT_FAILURE);
        }

        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid failed");
            exit(EXIT_FAILURE);
        }

        posix_spawnattr_destroy(&attr);
    }

    return 0;
}