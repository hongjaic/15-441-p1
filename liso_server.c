/**
 * CS 15-441 Computer Networks
 *
 * @file    liso_server.c
 * @auther  Hong Jai Cho <hongjaic@andrew.cmu.edu>
 *
 * Contains the main execution loop for the liso server.
 */

#include <netinet/in.h>
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/sendfile.h>
#include "es_connection.h"
#include "es_error_handler.h"
#include "liso_hash.h"
#include "http_parser.h"
#include "liso_file_io.h"
#include "http_request.h"
#include <sys/resource.h>
#include "liso_logger.h"


#define ECHO_PORT 9999
#define BUF_SIZE 8192
#define MAX_CONNECTIONS 1024
#define MAX_URI_LENGTH  2048

#define USAGE "\nUsage: %s <PORT> <PORT> <LOG_FILE> <LOCK_FILE> <WWW> <CGI>\n\n"

int close_socket(int sock)
{
    if (close(sock))
    {
        fprintf(stderr, "Failed closing socket.\n");
        return 1;
    }
    return 0;
}

void init_connection(es_connection *connection)
{
    connection->bufindex = 0;
    memset(connection->buf, 0, BUF_SIZE);
    connection->bufAvailable = 1;
    connection->requestOverflow = 0;
    connection->hasRequestHeaders = 0;
    connection->sentResponseHeaders = 0;
    connection->iindex = -1;
    connection->postfinish = 0;
    connection->oindex = -1;
    connection->ioIndex = -1;
    connection->sendContentSize = 0;
    connection->responseIndex = 0;
    connection->responseLeft = 0;
    connection->nextData = NULL;
    memset(connection->response, 0, BUF_SIZE);
    memset(connection->dir, 0, MAX_DIR_LENGTH);
    connection->request = (http_request *)malloc(sizeof(http_request));
    (connection->request)->headers = (liso_hash *)malloc(sizeof(liso_hash));
    init_hash((connection->request)->headers);
    (connection->request)->status = 0;
}

void cleanup_connection(es_connection *connection)
{
    connection->bufindex = 0;
    memset(connection->buf, 0, BUF_SIZE);
    connection->bufAvailable = 1;
    connection->requestOverflow = 0;
    connection->hasRequestHeaders = 0;
    connection->sentResponseHeaders = 0;
    connection->iindex = -1;
    connection->postfinish = 0;
    connection->oindex = -1;
    connection->ioIndex = -1;
    connection->sendContentSize = 0;
    connection->responseIndex = 0;
    connection->responseLeft = 0;
    connection->nextData = NULL;
    memset(connection->response, 0, BUF_SIZE);
    memset(connection->dir, 0, MAX_DIR_LENGTH);
    collapse((connection->request)->headers);
    free((connection->request)->headers);
    free(connection->request);
}

int main(int argc, char* argv[])
{
    int port, s_port;
    char *flog, *flock, *www, *cgi;

    if (argc < 7)
    {
        fprintf(stderr, USAGE, argv[0]);
        exit(0);
    }

    port = atoi(argv[1]);
    s_port = atoi(argv[2]);
    flog = argv[3];
    flock = argv[4];
    www = argv[5];
    cgi = argv[6];

    int sock, client_sock;

    ssize_t readret, writeret;
    socklen_t cli_size;
    struct sockaddr_in addr, cli_addr;
    char buf[BUF_SIZE];

    char request_line[BUF_SIZE];
    char headers[BUF_SIZE];
    char *next_data;

    int offset = 0;

    int selReturn = - 1;

    fd_set rfds;
    fd_set temp_rfds;
    fd_set wfds;
    fd_set temp_wfds;

    int nfds = 0;
    int fdmax;
    int i;
    int writeSize = 0;

    int noDataRead = 0;

    int errnoSave;

    int filefd;

    int iosize;

    int sendSize = 0;

    liso_logger logger;

    liso_logger_create(&logger, flog);

    es_connection connections[MAX_CONNECTIONS];

    // initialize the scokedt functions error handlers
    es_error_handler_setup();
    signal(SIGPIPE, SIG_IGN);

    fprintf(logger.loggerfd, "----- Liso/1.0 Web Server -----\n");

    // all networked programs must create a socket 
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
    {
        errnoSave = errno;
        fprintf(stderr, "Failed creating socket.\n");
        es_socket_error_handle(errnoSave, s_port, logger.loggerfd);
        return EXIT_FAILURE;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(s_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    // servers bind sockets to ports---notify the OS they accept connections
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)))
    {
        errnoSave = errno;
        close_socket(sock);
        fprintf(logger.loggerfd, "Failed binding socket.\n");
        es_bind_error_handle(errnoSave, s_port, logger.loggerfd);
        return EXIT_FAILURE;
    }


    if (listen(sock, 5))
    {
        errnoSave = errno;
        close_socket(sock);
        fprintf(logger.loggerfd, "Error listening on socket.\n");
        es_listen_error_handle(errnoSave, s_port, logger.loggerfd);
        return EXIT_FAILURE;
    }

    // have the file descriptors ready for multiplexing
    FD_ZERO(&rfds);
    FD_SET(sock, &rfds);
    FD_ZERO(&wfds);

    // current total number of file descriptors including stdin, stdout, stderr
    // and server socket
    nfds = 4;

    fdmax = sock;

    // finally, loop waiting for input and then write it back
    while (1)
    {
        // temp fds to maintain state of each of the select function calls
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
            // handle reads, including listening to new connections
            if (FD_ISSET(i, &temp_rfds))
            {
                // saw a change in the server socket, new connection incoming
                if (i == sock)
                {
                    // accept a new connection only if there are fewer file
                    // descriptors than the nax number allowed
                    if (nfds < MAX_CONNECTIONS - 1) 
                    {
                        //printf("Trying to accept a client\n");

                        cli_size = sizeof(cli_addr);
                        client_sock = accept(sock, (struct sockaddr *) &cli_addr, &cli_size);

                        // error occured accepting a client connection
                        if(client_sock < 0)
                        {
                            errnoSave = errno;
                            close(sock);
                            fprintf(logger.loggerfd, "Error accepting connection.\n");
                            es_accept_error_handle(errnoSave, s_port, logger.loggerfd);
                            return EXIT_FAILURE;
                        }

                        // include accepted client to fds so that the server can
                        // listen to client requests
                        FD_SET(client_sock, &rfds);

                        // intialize the acceptec client states
                        memset(buf, 0, BUF_SIZE);
                        init_connection(&(connections[client_sock]));

                        // update the max file descriptor number so that the for
                        // loop can listen to all file descriptors
                        if (client_sock > fdmax)
                        {
                            fdmax = client_sock;
                        }

                        // update the current number of file descriptors for the
                        // new connection
                        nfds++;

                        //printf("Client connection accepted: fd %d\n", client_sock);
                    }
                    else if (nfds == MAX_CONNECTIONS - 1)
                    {   
                        // already reached the max number of connections, do
                        // nothing
                        //printf("Client connect number at MAX. Refused/deferred client connection request.\n");

                        client_sock = accept(sock, (struct sockaddr *) &cli_addr, &cli_size);

                        init_connection(&(connections[client_sock]));

                        (connections[i].request)->status = 503;
                        //build_response(&(connections[i]), connections[i].response);

                        FD_SET(client_sock, &rfds);
                        FD_SET(client_sock, &wfds);

                        if (client_sock > fdmax)
                        {
                            fdmax = client_sock;
                        }

                        nfds++;
                    }
                    else
                    {
                        // proceed without doing anything
                        continue;
                    }
                }
                else
                {
                    noDataRead = 0;

                    readret = 0;
                    writeret = 0;

                    memset(buf, 0, BUF_SIZE);

                    //printf("BUFIDNEXBEFORERECV: %d\n", connections[i].bufindex);
                    readret = recv(i, buf, BUF_SIZE - connections[i].bufindex, MSG_DONTWAIT);
                    //printf("DIFF: %d\n", BUF_SIZE - connections[i].bufindex);
                    //printf("readfuckingret: %d\n", (int)readret);

                    if (connections[i].request->status == 200 || connections[i].request->status == 0) 
                    {
                        if (readret == 0)
                        {
                            //printf("readret = 0\n");
                            FD_CLR(i, &rfds);
                            FD_CLR(i, &wfds);

                            cleanup_connection(&(connections[i]));

                            if (close_socket(i))
                            {
                                close_socket(sock);
                                fprintf(logger.loggerfd, "Error closing client socket.\n");
                                return EXIT_FAILURE;
                            }

                            nfds--;
                            continue;
                        }

                        if (readret == -1) 
                        {
                            //printf("readret = -1\n");
                            errnoSave = errno;

                            if (errnoSave == EAGAIN)
                            {
                                noDataRead = 1;
                            }
                            else if (errnoSave == EINVAL && BUF_SIZE - connections[i].bufindex == 0)
                            {
                                noDataRead = 1;
                            }
                            else if (errnoSave == ECONNRESET)
                            {
                                memset(buf, 0, BUF_SIZE);
                                FD_CLR(i, &rfds);
                                FD_CLR(i, &wfds);
                                //FD_CLR(i, &temp_wfds);
                                cleanup_connection(&(connections[i]));
                                close_socket(i);
                                noDataRead = 1;
                                nfds--;
                                continue;
                            }
                            else
                            {
                                memset(buf, 0, BUF_SIZE);
                                (connections[i].request)->status = 500;
                                noDataRead = 1;
                                //build_response(&(connections[i]), connections[i].response);
                            }

                        }

                        if (noDataRead == 0)
                        {
                            memcpy(connections[i].buf + connections[i].bufindex, buf, readret);
                            connections[i].bufindex += readret;

                            if (connections[i].hasRequestHeaders == 0)
                            {
                                if (connections[i].bufindex == BUF_SIZE)
                                {
                                    //printf("Read filled the buffer.\n");
                                    if ((next_data = strstr(connections[i].buf, "\r\n\r\n")) == NULL)
                                    {
                                        //printf("Request was too long.\n");

                                        connections[i].requestOverflow = 1;
                                        connections[i].request->status = 500;
                                        //build_response(&(connections[i]), connections[i].response);

                                        FD_SET(i, &wfds);
                                    }
                                    else
                                    {
                                        parse_request(connections[i].buf, request_line, headers);
                                        parse_request_line(request_line, connections[i].request);
                                        parse_headers(headers, connections[i].request);
                                        connections[i].hasRequestHeaders = 1;

                                        determine_status(&(connections[i]));

                                        printPairs(connections[i].request->headers);
                                        //printf("%d\n", connections[i].request->status);

                                        //printf((connections[i].request)->method);
                                        //printf("\n");
                                        //printf((connections[i].request)->uri);
                                        //printf("\n");
                                        //printf((connections[i].request)->version);
                                        //printf("\n");
                                    }

                                    if (next_data + 4 == ((connections[i].buf) + BUF_SIZE))
                                    {
                                        memset(connections[i].buf, 0, BUF_SIZE);
                                        connections[i].bufindex = 0;
                                    }
                                    else
                                    {
                                        offset = (next_data + 4) - connections[i].buf;
                                        memset(connections[i].buf, 0, BUF_SIZE);
                                        memcpy(connections[i].buf, buf + offset, BUF_SIZE - offset);
                                        connections[i].bufindex = BUF_SIZE - offset;
                                    }


                                    memset(buf, 0, BUF_SIZE);

                                    if (strcmp(POST, (connections[i].request)->method) == 0)
                                    {
                                        if (connections[i].postfinish == 0)
                                        {
                                            if (connections[i].bufindex < atoi(get_value((connections[i].request)->headers, "content-length")) - connections[i].iindex) 
                                            {
                                                (connections[i].request)->status = 500;
                                                FD_SET(i, &wfds);
                                            }
                                            if ((connections[i].request)->status == 200)
                                            {

                                                writeSize = 0;

                                                if (connections[i].iindex < 0)
                                                {
                                                    connections[i].iindex = 0;
                                                }

                                                if (atoi(get_value((connections[i].request)->headers, "content-length")) - connections[i].iindex >= connections[i].bufindex)
                                                {
                                                    writeSize = connections[i].bufindex;
                                                } 
                                                else
                                                {
                                                    writeSize = atoi(get_value((connections[i].request)->headers, "content-length")) - connections[i].iindex;
                                                }

                                                connections[i].iindex += writeSize;

                                                if (writeSize == BUF_SIZE)
                                                {
                                                    memset(connections[i].buf, 0, BUF_SIZE);
                                                    connections[i].bufindex = 0;
                                                }
                                                else
                                                {
                                                    memcpy(buf, connections[i].buf + writeSize, connections[i].bufindex - writeSize);
                                                    memset(connections[i].buf, 0, BUF_SIZE);
                                                    memcpy(connections[i].buf, buf,  connections[i].bufindex - writeSize);
                                                    memset(buf, 0, BUF_SIZE);
                                                    connections[i].bufindex = connections[i].bufindex - writeSize;
                                                }

                                                if (atoi(get_value((connections[i].request)->headers, "content-length")) ==  connections[i].iindex)
                                                {
                                                    connections[i].postfinish = 1;
                                                    connections[i].iindex = -1;
                                                    FD_SET(i, &wfds);
                                                }
                                            }
                                            else
                                            {
                                                FD_SET(i, &wfds);
                                            }
                                        }
                                    } 
                                    else
                                    {
                                        FD_SET(i, &wfds);
                                    }
                                }
                                else
                                {
                                    //printf("Read didn't fill the buffer.\n");
                                    if ((next_data = strstr(connections[i].buf, "\r\n\r\n")) != NULL)// && (next_data = strstr(connections[i].buf, "\r\n\r\n")) != NULL)
                                    {
                                        parse_request(connections[i].buf, request_line, headers);
                                        parse_request_line(request_line, connections[i].request);
                                        parse_headers(headers, connections[i].request);
                                        connections[i].hasRequestHeaders = 1;

                                        sprintf(connections[i].dir, "%s%s", www, (connections[i].request)->uri);
                                        determine_status(&(connections[i]));

                                        if (strcmp(POST, (connections[i].request)->method) == 0)
                                        {
                                            if ((connections[i].request)->status == 200)
                                            {
                                                if (atoi(get_value((connections[i].request)->headers, "content-length")) != 0)
                                                {
                                                    if (*(connections[i].buf + connections[i].bufindex) == 0)
                                                    {
                                                        //printf("fucking error caught\n");
                                                        (connections[i].request)->status = 500;
                                                        FD_SET(i, &wfds);
                                                    }

                                                    if(atoi(get_value((connections[i].request)->headers, "content-length")) > (long)connections[i].bufindex - (long)(next_data + 4)) 
                                                    {
                                                        //printf("shit error cuaght\n");
                                                        (connections[i].request)->status = 500;
                                                        FD_SET(i, &wfds);
                                                    }
                                                }
                                            }
                                        }

                                        offset = (next_data + 4) - connections[i].buf;
                                        //printf("offfuckingset: %d\n", offset);
                                        memset(connections[i].buf, 0, BUF_SIZE);
                                        memcpy(connections[i].buf, buf + offset, BUF_SIZE - offset);
                                        connections[i].bufindex = connections[i].bufindex - offset;

                                        FD_SET(i, &wfds);
                                    }
                                    else
                                    {
                                        FD_SET(i, &wfds);
                                        (connections[i].request)->status = 400;
                                    }
                                }
                            }
                            else if (connections[i].hasRequestHeaders == 1)
                            {
                                memcpy(connections[i].buf + connections[i].bufindex, buf, readret);
                                connections[i].bufindex += readret;

                                if (strcmp(POST, (connections[i].request)->method) == 0)
                                {
                                    if (connections[i].postfinish == 0)
                                    {

                                        if (connections[i].bufindex < atoi(get_value((connections[i].request)->headers, "content-length")) - connections[i].iindex) 
                                        {
                                            (connections[i].request)->status = 500;
                                            FD_SET(i, &wfds);
                                        }

                                        if((connections[i].request)->status == 200)
                                        {
                                            if (connections[i].iindex < 0)
                                            {
                                                connections[i].iindex = 0;
                                            }

                                            if (atoi(get_value((connections[i].request)->headers, "content-length")) - connections[i].iindex >= connections[i].bufindex)
                                            {
                                                writeSize = connections[i].bufindex;
                                            } 
                                            else
                                            {
                                                writeSize = atoi(get_value((connections[i].request)->headers, "content-length")) - connections[i].iindex;
                                            }

                                            connections[i].iindex += writeSize;

                                            if (writeSize == BUF_SIZE)
                                            {
                                                memset(connections[i].buf, 0, BUF_SIZE);
                                                connections[i].bufindex = 0;
                                            }
                                            else
                                            {
                                                memcpy(buf, (connections[i].buf) + writeSize, connections[i].bufindex - writeSize);
                                                memset(connections[i].buf, 0, BUF_SIZE);
                                                memcpy(connections[i].buf, buf, connections[i].bufindex - writeSize);
                                                memset(buf, 0, BUF_SIZE);
                                                connections[i].bufindex = writeSize;
                                            }

                                            if (atoi(get_value((connections[i].request)->headers, "content-length")) ==  connections[i].iindex)
                                            {
                                                connections[i].postfinish = 1;
                                                connections[i].iindex = -1;
                                                FD_SET(i, &wfds);
                                            }
                                        }
                                        else
                                        {
                                            FD_SET(i, &wfds);
                                        }
                                    }
                                }
                                else
                                {
                                    FD_SET(i, &wfds);
                                }
                            }
                        }
                    }

                    memset(buf, 0, BUF_SIZE);
                    //}
                }
            }

            if (FD_ISSET(i, &temp_wfds))
            {
                //printf("Tring to Write something FUCK\n");
                writeret = 0;

                if ((connections[i].request)->status != 200)// || (connections[i].request)->status)
                {
                    //printf("Sending error message\n");
                    if(connections[i].responseLeft == 0)
                    {
                        build_response(&(connections[i]), connections[i].response);
                        connections[i].responseLeft = strlen(connections[i].response);
                        connections[i].responseIndex = 0;
                    }

                    //printf("RESPONSE: %s", connections[i].response);

                    if (connections[i].responseLeft >= BUF_SIZE)
                    {
                        sendSize = BUF_SIZE;
                    }
                    else
                    {
                        sendSize = connections[i].responseLeft;
                    }

                    writeret = send(i, connections[i].response + connections[i].responseIndex, sendSize, MSG_DONTWAIT);
                    //printf("responseIndex: %d\n", connections[i].responseIndex);
                    //printf("response: %s\n", connections[i].response);
                    //printf("responseLeft: %d\n", connections[i].responseLeft);
                    //printf("i: %d\n", i);
                    //printf("sendSize: %d\n", sendSize);

                    if (writeret < 0)
                    {
                        errnoSave = errno;
                        FD_CLR(i, &rfds);
                        FD_CLR(i, &wfds);

                        //printf("before cleanup\n");
                        //printf("errorstate\n");
                        //printf("HTTP ERROR: %d\n", connections[i].request->status);
                        cleanup_connection(&(connections[i]));
                        //printf("after cleanup\n");
                        close_socket(i);

                        // error is not client disconnection, terminate the server
                        if (es_send_error_handle(errnoSave, s_port, logger.loggerfd) != 1)
                        {
                            close_socket(sock);
                            return EXIT_FAILURE;
                        }

                        // error is client disconnection, keep the server alive and
                        // do cleanup
                        nfds--;

                        continue;
                    }

                    connections[i].responseIndex += writeret;
                    connections[i].responseLeft -= writeret;

                    //printPairs((connections[i].request)->headers);
                    //printf("STATUS: %d\n", connections[i].request->status);
                    //printf("responseIndex: %d\nwriteret: %d\n", connections[i].responseIndex, (int)writeret);
                    //printf("response: %s\n", connections[i].response);
                    if (connections[i].responseLeft == 0)
                    {
                        //printf("before clean fucking up\n");
                        cleanup_connection(&(connections[i]));
                        //printf("after clean fuckin gup\n");
                        FD_CLR(i, &rfds);
                        FD_CLR(i, &wfds);
                        close_socket(i);
                        nfds--;
                    }
                }
                else
                {
                    //printf("Sending 200 response\n");

                    if (strcmp("POST", (connections[i].request)->method) == 0)
                    {
                        if (connections[i].responseLeft == 0)
                        {
                            determine_status(&(connections[i]));
                            build_response(&(connections[i]), connections[i].response);
                            connections[i].responseLeft = strlen(connections[i].response);
                            connections[i].responseIndex = 0;
                        }

                        //printf("RESPONSE: %s\n", connections[i].response);

                        if (connections[i].responseLeft >= BUF_SIZE)
                        {
                            sendSize = BUF_SIZE;
                        }
                        else
                        {
                            sendSize = connections[i].responseLeft;
                        }

                        writeret = send(i, connections[i].response + connections[i].responseIndex, sendSize, MSG_DONTWAIT);
                        //printf("responseIndex: %d\nwriteret: %d\n", connections[i].responseIndex, (int)writeret);
                        //printf("response: %s\n", connections[i].response);
                        //printPairs(connections[i].request->headers);
                        //printf("STATUS: %d\n", connections[i].request->status);
                        //printf("\n");
                        if (writeret < 0)
                        {
                            errnoSave = errno;
                            FD_CLR(i, &rfds);
                            FD_CLR(i, &wfds);

                            //printf("before cleanup\n");
                            cleanup_connection(&(connections[i]));
                            //printf("after cleanup\n");
                            close_socket(i); 

                            // error is not client disconnection, terminate the server
                            if (es_send_error_handle(errnoSave, s_port, logger.loggerfd) != 1)
                            {
                                close_socket(sock);
                                return EXIT_FAILURE;
                            }

                            // error is client disconnection, keep the server alive and
                            // do cleanup
                            nfds--;

                            continue;
                        }

                        connections[i].responseIndex += writeret;
                        connections[i].responseLeft -= writeret;

                        if (connections[i].responseLeft == 0)
                        {
                            //printf("before wtf\n");
                            cleanup_connection(&(connections[i]));
                            //printf("buf: %s\n", connections[i].buf);
                            // printf("after wtf\n");
                            FD_CLR(i, &rfds);
                            FD_CLR(i, &wfds);
                            close_socket(i);
                            nfds--;
                        }
                    }
                    else
                    {
                        sprintf(connections[i].dir, "%s%s", www, (connections[i].request)->uri);

                        filefd = open(connections[i].dir, O_RDONLY);

                        if (filefd < 0)
                        {
                            errnoSave = errno;

                            if (errnoSave == EACCES)
                            {
                                (connections[i].request)->status = 401;
                            }
                            else if (errnoSave == ENOENT)
                            {
                                (connections[i].request)->status = 404;
                            }
                            else
                            {
                                (connections[i].request)->status = 500;
                            }
                        }

                        close(filefd);

                        if (strcmp("HEAD", (connections[i].request)->method) == 0)
                        {
                            if (connections[i].sentResponseHeaders == 0)
                            {
                                if (connections[i].responseIndex == 0)
                                {

                                    determine_status(&(connections[i]));
                                    build_response(&(connections[i]), connections[i].response);
                                    connections[i].responseIndex = strlen(connections[i].response);
                                }

                                //printf("RESPONSE: %s\n", connections[i].response);

                                writeret = send(i, connections[i].response, connections[i].responseIndex, MSG_DONTWAIT);

                                if (writeret < 0)
                                {
                                    errnoSave = errno;
                                    FD_CLR(i, &rfds);
                                    FD_CLR(i, &wfds);

                                    cleanup_connection(&(connections[i]));
                                    close_socket(i);

                                    // error is client disconnection, keep the server alive and
                                    // do cleanup
                                    nfds--;

                                    continue;
                                }

                                connections[i].responseIndex -= writeret;

                                if (connections[i].responseIndex == 0)
                                {
                                    cleanup_connection(&(connections[i]));
                                    FD_CLR(i, &rfds);
                                    FD_CLR(i, &wfds);
                                    close_socket(i);
                                    nfds--;
                                }
                            }
                        }
                        else if (strcmp("GET", (connections[i].request)->method) == 0)
                        {
                            if (connections[i].sentResponseHeaders == 0)
                            {
                                if (connections[i].responseLeft == 0)
                                {

                                    determine_status(&(connections[i]));
                                    build_response(&(connections[i]), connections[i].response);
                                    connections[i].responseLeft = strlen(connections[i].response);
                                    connections[i].responseIndex = 0;
                                }

                                //printPairs(connections[i].request->headers);
                                //printf("%s\n", connections[i].request->uri);
                                //printf("RESPONSE: %s\n", connections[i].response);

                                if (connections[i].responseLeft <= BUF_SIZE)
                                {
                                    sendSize = connections[i].responseLeft;
                                } 
                                else
                                {
                                    sendSize = BUF_SIZE;
                                }

                                //printf("%d\n", connections[i].responseIndex);

                                writeret = send(i, connections[i].response + connections[i].responseIndex, sendSize, MSG_DONTWAIT);

                                connections[i].responseIndex += writeret;
                                connections[i].responseLeft -= writeret;

                                if (connections[i].responseLeft == 0)
                                {
                                    connections[i].sentResponseHeaders = 1;
                                }
                            }
                            else if (connections[i].sentResponseHeaders == 1)
                            {
                                //printf("sent response headers and now trying to send file\n");
                                if ((connections[i].request)->status == 200)
                                {
                                    if (connections[i].ioIndex < 0)
                                    {
                                        connections[i].ioIndex = 0;
                                    }

                                    if (connections[i].sendContentSize >= BUF_SIZE)
                                    {
                                        iosize = BUF_SIZE;
                                    }
                                    else
                                    {
                                        iosize = connections[i].sendContentSize;
                                    }

                                    filefd = open(connections[i].dir, O_RDONLY);

                                    iosize = sendfile(i, filefd, &(connections[i].ioIndex), iosize);

                                    //printf("iosize %d\n", iosize);
                                    if (iosize < 0)
                                    {
                                        errnoSave = errno;
                                        FD_CLR(i, &rfds);
                                        FD_CLR(i, &wfds);

                                        cleanup_connection(&(connections[i]));
                                        close(filefd);
                                        close_socket(i);

                                        // error is not client disconnection, terminate the server

                                        // error is client disconnection, keep the server alive and
                                        // do cleanup
                                        nfds--;

                                        continue;
                                    }

                                    connections[i].sendContentSize -= iosize;
                                    //connections[i].ioIndex += iosize;

                                    close(filefd);
                                    if (connections[i].sendContentSize == 0)
                                    {
                                        cleanup_connection(&(connections[i]));
                                        FD_CLR(i, &rfds);
                                        FD_CLR(i, &wfds);
                                        close_socket(i);
                                        nfds--;
                                    }
                                }
                                else
                                {
                                    cleanup_connection(&(connections[i]));
                                    FD_CLR(i, &rfds);
                                    FD_CLR(i, &wfds);
                                    close_socket(i);
                                    nfds--;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    close_socket(sock);

    liso_logger_close(&logger);

    return EXIT_SUCCESS;
}
