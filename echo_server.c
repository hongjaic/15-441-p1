/******************************************************************************
 * echo_server.c                                                               *
 *                                                                             *
 * Description: This file contains the C source code for an echo server.  The  *
 *              server runs on a hard-coded port and simply write back anything*
 *              sent to it by connected clients.  It does not support          *
 *              concurrent clients.                                            *
 *                                                                             *
 * Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                         *
 *          Wolf Richter <wolf@cs.cmu.edu>                                     *
 *                                                                             *
 *******************************************************************************/

#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>

#include <sys/select.h>
#include "es_connection.h"
#include "es_error_handler.h"

#define ECHO_PORT 9999
#define BUF_SIZE 4096
#define MAX_CONNECTIONS 1024

int close_socket(int sock)
{
    if (close(sock))
    {
        fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    return 0;
}

int main(int argc, char* argv[])
{
    int sock, client_sock;
    ssize_t readret, writeret;
    socklen_t cli_size;
    struct sockaddr_in addr, cli_addr;
    char buf[BUF_SIZE];

    int selReturn = - 1;

    fd_set rfds;
    fd_set temp_rfds;
    fd_set wfds;
    fd_set temp_wfds;

    int nfds = 0;
    int fdmax;
    int i;

    int errnoSave;

    es_connection connections[MAX_CONNECTIONS];

    /* initialize the scokedt functions error handlers */
    es_error_handler_setup();
    signal(SIGPIPE, SIG_IGN);

    fprintf(stdout, "----- Echo Server -----\n");

    /* all networked programs must create a socket */
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        errnoSave = errno;
        fprintf(stderr, "Failed creating socket.\n");
        es_socket_error_handle(errnoSave);
        return EXIT_FAILURE;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(ECHO_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    /* servers bind sockets to ports---notify the OS they accept connections */
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)))
    {
        errnoSave = errno;
        close_socket(sock);
        fprintf(stderr, "Failed binding socket.\n");
        es_bind_error_handle(errnoSave);
        return EXIT_FAILURE;
    }


    if (listen(sock, 5))
    {
        errnoSave = errno;
        close_socket(sock);
        fprintf(stderr, "Error listening on socket.\n");
        es_listen_error_handle(errnoSave);
        return EXIT_FAILURE;
    }

    /* have the file descriptors ready for multiplexing */
    FD_ZERO(&rfds);
    FD_SET(sock, &rfds);
    FD_ZERO(&wfds);

    /* 
     * current total number of file descriptors including stdin, stdout, stderr
     * and server socket
     */
    nfds = 4;

    fdmax = sock;

    /* finally, loop waiting for input and then write it back */
    while (1)
    {
        printf("%d\n", nfds);
        /* temp fds to maintain state of each of the select function calls */
        temp_rfds = rfds;
        temp_wfds = wfds;

        selReturn = select(fdmax + 1, &temp_rfds, &temp_wfds, NULL, NULL);

        if (selReturn < 0)
        {
            close_socket(sock);
            perror("select");
            exit(4);
        }

        if (selReturn == 0)
        {
            continue;
        }

        for (i = 0; i < fdmax + 1; i++)
        {
            /* handle reads, including listening to new connections */
            if (FD_ISSET(i, &temp_rfds))
            {
                /* saw a change in the server socket, new connection incoming */
                if (i == sock)
                {
                    /* 
                     * accept a new connection only if there are fewer file
                     * descriptors than the nax number allowed
                     */
                    if (nfds < MAX_CONNECTIONS) 
                    {
                        printf("Trying to accept a client\n");

                        cli_size = sizeof(cli_addr);
                        client_sock = accept(sock, (struct sockaddr *) &cli_addr, &cli_size);

                        /* error occured accepting a client connection */
                        if(client_sock < 0)
                        {
                            errnoSave = errno;
                            close(sock);
                            fprintf(stderr, "Error accepting connection.\n");
                            es_accept_error_handle(errnoSave);
                            return EXIT_FAILURE;
                        }

                        /* 
                         * include accepted client to fds so that the server can
                         * listen to client requests
                         */
                        FD_SET(client_sock, &rfds);

                        /* intialize the acceptec client states */
                        connections[client_sock].bufindex = 0;
                        connections[client_sock].bufAvailable = 1;
                        memset(buf, 0, BUF_SIZE);
                        memset(connections[client_sock].buf, 0, BUF_SIZE);

                        /* 
                         * update the max file descriptor number so that the for
                         * loop can listen to all file descriptors
                         */
                        if (client_sock > fdmax)
                        {
                            fdmax = client_sock;
                        }

                        /* 
                         * update the current number of file descriptors for the
                         * new connection
                         */
                        nfds++;

                        printf("Client connection accepted: fd %d\n", client_sock);
                    } 
                    else 
                    {
                        /* 
                         * already reached the max number of connections, do
                         * nothing
                         */
                        printf("Client connect number at MAX. Refused/deferred client connection request.\n");
                    }
                }
                else
                {
                    readret = 0;
                    writeret = 0;

                    printf("Trying to receive from a client: fd %d\n", i);

                    /* 
                     * For this client, try to receive more only if the buf
                     * is empty
                     */
                    if (connections[i].bufAvailable == 1)
                    {
                        readret = recv(i, buf, BUF_SIZE, MSG_DONTWAIT);

                        /* recv error */
                        if (readret == -1)
                        {
                            errnoSave = errno;
                            memset(buf, 0, BUF_SIZE);
                            FD_CLR(i, &rfds);
                            FD_CLR(i, &wfds);
                            close_socket(i);
                            fprintf(stderr, "Error reading from client socket.\n");

                            /*
                             * This error is not client disconnect. Server
                             * terminates.
                             */
                            if (es_recv_error_handle(errnoSave) != 1)
                            {
                                close_socket(sock);
                                return EXIT_FAILURE;
                            }

                            /* 
                             * Server does not exit and continues if the
                             * client has disconnected.
                             */
                            nfds--;
                            continue;
                        }

                        /* 
                         * IO between client is over. Do all necessary clean
                         * up and close connection.
                         */
                        if (readret == 0)
                        {
                            printf("No more to read.\n");
                            FD_CLR(i, &rfds);
                            FD_CLR(i, &wfds);

                            if (close_socket(i))
                            {
                                close_socket(sock);
                                fprintf(stderr, "Error closing client socket.\n");
                                return EXIT_FAILURE;
                            }
                            printf("Connection close sucessful: fd %d\n", i);

                            nfds--;
                            continue;
                        }

                        /* 
                         * recv read from client send, copy read data into
                         * connection specific buffer.
                         */
                        memcpy(connections[i].buf, buf, readret);
                        connections[i].bufindex += readret;

                        /* data is ready, make writeable to client */
                        FD_SET(i, &wfds);

                        /* 
                         * the client buffer has data, make it unavailable
                         * until data is sent back to client.
                         */
                        connections[i].bufAvailable = 0;

                        printf("Receive complete: fd %d\n", i);
                    }
                }
            }

            if (FD_ISSET(i, &temp_wfds))
            {
                printf("Trying to send to a client: fd %d\n", i);

                writeret = 0;

                writeret = send(i, connections[i].buf, connections[i].bufindex, MSG_DONTWAIT);

                /* send error */
                if (writeret < 0)
                {
                    errnoSave = errno;
                    FD_CLR(i, &rfds);
                    FD_CLR(i, &wfds);

                    close_socket(i);

                    /* error is not client disconnection, terminate the server */
                    if (es_send_error_handle(errnoSave) != 1)
                    {
                        close_socket(sock);
                        return EXIT_FAILURE;
                    }

                    /* 
                     * error is client disconnection, keep the server alive and
                     * do cleanup
                     */
                    nfds--;

                    memset(connections[i].buf, 0, BUF_SIZE);
                    connections[i].bufindex = 0;
                    connections[i].bufAvailable = 1;

                    continue;
                }

                /* 
                 * For some reason, size of sent data and size of received data
                 * do not match. Terminate the server.
                 */
                if (writeret != connections[i].bufindex)
                {   
                    errnoSave = errno;
                    close_socket(i);
                    close_socket(sock);
                    fprintf(stderr, "Error sending to client.\n");
                    return EXIT_FAILURE;
                }

                memset(connections[i].buf, 0, BUF_SIZE);
                connections[i].bufindex = 0;
                connections[i].bufAvailable = 1;

                printf("Send complete: fd %d\n", i);
            }
        }
    }

    close_socket(sock);

    return EXIT_SUCCESS;
}
