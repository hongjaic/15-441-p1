/**
 * CS 15-441 Computer Networks
 *
 * @file    liso_select_engine.c
 * @author  Hong Jai Cho (hongjaic@andrew.cmu.edu)
 */

#include "liso_select_engine.h"

int liso_handle_recv(int i);
int liso_handle_send(int i);
void post_recv_phase(es_connection *connection, int i);
int send_response(es_connection *connection, int i);
int handle_get_io(es_connection *connection, int i);
void buffer_shift_forward(es_connection *connection, char *next_data);
void liso_select_cleanup(int i);

int liso_engine_create(int port, char *flog, char *flock)
{
    struct sockaddr_in addr;
    struct sockaddr_in ssl_addr;

    liso_logger_create(&(engine.logger), flog);

    // initialize the scokedt functions error handlers
    es_error_handler_setup();

    // all networked programs must create a socket 
    engine.sock = socket(PF_INET, SOCK_STREAM, 0);
    if (engine.sock == -1)
    {
        fprintf(stderr, "Failed creating socket.\n");
        liso_logger_log(ERROR, "socket", "Failed creating socket.\n", port, engine.logger.loggerfd);
        exit(EXIT_FAILURE);
    }

    engine.sock_ssl = socket(PF_INET, SOCK_STREAM, 0);
    if (engine.sock_ssl == -1)
    {
        fprintf(stderr, "Failed create ssl socket.\n");
        liso_logger_log(ERROR, "socket", "Failed creating ssl socket.\n", ssl_port, engine.logger.loggerfd);
        exit(EXIT_FAILURE);
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    ssl_addr.sin_family = AF_INET;
    ssl_addr.sin_port = htons(ssl_port);
    ssl_addr.sin_addr.s_addr = INADDR_ANY;

    // servers bind sockets to ports---notify the OS they accept connections
    if (bind(engine.sock, (struct sockaddr *) &addr, sizeof(addr)))
    {
        close_socket(engine.sock);
        fprintf(stderr, "Failed binding socket.\n");
        liso_logger_log(ERROR, "bind", "select returned -1\n", port, engine.logger.loggerfd);
        exit(EXIT_FAILURE);
    }


    if (listen(engine.sock, 5))
    {
        close_socket(engine.sock);
        fprintf(stderr, "Error listening on socket.\n");
        liso_logger_log(ERROR, "listen", "select returned -1\n", port, engine.logger.loggerfd);
        exit(EXIT_FAILURE);
    }

    // have the file descriptors ready for multiplexing
    FD_ZERO(&(engine.rfds));
    FD_SET(engine.sock, &(engine.rfds));
    FD_ZERO(&(engine.wfds));

    // current total number of file descriptors including stdin, stdout, stderr
    // and server socket
    engine.nfds = 4;

    engine.fdmax = engine.sock;

    return 1; 
}

int liso_engine_event_loop()
{
    int i;
    int selReturn = -1;
    int rwval;

    while (1)
    {
        //printf("NFDS: %d\n", engine.nfds);

        engine.temp_rfds = engine.rfds;
        engine.temp_wfds = engine.wfds;

        selReturn = select(engine.fdmax + 1, &(engine.temp_rfds), &(engine.temp_wfds), NULL, NULL);

        if (selReturn < 0)
        {
            close_socket(engine.sock);
            perror("ERROR: select returned -1\n");
            liso_logger_log(ERROR, "select", "select returned -1\n", port, engine.logger.loggerfd);
            exit(4);
        }

        if (selReturn == 0)
        {
            continue;
        }

        for (i = 0; i < engine.fdmax + 1; i++)
        {
            if (FD_ISSET(i, &(engine.temp_rfds)))
            {

                rwval = liso_handle_recv(i);
                if (rwval == -1)
                {
                    continue;
                }
            }

            if (FD_ISSET(i, &(engine.temp_wfds)))
            {
                rwval = liso_handle_send(i);

                if (rwval == -1)
                {
                    continue;
                }
            }

            memset(engine.buf, 0, BUF_SIZE);
        }
    }

    return 1;
}

int liso_handle_recv(int i)
{
    socklen_t cli_size;
    struct sockaddr_in cli_addr;
    int noDataRead, readret, client_sock, errnoSave;
    char *next_data;
    es_connection *currConnection;

    if (i == engine.sock)
    {
        if (engine.nfds < MAX_CONNECTIONS - 1)
        {
            cli_size = sizeof(cli_addr);
            client_sock = accept(engine.sock, (struct sockaddr *) &cli_addr, &cli_size);

            if (client_sock < 0)
            {
                close(engine.sock);
                fprintf(stderr, "Error accepting connection.\n");
                liso_logger_log(ERROR, "accpet", "select returned -1\n", port, engine.logger.loggerfd);
                return -1;
                //return EXIT_FAILURE;
            }

            FD_SET(client_sock, &(engine.rfds));

            //memset(engine.buf, 0, BUF_SIZE);

            init_connection(&((engine.connections)[client_sock]));

            if (client_sock > engine.fdmax)
            {
                engine.fdmax = client_sock;
            }

            (engine.nfds)++;
        }
        else if (engine.nfds == MAX_CONNECTIONS -1)
        {
            client_sock = accept(engine.sock, (struct sockaddr *) &cli_addr, &cli_size);

            init_connection(&((engine.connections)[client_sock]));

            if (client_sock < 0)
            {
                close(engine.sock);
                fprintf(stderr, "Error accepting connection.\n");
                liso_logger_log(ERROR, "accept", "select returned -1\n", port, engine.logger.loggerfd);
                return -1;
                //return EXIT_FAILURE;
            }

            (engine.connections)[i].request->status = 503;

            FD_SET(client_sock, &(engine.rfds));
            FD_SET(client_sock, &(engine.wfds));

            if (client_sock > engine.fdmax)
            {
                engine.fdmax = client_sock;
            }

            (engine.nfds)++;
        }
        else
        {
            return -1;
        }
    }
    else
    {
        noDataRead = 0;
        readret = 0;

        memset(engine.buf, 0, BUF_SIZE);

        currConnection = &((engine.connections)[i]);

        readret = recv(i, engine.buf, BUF_SIZE - currConnection->bufindex, MSG_DONTWAIT);

        if (currConnection->request->status == 200 || currConnection->request->status == 0)
        {
            if (readret == 0)
            {
                liso_select_cleanup(i);
                return -1;
            }

            if (readret == -1)
            {
                errnoSave = errno;

                if (errnoSave == EINVAL && BUF_SIZE - currConnection->bufindex == 0)
                {
                    noDataRead = 1;
                }
                else if (errnoSave == ECONNRESET)
                {
                    memset(engine.buf, 0, BUF_SIZE);
                    //noDataRead = 1;
                    liso_select_cleanup(i);
                    return -1; 
                }
                else
                {
                    memset(engine.buf, 0, BUF_SIZE);
                    currConnection->request->status = 500;
                    noDataRead = 1;
                    FD_SET(i, &(engine.wfds));
                }
            }

            if (noDataRead == 0)
            {
                memcpy(currConnection->buf + currConnection->bufindex, engine.buf, readret);
                currConnection->bufindex += readret;

                if (currConnection->hasRequestHeaders == 0)
                {
                    if (currConnection->bufindex == BUF_SIZE)
                    {
                        if ((next_data = strstr(currConnection->buf, "\r\n\r\n")) == NULL)
                        {
                            currConnection->request->status = 500;
                            FD_SET(i, &(engine.wfds));
                        }
                    }

                    if ((next_data = strstr(currConnection->buf, "\r\n\r\n")) != NULL)
                    {
                        parse_http(currConnection);
                        currConnection->hasRequestHeaders = 1;
                        determine_status(currConnection);
                    }
                    else
                    {
                        currConnection->request->status = 400;
                        FD_SET(i, &(engine.wfds));
                    }

                    if (next_data != NULL)
                    {
                        buffer_shift_forward(currConnection, next_data);
                    }

                    if (currConnection->request->status == 200 && (strcmp(HEAD, currConnection->request->method) == 0 || strcmp(GET, currConnection->request->method) == 0))
                    {
                        FD_SET(i, &(engine.wfds));
                    }
                    else if (currConnection->request->status != 200 && currConnection->request->status != 0)
                    {
                        FD_SET(i, &(engine.wfds));
                    }

                    if (strcmp(POST, currConnection->request->method) == 0)
                    {
                        post_recv_phase(currConnection, i);
                    }
                }
                else if (currConnection->hasRequestHeaders == 1)
                {
                    if (strcmp(POST, currConnection->request->method) == 0)
                    {
                        post_recv_phase(currConnection, i);
                    }
                    else
                    {
                        FD_SET(i, &(engine.wfds));
                    }
                }
            }
            memset(engine.buf, 0, BUF_SIZE);
        }
    }

    return 1;
}

int liso_handle_send(int i)
{
    int retval;
    int sentResponse;
    es_connection *currConnection = (&(engine.connections)[i]);

    if (currConnection->request->status != 200)
    {
        retval =  send_response(currConnection, i);

        if (currConnection->responseLeft == 0)
        {
            cleanup_connection(currConnection);
            FD_CLR(i, &(engine.rfds));
            FD_CLR(i, &(engine.wfds));
            close_socket(i);
            (engine.nfds)--;
        }
    }
    else
    {
        if (strcmp(POST, currConnection->request->method) == 0)
        {
            retval = send_response(currConnection, i);

            //printPairs(currConnection->request->headers);
            //printf("Response: %s\n", currConnection->response);
            //printf("ssibaloma\n");

            if (retval != -1)
            {
                if (currConnection->responseLeft == 0)
                {
                    printf("Indiciation that response was sent.\n");
                   //sleep(10);
                    cleanup_connection(currConnection);
                    FD_CLR(i, &(engine.rfds));
                    FD_CLR(i, &(engine.wfds));
                    close_socket(i);
                    (engine.nfds)--;
                }
            }
        }
        else if(strcmp(HEAD, currConnection->request->method) == 0)
        {
            retval = send_response(currConnection, i);

            if (retval != -1)
            {
                if (currConnection->responseLeft == 0)
                {
                    cleanup_connection(currConnection);
                    FD_CLR(i, &(engine.rfds));
                    FD_CLR(i, &(engine.wfds));
                    close_socket(i);
                    (engine.nfds)--;
                }
            }
        }
        else if(strcmp(GET, currConnection->request->method) == 0)
        {
            if ((sentResponse = currConnection->sentResponseHeaders) == 0)
            {
                retval = send_response(currConnection, i);
            }
            else if (sentResponse == 1)
            {
                if (currConnection->request->status == 200)
                {
                    retval = handle_get_io(currConnection, i);
                }
                else
                {
                    cleanup_connection(currConnection);
                    FD_CLR(i, &(engine.rfds));
                    FD_CLR(i, &(engine.wfds));
                    close_socket(i);
                    (engine.nfds)--;
                }
            }
        }    
    }

    return retval;
}

void post_recv_phase(es_connection *connection, int i)
{
    int writeSize;
    int content_length;

    if (connection->postfinish == 0)
    {
        if (connection->request->status == 200)
        {
            if (connection->iindex < 0)
            {
                connection->iindex = 0;
            }

            content_length = atoi(get_value(connection->request->headers, "content-length"));

            if (content_length - connection->iindex >= connection->bufindex)
            {
                writeSize = connection->bufindex;
            }
            else
            {
                writeSize = content_length;
            }

            connection->iindex += writeSize;

            if (writeSize == connection->bufindex)
            {
                memset(connection->buf, 0, connection->bufindex);
                connection->bufindex = 0;
            }
            else
            {
                memcpy(engine.buf, connection->buf + writeSize, connection->bufindex - writeSize);
                memset(connection->buf, 0, BUF_SIZE);
                memcpy(connection->buf, engine.buf, connection->bufindex - writeSize);
                connection->bufindex = connection->bufindex - writeSize;
                memset(engine.buf, 0, BUF_SIZE);
            }

            if (content_length == connection->iindex)
            {
                connection->postfinish = 1;
                connection->iindex = -1;
                FD_SET(i, &(engine.wfds));
            }
        }
        else
        {
            FD_SET(i, &(engine.wfds));
        }
    }
}

int send_response(es_connection *connection, int i)
{
    int sendSize;
    int writeret;
    int errnoSave;

    if (connection->responseLeft == 0)
    {
        determine_status(connection);
        build_response(connection, connection->response);
        connection->responseLeft = strlen(connection->response);
        connection->responseIndex = 0;
    }

    if (connection->responseLeft >= BUF_SIZE)
    {
        sendSize = BUF_SIZE;
    }
    else
    {
        sendSize = connection->responseLeft;
    }

    writeret = send(i, connection->response + connection->responseIndex, sendSize, MSG_NOSIGNAL);

    if (writeret < 0)
    {
        printf("oh my god what the fuck\n");
        errnoSave = errno;

        FD_CLR(i, &(engine.rfds));
        FD_CLR(i, &(engine.wfds));

        cleanup_connection(connection);
        close_socket(i);

        if (errnoSave != ECONNRESET && errnoSave != EPIPE)
        {
            close_socket(engine.sock);
            exit(EXIT_FAILURE);
            //return EXIT_FAILURE;
        }

        (engine.nfds)--;

        return -1;
    }

    connection->responseIndex += writeret;
    connection->responseLeft -= writeret;

    if (connection->responseLeft == 0)
    {
        connection->sentResponseHeaders = 1;
    }

    //if (connection->responseLeft == 0)
    //{
    //    cleanup_connection(connection);
    //    FD_CLR(i, &(engine.rfds));
    //    FD_CLR(i, &(engine.wfds));
    //    close_socket(i);
    //    (engine.nfds)--;
    //}

    return 1;
}

int handle_get_io(es_connection *connection, int i)
{
    int filefd;
    int iosize;
    int errnoSave;

    if (connection->ioIndex < 0)
    {
        connection->ioIndex = 0;
    }

    if (connection->sendContentSize >= BUF_SIZE)
    {
        iosize = BUF_SIZE;
    }
    else
    {
        iosize = connection->sendContentSize;
    }

    filefd = open(connection->dir, O_RDONLY);

    iosize = sendfile(i, filefd, &(connection->ioIndex), iosize);

    close(filefd);

    if (iosize < 0)
    {
        errnoSave = errno;

        FD_CLR(i, &(engine.rfds));
        FD_CLR(i, &(engine.wfds));

        cleanup_connection(connection);
        close_socket(i);

        (engine.nfds)--;

        return -1;
    }


    connection->sendContentSize -= iosize;

    if (connection->sendContentSize == 0)
    {
        FD_CLR(i, &(engine.rfds));
        FD_CLR(i, &(engine.wfds));
        cleanup_connection(connection);
        close_socket(i);
        (engine.nfds)--;
    }

    return 1;
}

void buffer_shift_forward(es_connection *connection, char *next_data)
{
    int offset;

    if (next_data + 4 == connection->buf + BUF_SIZE)
    {
        memset(connection->buf, 0, BUF_SIZE);
        connection->bufindex = 0;
    }
    else
    {
        offset = (next_data + 4) - connection->buf;
        memset(connection->buf, 0, BUF_SIZE);
        memcpy(connection->buf, engine.buf + offset, connection->bufindex - offset); // verify that this is correct
        connection->bufindex = connection->bufindex - offset; // check if this is actually correct
    }
}

void liso_select_cleanup(int i)
{
    FD_CLR(i, &(engine.rfds));
    FD_CLR(i, &(engine.wfds));

    cleanup_connection(&((engine.connections)[i]));

    close_socket(i);

    (engine.nfds)--;
}

int close_socket(int sock)
{
    if (close(sock))
    {
        fprintf(stderr, "Failed closing socket.\n");
        liso_logger_log(ERROR, "close_socket", "select returned -1\n", port, engine.logger.loggerfd);
        return 1;
    }
    return 0;
}
