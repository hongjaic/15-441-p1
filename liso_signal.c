/**
 * CS 15-441 Computer Networks
 *
 * @file    liso_signal.c
 * @author  Hong Jai Cho <hongjaic@andrew.cmu.edu>
 */

#include "liso_signal.h"

void sigchld_handler(int sig)
{
    pid_t pid;			/* process id of child */
    int status;			/* status of child */

    /* parent waits for jobs to terminate */
    while ((pid = waitpid(-1, &status, WNOHANG|WUNTRACED|WCONTINUED)) > 0)
    {
    
    }

    /* indicate waitpid failure */
    if (pid == -1 && errno != ECHILD)
    {
        fprintf(stderr, "waitpid error: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
}
