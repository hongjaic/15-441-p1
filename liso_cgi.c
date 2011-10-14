/**
 * CS 15-441 Computer Networks
 *
 * @file    liso_cgi.c
 * @author  Hong Jai Cho <hongjaic@andrew.cmu.edu>
 */

#include "liso_cgi.h"

int cgi_init(es_connection *connection)
{
    pid_t pid;

    if (pipe(connection->stdin_pipe) < 0)
    {
        perror("pipe");
        return -1;
    }

    if (pipe(connection->stdout_pipe) < 0)
    {
        perror("pipe");
        return -1;

    }

    pid = fork();

    if (pid < 0)
    {
        perror("fork");
        return pid;
    }

    if (pid == 0)
    {
        close((connection->stdout_pipe)[0]);
        close((connection->stdin_pipe)[1]);
        dup2((connection->stdout_pipe)[1], fileno(stdout));
        dup2((connection->stdin_pipe)[0], fileno(stdin));
        
        if(execve(connection->dir, ARGV, ENVP) < 0)
        {
            //execve_error_handler();
            perror("execve");
            return -1;
        }
    }

    if (pid > 0)
    {
        close((connection->stdout_pipe)[1]);
        close((connection->stdin_pipe)[0]);
    }

    return pid;
}

int cgi_write(es_connection *connection, int writesize)
{
    int writeret;

    if ((writeret = write((connection->stdin_pipe)[1], connection->buf, writesize)) < 0)
    {
        perror("write");
        return -1;
    }

    return writeret;
}

int cgi_send_response(es_connection *connection, int i)
{
    int sent;
    int read;
    char temp[8096];

    read = cgi_read(connection, connection->response);

    if (read < 0)
    {
        return -1;
    }
    
    (connection->responseIndex) += read;

    sent = send(i, connection->response, connection->responseIndex, 0);

    if (sent < 0)
    {
        return -1;
    }

    if (sent == connection->responseIndex)
    {
        memset(connection->response, 0, 8096);
        connection->responseIndex = 0;
    }
    else
    {
        memset(temp, 0, 8096);
        memcpy(temp, connection->response + sent, connection->responseIndex - sent);
        memset(connection->response, 0, connection->responseIndex);
        memcpy(connection->response , temp, connection->responseIndex - sent);
        (connection->responseIndex) -= sent; 
    }

    return sent;
}

int cgi_read(es_connection *connection, char *buf)
{
    int readret;

    if ((readret = read((connection->stdout_pipe)[0], buf, 8096 - connection->responseIndex)) < 0)
    {
        perror("read");
        return -1;
    }

    return readret;
}

int cgi_close_parent_pipe(es_connection *connection)
{
    close((connection->stdout_pipe)[0]);
    close((connection->stdin_pipe)[1]);
    
    return 1;
}
