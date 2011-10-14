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
int handle_get_io_ssl(es_connection *connection, int i);
void buffer_shift_forward(es_connection *connection, char *next_data);
void liso_select_cleanup(int i);
void liso_close_and_cleanup(int i);
void liso_get_ready_for_pipeline(es_connection *connection, int i);
void liso_close_if_requested(es_connection *connection, int i);
void SSL_init();

int liso_engine_create(int port, char *flog, char *flock)
{
    struct sockaddr_in addr;
    struct sockaddr_in ssl_addr;

    SSL_init();

    liso_logger_create(&(engine.logger), flog);

    // initialize the scokedt functions error handlers
    es_error_handler_setup();

    // all networked programs must create a socket 
    engine.sock = socket(PF_INET, SOCK_STREAM, 0);
    if (engine.sock == -1)
    {
        SSL_CTX_free(engine.ssl_context);
        fprintf(stderr, "Failed creating socket.\n");
        liso_logger_log(ERROR, "socket", "Failed creating socket.\n", port, engine.logger.loggerfd);
        exit(EXIT_FAILURE);
    }

    engine.ssl_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (engine.ssl_sock == -1)
    {
        SSL_CTX_free(engine.ssl_context);
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
        SSL_CTX_free(engine.ssl_context);
        close_socket(engine.sock);
        close_socket(engine.ssl_sock);
        fprintf(stderr, "Failed binding socket.\n");
        liso_logger_log(ERROR, "bind", "Failed binding socket.\n", port, engine.logger.loggerfd);
        exit(EXIT_FAILURE);
    }

    if (bind(engine.ssl_sock, (struct sockaddr *) &ssl_addr, sizeof(ssl_addr)))
    {
        SSL_CTX_free(engine.ssl_context);
        close_socket(engine.sock);
        close_socket(engine.ssl_sock);
        fprintf(stderr, "Failed binding ssl socket.\n");
        liso_logger_log(ERROR, "bind", "Failed binding ssl socket.\n", ssl_port, engine.logger.loggerfd);
        exit(EXIT_FAILURE);
    }


    if (listen(engine.sock, 5))
    {
        SSL_CTX_free(engine.ssl_context);
        close_socket(engine.sock);
        close_socket(engine.ssl_sock);
        fprintf(stderr, "Error listening on socket.\n");
        liso_logger_log(ERROR, "listen", "Failed listening on socket.\n", port, engine.logger.loggerfd);
        exit(EXIT_FAILURE);
    }

    if (listen(engine.ssl_sock, 5))
    {
        SSL_CTX_free(engine.ssl_context);
        close_socket(engine.sock);
        close_socket(engine.ssl_sock);
        fprintf(stderr, "Error listening on ssl socket.\n");
        liso_logger_log(ERROR, "listen", "Failed listening on ssl socket.\n", ssl_port, engine.logger.loggerfd);
        exit(EXIT_FAILURE);
    }

    // have the file descriptors ready for multiplexing
    FD_ZERO(&(engine.rfds));
    FD_SET(engine.sock, &(engine.rfds));
    FD_SET(engine.ssl_sock, &(engine.rfds));
    FD_ZERO(&(engine.wfds));

    // current total number of file descriptors including stdin, stdout, stderr
    // and server socket
    engine.nfds = 6;

    engine.fdmax = engine.sock >= engine.ssl_sock ? engine.sock : engine.ssl_sock;

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

        //printf("block here?\n");
        selReturn = select(engine.fdmax + 1, &(engine.temp_rfds), &(engine.temp_wfds), NULL, NULL);
        //printf("nope\n");
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
            //printf("i in main loop: %d\n", i);
            memset(engine.buf, 0, BUF_SIZE);

            if (FD_ISSET(i, &(engine.temp_rfds)))
            {
                //printf("read phase\n");

                rwval = liso_handle_recv(i);
                if (rwval == -1)
                {
                    continue;
                }
            }

            if (FD_ISSET(i, &(engine.temp_wfds)))
            {
                //printf("write phase\n");

                rwval = liso_handle_send(i);

                //printf("wrote\n");
                if (rwval == -1)
                {
                    //printf("ssibal\n");
                    continue;
                }
            }
        }
    }

    return 1;
}

int liso_handle_recv(int i)
{
    int returnnum;

    socklen_t cli_size;
    struct sockaddr_in cli_addr;
    int noDataRead, readret, client_sock, errnoSave;
    char *next_data;
    es_connection *currConnection;

    if (i == engine.sock || i == engine.ssl_sock)
    {
        if (engine.nfds < MAX_CONNECTIONS - 1)
        {
            //printf("trying to accept a client.\n");
            cli_size = sizeof(cli_addr);

            if (i == engine.sock)
            {
                client_sock = accept(engine.sock, (struct sockaddr *) &cli_addr, &cli_size);
                //printf("accept happend: %d\n", client_sock);
            }
            else if (i == engine.ssl_sock)
            {
                client_sock = accept(engine.ssl_sock, (struct sockaddr *) &cli_addr, &cli_size);
                //printf("accept happend ssl: %d\n", client_sock);
                //printf("STDIN FD: %d\n", fileno(stdin));
            }

            if (client_sock < 0)
            {
                SSL_CTX_free(engine.ssl_context);
                close(engine.sock);
                close(engine.ssl_sock);
                fprintf(stderr, "Error accepting connection.\n");
                liso_logger_log(ERROR, "accpet", "select returned -1\n", port, engine.logger.loggerfd);
                return -1;
                //return EXIT_FAILURE;
            }

            FD_SET(client_sock, &(engine.rfds));
            //printf("sock: %d\n", client_sock);

            //memset(engine.buf, 0, BUF_SIZE);

            init_connection(&((engine.connections)[client_sock]));

            if (i == engine.ssl_sock) 
            {
                //printf("Trying to accept SSL client\n");
                
                currConnection = &((engine.connections)[client_sock]);

                currConnection->context = SSL_new(engine.ssl_context);

                if (currConnection->context != NULL)
                {
                    if (SSL_set_fd(currConnection->context, client_sock) == 0)
                    {
                        //printf("ssibal\n");
                        close_socket(engine.sock);
                        close_socket(engine.ssl_sock);
                        SSL_free(currConnection->context);
                        SSL_CTX_free(engine.ssl_context);
                        fprintf(stderr, "Error creating client SSL context.\n");
                        liso_logger_log(ERROR, "SSL_set_fd", "Error creating client SSL context.\n", ssl_port, engine.logger.loggerfd);
                        exit(EXIT_FAILURE);
                    }

                    if ((returnnum = SSL_accept(currConnection->context)) <= 0)
                    {
                        //printf("dammit: %d\n", returnnum);
                        //printf("sslerror: %d\n", SSL_get_error(currConnection->context, returnnum));
                        close_socket(engine.sock);
                        close_socket(engine.ssl_sock);
                        SSL_free(currConnection->context);
                        SSL_CTX_free(engine.ssl_context);
                        fprintf(stderr, "Error creating client SSL context.\n");
                        liso_logger_log(ERROR, "SSL_set_fd", "Error accepting (handshake) client SSL context.\n", ssl_port, engine.logger.loggerfd);
                        exit(EXIT_FAILURE);
                    }
                }
                
                //printf("SSL client accepted: %d\n", client_sock);
            }

            if (client_sock > engine.fdmax)
            {
                engine.fdmax = client_sock;
            }

            (engine.nfds)++;
            //printf("accpeted client: %d\n", client_sock);
        }
        else if (engine.nfds == MAX_CONNECTIONS -1)
        {
            client_sock = accept(engine.sock, (struct sockaddr *) &cli_addr, &cli_size);

            if (client_sock < 0)
            {
                close(engine.sock);
                fprintf(stderr, "Error accepting connection.\n");
                liso_logger_log(ERROR, "accept", "select returned -1\n", port, engine.logger.loggerfd);
                return -1;
                //return EXIT_FAILURE;
            }
            init_connection(&((engine.connections)[client_sock]));

            (engine.connections)[i].request->status = 503;

            if (i == engine.ssl_sock) 
            {
                currConnection = &((engine.connections)[client_sock]);

                currConnection->context = SSL_new(engine.ssl_context);

                if (currConnection->context != NULL)
                {
                    if (SSL_set_fd(currConnection->context, client_sock) == 0)
                    {
                        //printf("ssibal\n");
                        close_socket(engine.sock);
                        close_socket(engine.ssl_sock);
                        SSL_free(currConnection->context);
                        SSL_CTX_free(engine.ssl_context);
                        fprintf(stderr, "Error creating client SSL context.\n");
                        liso_logger_log(ERROR, "SSL_set_fd", "Error creating client SSL context.\n", ssl_port, engine.logger.loggerfd);
                        exit(EXIT_FAILURE);
                    }

                    if (SSL_accept(currConnection->context) <= 0)
                    {
                        //printf("dammit\n");
                        close_socket(engine.sock);
                        close_socket(engine.ssl_sock);
                        SSL_free(currConnection->context);
                        SSL_CTX_free(engine.ssl_context);
                        fprintf(stderr, "Error creating client SSL context.\n");
                        liso_logger_log(ERROR, "SSL_set_fd", "Error accepting (handshake) client SSL context.\n", ssl_port, engine.logger.loggerfd);
                        exit(EXIT_FAILURE);
                    }
                }
            }

            FD_SET(client_sock, &(engine.rfds));
            FD_SET(client_sock, &(engine.wfds));
            //printf("sock2: %d\n", client_sock);

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
        //printf("entering\n");
        noDataRead = 0;
        readret = 0;

        memset(engine.buf, 0, BUF_SIZE);

        currConnection = &((engine.connections)[i]);

        if (currConnection->context != NULL)
        {
            readret = SSL_read(currConnection->context, engine.buf, BUF_SIZE - currConnection->bufindex);
        }
        else
        {
            //printf("why is it blocking\n");
            //printf("oh no: %d\n", BUF_SIZE - currConnection->bufindex);
            readret = recv(i, engine.buf, BUF_SIZE - currConnection->bufindex, 0);
            //printf("crap\n");
        }

        if (currConnection->request->status == 200 || currConnection->request->status == 0)
        {
            //printf("what is going on\n");
            if (currConnection->context == NULL)
            {
                if (readret == 0)
                {
                    //printf("readret returned 0\n");
                    //printf("client disconnected\n");
                    liso_close_and_cleanup(i);
                    //liso_select_cleanup(i);
                    //close_socket(i);
                    //(engine.nfds)--;
                    return -1;
                }

                if (readret == -1)
                {
                    //printf("readret returned -1\n");
                    errnoSave = errno;

                    if (errnoSave == EINVAL && BUF_SIZE - currConnection->bufindex == 0)
                    {
                        //printf("no data is read\n");
                        noDataRead = 1;
                    }
                    else if (errnoSave == ECONNRESET)
                    {
                        memset(engine.buf, 0, BUF_SIZE);
                        //noDataRead = 1;
                        liso_close_and_cleanup(i);
                        //liso_select_cleanup(i);
                        //close_socket(i);
                        //(engine.nfds)--;
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
            }
            else
            {
                if (readret == 0)
                {
                    if (SSL_get_error(currConnection->context, readret) == SSL_ERROR_WANT_READ)
                    {
                        noDataRead = 1;
                    }
                    else 
                    {
                        //printf("ERROR NUM: %d %d\n", SSL_get_error(currConnection->context, readret), SSL_ERROR_ZERO_RETURN);
                        //printf("CLIENT DISCONNECTED\n");
                        liso_close_and_cleanup(i);
                        return -1;
                    }
                    //return -1;
                }

                if (readret < 0)
                {
                    //printf("ssibal inside ssl portion\n");
                    if (SSL_get_error(currConnection->context, readret) == SSL_ERROR_ZERO_RETURN)
                    {
                        liso_close_and_cleanup(i);
                        return -1;
                    } 
                    else if (SSL_get_error(currConnection->context, readret) == SSL_ERROR_WANT_READ)
                    {
                        // check this for correctness
                        return -1;
                    }
                    else
                    {
                        //printf("HAHAH\n");
                        liso_close_and_cleanup(i);
                        return -1;
                    }
                }
            }

            //printf("bufidex of currConnection: %d\n", currConnection->bufindex);

            if (noDataRead == 0)// || currConnection->bufindex != 0)
            {
                //printf("data is read\n");
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

                        if (strstr(currConnection->request->uri, "/cgi/")) // check with wolf if this is correct
                        {
                            cgi_init(currConnection);
                        }
                    }
                    else
                    {
                        //printf("----------------no-----------------\n");
                        currConnection->request->status = 400;
                        FD_SET(i, &(engine.wfds));
                    }

                    if (next_data != NULL)
                    {
                        buffer_shift_forward(currConnection, next_data);
                    }

                    //if (currConnection->request->status == 200 && (strcmp(HEAD, currConnection->request->method) == 0 || strcmp(GET, currConnection->request->method) == 0))
                    //{
                    //    FD_SET(i, &(engine.wfds));
                    //}
                    //else 
                    if (currConnection->request->status != 200 && currConnection->request->status != 0)
                    {
                        FD_SET(i, &(engine.wfds));
                    }

                    //printf("eh...\n");

                    //if (strcmp(POST, currConnection->request->method) == 0)
                    //{
                        post_recv_phase(currConnection, i);
                    //}
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
    char *conn;
    char *version;
    //char buf[BUF_SIZE];
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
    else if ((currConnection->stdout_pipe)[0] >= 0)
    {
        retval = cgi_send_response(currConnection, i);

        if (retval == -1)
        {
            // need to handle this portion
            printf("CGI MESSED UP\n");
            exit(EXIT_FAILURE);
        }

        if (retval == 0)
        {
            get_ready_for_pipeline(currConnection);
            FD_CLR(i, &(engine.wfds));
        }
    }
    else
    {
        if (strcmp(POST, currConnection->request->method) == 0)
        {
            retval = send_response(currConnection, i);

            //FD_SET(i, &(engine.rfds));
            //printf("sock3: %d\n", i);

            //printPairs(currConnection->request->headers);
            //printf("Response: %s\n", currConnection->response);

            if (retval != -1)
            {
                if (currConnection->responseLeft == 0)
                {
                    //cleanup_connection(currConnection);
                    //FD_CLR(i, &(engine.rfds));
                    //FD_CLR(i, &(engine.wfds));
                    //close_socket(i);
                    //(engine.nfds)--;

                    conn = get_value(currConnection->request->headers, "connection");
                    version = currConnection->request->version;

                    if (strcmp("HTTP/1.0", version) == 0)
                    {
                        liso_close_and_cleanup(i);
                    }
                    else if (strcmp(conn, "close") == 0)
                    {
                        liso_close_and_cleanup(i);
                    }
                    else {
                        get_ready_for_pipeline(currConnection);
                        FD_CLR(i, &(engine.wfds));
                    }
                }
            }
        }
        else if(strcmp(HEAD, currConnection->request->method) == 0)
        {
            retval = send_response(currConnection, i);

            //FD_SET(i, &(engine.rfds));

            //printf("sock4: %d\n", i);

            if (retval != -1)
            {
                if (currConnection->responseLeft == 0)
                {
                    //cleanup_connection(currConnection);
                    //FD_CLR(i, &(engine.rfds));
                    //FD_CLR(i, &(engine.wfds));
                    //close_socket(i);
                    //(engine.nfds)--;

                    conn = get_value(currConnection->request->headers, "connection");
                    version = currConnection->request->version;

                    if (strcmp("HTTP/1.0", version) == 0)
                    {
                        liso_close_and_cleanup(i);
                    }
                    else if (strcmp(conn, "close") == 0)
                    {
                        liso_close_and_cleanup(i);
                    }
                    else {
                        get_ready_for_pipeline(currConnection);
                        FD_CLR(i, &(engine.wfds));
                    }
                }
            }
        }
        else if(strcmp(GET, currConnection->request->method) == 0)
        {
            if ((sentResponse = currConnection->sentResponseHeaders) == 0)
            {
                retval = send_response(currConnection, i);
                //FD_SET(i, &(engine.rfds));

            //printf("sock5: %d\n", i);
                //printf("response probably sent: %d\n", i);
            }
            else if (sentResponse == 1)
            {
                //printf("about to send body: %d\n", i);
                if (currConnection->request->status == 200)
                {
                    retval = handle_get_io(currConnection, i);
                    //printf("sent response\n");
                    //FD_SET(i, &(engine.rfds));

            //printf("sock6: %d\n", i);

                    if (retval != -1)
                    {
                        if (currConnection->sendContentSize == 0)
                        {
                            //printf("send body complete\n");
                            fflush(stdout);
                            conn = get_value(currConnection->request->headers, "connection");
                            version = currConnection->request->version;

                            if (strcmp("HTTP/1.0", version) == 0)
                            {
                                liso_close_and_cleanup(i);
                            }
                            else if (conn != NULL && strcmp(conn, "close") == 0)
                            {
                                liso_close_and_cleanup(i);
                                //printf("shit\n");
                            }
                            else {
                                //printf("frig\n");
                                get_ready_for_pipeline(currConnection);
                                FD_CLR(i, &(engine.wfds));
                            }
                        }
                    }
                }
                else
                {
                    //printf("STATUS WAS NOT 200\n");
                    cleanup_connection(currConnection);
                    FD_CLR(i, &(engine.rfds));
                    FD_CLR(i, &(engine.wfds));
                    close_socket(i);
                    (engine.nfds)--;
                }
            }
        }    
    }

    //printf("FUCCCCCCCCK\n");

    return retval;
}

void post_recv_phase(es_connection *connection, int i)
{
    int writeSize;
    int content_length;
    char *content_length_s;

    if (connection->postfinish == 0)
    {
        if (connection->request->status == 200)
        {
            if (connection->iindex < 0)
            {
                connection->iindex = 0;
            }

            content_length_s = get_value(connection->request->headers, "content-length");

            if (content_length_s == NULL)
            {
                content_length = 0;
            }
            else 
            {
                content_length = atoi(get_value(connection->request->headers, "content-length"));
            }

            if (content_length - connection->iindex >= connection->bufindex)
            {
                writeSize = connection->bufindex;
            }
            else
            {
                writeSize = content_length;
            }

            if ((connection->stdout_pipe)[0] >= 0)
            {
                writeSize = cgi_write(connection, writeSize);
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

    //printf("response: %s\n", connection->response);
    //printPairs(connection->request->headers);
    //printf("path: %s\n", connection->dir);
    //printf("rem: %s\n", connection->buf);

    if (connection->responseLeft >= BUF_SIZE)
    {
        sendSize = BUF_SIZE;
    }
    else
    {
        sendSize = connection->responseLeft;
    }

    if (connection->context == NULL)
    {
        //printf("before\n");
        writeret = send(i, connection->response + connection->responseIndex, sendSize, MSG_NOSIGNAL);
        //printf("after\n");
    }
    else 
    {
        writeret = SSL_write(connection->context, connection->response + connection->responseIndex, sendSize);
    }

    if (writeret < 0)
    {
        //printf("IO WRITE error\n");
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
    FILE *fp;
    int filefd;
    int iosize;
    int errnoSave;

    //printf("ssl send file\n");

    if (connection->ioIndex < 0)
    {
        connection->ioIndex = 0;
    }

    if (connection->sslioIndex < 0)
    {
        connection->sslioIndex = 0;
    }

    if (connection->sendContentSize >= BUF_SIZE)
    {
        iosize = BUF_SIZE;
    }
    else
    {
        iosize = connection->sendContentSize;
    }

    if (connection->context == NULL)
    {
        filefd = open(connection->dir, O_RDONLY);
        iosize = sendfile(i, filefd, &(connection->ioIndex), iosize);
        close(filefd);
    }
    else if(connection->context != NULL)
    {
        fp = fopen(connection->dir, "r");
        iosize = ssl_send_file(connection->context, fp, &(connection->sslioIndex), iosize);
        //printf("FILE: %d\n", fileno(fp));
        fclose(fp);
    }

    //printf("ioindex: %d\n", connection->sslioIndex);

    //printf("IOSIZE: %d\n", iosize);
    //printf("CONTENT LEFT: %d\n", connection->sendContentSize);

    //close(filefd);

    if (iosize < 0)
    {

        //printf("IoERROR\n");
        errnoSave = errno;

        FD_CLR(i, &(engine.rfds));
        FD_CLR(i, &(engine.wfds));

        cleanup_connection(connection);
        close_socket(i);

        (engine.nfds)--;

        return -1;
    }

    connection->sendContentSize -= iosize;

    //if (connection->sendContentSize == 0)
    //{
    //    FD_CLR(i, &(engine.rfds));
    //    FD_CLR(i, &(engine.wfds));
    //    cleanup_connection(connection);
    //    close_socket(i);
    //    (engine.nfds)--;
    //}

    return 1;
}


int handle_get_io_ssl(es_connection *connection, int i);

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

    //printf(connection->buf);
}

void liso_select_cleanup(int i)
{
    FD_CLR(i, &(engine.rfds));
    FD_CLR(i, &(engine.wfds));

    cleanup_connection(&((engine.connections)[i]));
}

void liso_close_and_cleanup(int i)
{
    liso_select_cleanup(i);
    close_socket(i);
    (engine.nfds)--;
}

void liso_close_if_requested(es_connection *connection, int i)
{
    char *conn = get_value(connection->request->headers, "connection");

    if (strcmp(conn, "close") == 0)
    {
        liso_close_and_cleanup(i);
    }
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

void SSL_init()
{
    SSL_load_error_strings();
    SSL_library_init();

    if ((engine.ssl_context = SSL_CTX_new(TLSv1_server_method())) == NULL)
    {
        liso_logger_log(ERROR, "SSL_init", "Error creating SSL context.\n", port, engine.logger.loggerfd);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(engine.ssl_context, "../private/hongjaic2.key", SSL_FILETYPE_PEM) == 0)
    {
        SSL_CTX_free(engine.ssl_context);
        liso_logger_log(ERROR, "SSL_init", "Error associating private key.\n", port, engine.logger.loggerfd);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_certificate_file(engine.ssl_context, "../certs/hongjaic2.crt", SSL_FILETYPE_PEM) == 0)
    {
        SSL_CTX_free(engine.ssl_context);
        liso_logger_log(ERROR, "SSL_init", "Error associating certification.\n", port, engine.logger.loggerfd);
        exit(EXIT_FAILURE);
    }
}
