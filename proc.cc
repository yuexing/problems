#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <fcntl.h>

#include <iostream>
#include <string>
using namespace std;

extern char **environ;      /* defined in libc */

void test_execve()
{
    int pid = fork();
    if (pid) {
        printf("parent\n");
    } else {
        printf("child\n");
        int fd = open("file", O_CREAT|O_WRONLY, S_IRWXU);
        dup2(fd, 1);
        write (1, "test", 5);
        char *argv[] = {"cov-analyze", "--dir"};
        if(execve(argv[0], argv, environ)) {
            strerror(errno);
            exit(-1);
        }
    }

    int status;
    waitpid (pid, &status, 0);
    if (WIFEXITED(status)) {
        printf ("exit normally.\n");
    }
}

void test_pipe()
{
    int pipefd[2];
    pid_t cpid;
    char buf;

    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    cpid = fork();
    if (cpid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (cpid == 0) {    /* Child reads from pipe */
        close(pipefd[1]);          /* Close unused write end */

        while(read(pipefd[0], &buf, 1) > 0)
            write(STDOUT_FILENO, &buf, 1);

        write(STDOUT_FILENO, "\n", 1);
        close(pipefd[0]);
        _exit(EXIT_SUCCESS);

    } else {            /* Parent writes  to pipe */
        close(pipefd[0]);          /* Close unused read end */

        string input;
        while(cin >> input) {
            input += "\n";
            write(pipefd[1], input.c_str(), input.size());
        }

        close(pipefd[1]);          /* Reader will see EOF */
        wait(NULL);                /* Wait for child */
        exit(EXIT_SUCCESS);
    }
}

// FD_CLR, FD_ISSET, FD_SET, FD_ZERO: synchronize IO multiplexing.
struct time_server_t
{
    // spawn a child process keeping asking for million-seconds and writing to
    // mmaped_time.
    time_server_t()
    {
        
    }

    // close the pipe, so that the child get informed. 
    // wait for child
    ~time_server_t()
    {

    }

    int get()
    {
        return *mmaped_time;
    }

private:
    int pid;
    int *mmaped_time;
};

/*
int main()
{
    test_pipe();
}*/
