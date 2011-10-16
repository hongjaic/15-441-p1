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
void look_buffer_for_request(es_connection *connection, int i);
void process_buffer(es_connection *connection, int i);
void non_poisonouse_finish(es_connection *connection, int i);
void SSL_init();
int SSL_wrap(es_connection *connection, int sock);

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
        liso_logger_log(ERROR, "socket", "Failed creating socket.\n", port, engine.logger.loggerfd);
        liso_logger_close(&(engine.logger));
        exit(EXIT_FAILURE);
    }

    engine.ssl_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (engine.ssl_sock == -1)
    {
        SSL_CTX_free(engine.ssl_context);
        liso_logger_log(ERROR, "socket", "Failed creating ssl socket.\n", ssl_port, engine.logger.loggerfd);
        liso_logger_close(&(engine.logger));
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
        liso_logger_log(ERROR, "bind", "Failed binding socket.\n", port, engine.logger.loggerfd);
        liso_logger_close(&(engine.logger));
        exit(EXIT_FAILURE);
    }

    if (bind(engine.ssl_sock, (struct sockaddr *) &ssl_addr, sizeof(ssl_addr)))
    {
        SSL_CTX_free(engine.ssl_context);
        close_socket(engine.sock);
        close_socket(engine.ssl_sock);
        liso_logger_log(ERROR, "bind", "Failed binding ssl socket.\n", ssl_port, engine.logger.loggerfd);
        liso_logger_close(&(engine.logger));
        exit(EXIT_FAILURE);
    }


    if (listen(engine.sock, 5))
    {
        SSL_CTX_free(engine.ssl_context);
        close_socket(engine.sock);
        close_socket(engine.ssl_sock);
        liso_logger_log(ERROR, "listen", "Failed listening on socket.\n", port, engine.logger.loggerfd);
        liso_logger_close(&(engine.logger));
        exit(EXIT_FAILURE);
    }

    if (listen(engine.ssl_sock, 5))
    {
        SSL_CTX_free(engine.ssl_context);
        close_socket(engine.sock);
        close_socket(engine.ssl_sock);
        liso_logger_log(ERROR, "listen", "Failed listening on ssl socket.\n", ssl_port, engine.logger.loggerfd);
        liso_logger_close(&(engine.logger));
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

        engine.temp_rfds = engine.rfds;
        engine.temp_wfds = engine.wfds;

        selReturn = select(engine.fdmax + 1, &(engine.temp_rfds), &(engine.temp_wfds), NULL, NULL);
        if (selReturn < 0)
        {
            close_socket(engine.sock);
            liso_logger_log(ERROR, "select", "select returned -1", port, engine.logger.loggerfd);
            liso_logger_close(&(engine.logger));
            exit(4);
        }

        if (selReturn == 0)
        {
            continue;
        }

        for (i = 0; i < engine.fdmax + 1; i++)
        {
            memset(engine.buf, 0, BUF_SIZE);

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
        }
    }

    return 1;
}

int liso_handle_recv(int i)
{
    socklen_t cli_size;
    struct sockaddr_in cli_addr;
    int noDataRead, readret, client_sock, errnoSave;
    es_connection *currConnection;

    if (i == engine.sock || i == engine.ssl_sock)
    {
        if (engine.nfds < MAX_CONNECTIONS - 1)
        {
            cli_size = sizeof(cli_addr);

            if (i == engine.sock)
            {
                client_sock = accept(engine.sock, (struct sockaddr *) &cli_addr, &cli_size);
            }
            else if (i == engine.ssl_sock)
            {
                client_sock = accept(engine.ssl_sock, (struct sockaddr *) &cli_addr, &cli_size);
            }

            if (client_sock < 0)
            {
                SSL_CTX_free(engine.ssl_context);
                close(engine.sock);
                close(engine.ssl_sock);
                liso_logger_log(ERROR, "accpet", "accept error", port, engine.logger.loggerfd);
                return -1;
            }

            FD_SET(client_sock, &(engine.rfds));

            init_connection(&((engine.connections)[client_sock]));
            (engine.connections)[client_sock].remote_ip = inet_ntoa(cli_addr.sin_addr);

            if (i == engine.ssl_sock) 
            {
                currConnection = &((engine.connections)[client_sock]);

                currConnection->context = SSL_new(engine.ssl_context);

                if (SSL_wrap(currConnection, client_sock) < 0)
                {
                    return -1;
                }
            }

            if (client_sock > engine.fdmax)
            {
                engine.fdmax = client_sock;
            }

            (engine.nfds)++;
        }
        else if (engine.nfds == MAX_CONNECTIONS -1)
        {
            client_sock = accept(engine.sock, (struct sockaddr *) &cli_addr, &cli_size);

            if (client_sock < 0)
            {
                close(engine.sock);
                liso_logger_log(ERROR, "accept", "accpet error", ssl_port, engine.logger.loggerfd);
                return -1;
            }
            init_connection(&((engine.connections)[client_sock]));

            (engine.connections)[i].request->status = 503;

            if (i == engine.ssl_sock) 
            {
                currConnection = &((engine.connections)[client_sock]);

                currConnection->context = SSL_new(engine.ssl_context);

                if (SSL_wrap(currConnection, client_sock) < 0)
                {
                    return -1;
                }
            }

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

        if (currConnection->context != NULL)
        {
            readret = SSL_read(currConnection->context, engine.buf, BUF_SIZE - currConnection->bufindex);
        }
        else
        {
            readret = recv(i, engine.buf, BUF_SIZE - currConnection->bufindex, 0);
        }

        if (currConnection->request->status == 200 || currConnection->request->status == 0)
        {
            if (currConnection->context == NULL)
            {
                if (readret == 0)
                {
                    liso_close_and_cleanup(i);
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
                        liso_close_and_cleanup(i);
                        liso_logger_log(ERROR, "liso_handle_recv", "ECONNRESET\n", port, engine.logger.loggerfd);
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
                        liso_close_and_cleanup(i);
                        return -1;
                    }
                }

                if (readret < 0)
                {
                    if (SSL_get_error(currConnection->context, readret) == SSL_ERROR_ZERO_RETURN)
                    {
                        liso_logger_log(ERROR, "liso_handle_recv", "SSL_ERROR_ZERO_RETURN\n", ssl_port, engine.logger.loggerfd);
                        SSL_free(currConnection->context);
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
                        liso_logger_log(ERROR, "liso_handle_recv", "Unidentified SSL_read error \n", ssl_port, engine.logger.loggerfd);
                        liso_close_and_cleanup(i);
                        return -1;
                    }
                }
            }

            if (noDataRead == 0)
            {
                memcpy(currConnection->buf + currConnection->bufindex, engine.buf, readret);
                currConnection->bufindex += readret;

                process_buffer(currConnection, i);

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


    if (strstr(currConnection->request->uri, "/cgi/") != NULL)
    {
        retval = cgi_send_response(currConnection, i);

        if (retval == -1)
        {
            return -1;
        }

        if (retval == 0)
        {
            FD_CLR((currConnection->stdout_pipe)[0], &(engine.wfds));
            FD_CLR((currConnection->stdin_pipe)[1], &(engine.rfds));
            cgi_close_parent_pipe(currConnection);
            engine.nfds--;
            engine.nfds--;
            get_ready_for_pipeline(currConnection);
            FD_CLR(i, &(engine.wfds));
        }

        if (currConnection->bufindex != 0)
        {
            process_buffer(currConnection, i);
        }
    }
    else if (currConnection->request->status != 200)
    {
        retval =  send_response(currConnection, i);

        if (retval != -1)
        {
            non_poisonouse_finish(currConnection, i);
        }
    }
    else
    {
        if (strcmp(POST, currConnection->request->method) == 0)
        {
            retval = send_response(currConnection, i);

            if (retval != -1)
            {
                non_poisonouse_finish(currConnection, i);
            }
        }
        else if(strcmp(HEAD, currConnection->request->method) == 0)
        {
            retval = send_response(currConnection, i);

            if (retval != -1)
            {
                non_poisonouse_finish(currConnection, i);
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

                    if (retval != -1)
                    {
                        if (currConnection->sendContentSize == 0)
                        {
                            non_poisonouse_finish(currConnection, i);
                        }
                    }
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
    char *content_length_s;

    if (connection->postfinish == 0)
    {
        if (strstr(connection->request->uri, "/cgi/") != NULL || connection->request->status == 200)
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
                writeSize = content_length - connection->iindex;
            }

            if (strstr(connection->request->uri, "/cgi/") != NULL)
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
        writeret = send(i, connection->response + connection->responseIndex, sendSize, MSG_NOSIGNAL);
    }
    else 
    {
        writeret = SSL_write(connection->context, connection->response + connection->responseIndex, sendSize);
    }

    if (writeret < 0)
    {
        errnoSave = errno;

        FD_CLR(i, &(engine.rfds));
        FD_CLR(i, &(engine.wfds));

        if(connection->context != NULL)
        {
            SSL_free(connection->context);
        }

        cleanup_connection(connection);
        close_socket(i);

        if (errnoSave != ECONNRESET && errnoSave != EPIPE)
        {
            close_socket(engine.sock);
            close_socket(engine.ssl_sock);
            if (connection->context != NULL)
            {
                SSL_free(connection->context);
            }
            SSL_CTX_free(engine.ssl_context);
            liso_logger_log(ERROR, "send_response", "Either send or SSL_WRITE error.\n", ssl_port, engine.logger.loggerfd);
            exit(EXIT_FAILURE);
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

    return 1;
}

int handle_get_io(es_connection *connection, int i)
{
    FILE *fp;
    int filefd;
    int iosize;
    int errnoSave;

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
        fclose(fp);
    }

    if (iosize < 0)
    {
        errnoSave = errno;

        FD_CLR(i, &(engine.rfds));
        FD_CLR(i, &(engine.wfds));

        if (connection->context != NULL)
        {
            SSL_free(connection->context);
        }

        cleanup_connection(connection);
        close_socket(i);

        (engine.nfds)--;

        return -1;
    }

    connection->sendContentSize -= iosize;

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
        memset(engine.buf, 0, BUF_SIZE);
        offset = (next_data + 4) - connection->buf;
        memcpy(engine.buf, connection->buf, connection->bufindex);
        memset(connection->buf, 0, BUF_SIZE);
        memcpy(connection->buf, engine.buf + offset, connection->bufindex - offset); // verify that this is correct
        connection->bufindex = connection->bufindex - offset; // check if this is actually correct
    }
}

void liso_select_cleanup(int i)
{
    es_connection *currConnection = &((engine.connections)[i]);

    if (strstr(currConnection->request->uri, "/cgi/") != NULL)
    {
        FD_CLR((currConnection->stdout_pipe)[0], &(engine.wfds));
        FD_CLR((currConnection->stdin_pipe)[1], &(engine.rfds));
        FD_CLR(i, &(engine.rfds));
        FD_CLR(i, &(engine.wfds));
        engine.nfds--;
        engine.nfds--;
    }

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

    if (strcmp(conn, "Close") == 0)
    {
        liso_close_and_cleanup(i);
    }
}

int close_socket(int sock)
{
    if (close(sock))
    {
        liso_logger_log(ERROR, "close_socket", "close_socket error\n", port, engine.logger.loggerfd);
        liso_logger_close(&(engine.logger));
        return 1;
    }
    return 0;
}

void look_buffer_for_request(es_connection *connection, int i)
{
    char *next_data;

    if (connection->bufindex == BUF_SIZE)
    {
        if ((next_data = strstr(connection->buf, "\r\n\r\n")) == NULL)
        {
            connection->request->status = 500;
            FD_SET(i, &(engine.wfds));
        }
    }

    if ((next_data = strstr(connection->buf, "\r\n\r\n")) != NULL)
    {
        parse_http(connection);
        connection->hasRequestHeaders = 1;
        determine_status(connection);

        if (strstr(connection->request->uri, "/cgi/"))
        {
            cgi_init(connection);
        }
    }
    else
    {
        connection->request->status = 400;
        FD_SET(i, &(engine.wfds));
    }

    if (next_data != NULL)
    {
        buffer_shift_forward(connection, next_data);
    }
}

void process_buffer(es_connection *connection, int i)
{
    if (connection->hasRequestHeaders == 0)
    {
        look_buffer_for_request(connection, i);

        if (connection->request->status != 200 && connection->request->status != 0)
        {
            FD_SET(i, &(engine.wfds));
        }

        post_recv_phase(connection, i);
    }
    else if (connection->hasRequestHeaders == 1)
    {
        if (strcmp(POST, connection->request->method) == 0)
        {
            post_recv_phase(connection, i);
        }
        else
        {
            FD_SET(i, &(engine.wfds));
        }
    }
}

void non_poisonouse_finish(es_connection *connection, int i)
{
    char *conn;
    char *version;

    if (connection->responseLeft == 0)
    {
        conn = get_value(connection->request->headers, "connection");
        version = connection->request->version;

        if (strcmp("HTTP/1.0", version) == 0)
        {
            liso_close_and_cleanup(i);
        }
        else if (conn != NULL && strcmp(conn, "Close") == 0)
        {
            liso_close_and_cleanup(i);
        }
        else 
        {
            get_ready_for_pipeline(connection);
            FD_CLR(i, &(engine.wfds));
        }

        if (connection->bufindex != 0)
        {
            process_buffer(connection, i);
        }
    }
}

void SSL_init()
{
    SSL_load_error_strings();
    SSL_library_init();

    if ((engine.ssl_context = SSL_CTX_new(TLSv1_server_method())) == NULL)
    {
        liso_logger_log(ERROR, "SSL_init", "Error creating SSL context.\n", port, engine.logger.loggerfd);
        liso_logger_close(&(engine.logger));
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(engine.ssl_context, key, SSL_FILETYPE_PEM) == 0)
    {
        SSL_CTX_free(engine.ssl_context);
        liso_logger_log(ERROR, "SSL_init", "Error associating private key.\n", port, engine.logger.loggerfd);
        liso_logger_close(&(engine.logger));
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_certificate_file(engine.ssl_context, cert, SSL_FILETYPE_PEM) == 0)
    {
        SSL_CTX_free(engine.ssl_context);
        liso_logger_log(ERROR, "SSL_init", "Error associating certification.\n", port, engine.logger.loggerfd);
        liso_logger_close(&(engine.logger));
        exit(EXIT_FAILURE);
    }
}

int SSL_wrap(es_connection *connection, int sock)
{
    if (connection->context != NULL)
    {
        if (SSL_set_fd(connection->context, sock) == 0)
        {
            close_socket(engine.sock);
            close_socket(engine.ssl_sock);
            SSL_free(connection->context);
            SSL_CTX_free(engine.ssl_context);
            liso_logger_log(ERROR, "SSL_set_fd", "Error creating client SSL context.\n", ssl_port, engine.logger.loggerfd);
            liso_logger_close(&(engine.logger));
            exit(EXIT_FAILURE);
        }

        if ( SSL_accept(connection->context) <= 0)
        {
            close_socket(sock);
            FD_CLR(sock, &(engine.rfds));
            FD_CLR(sock, &(engine.wfds));
            SSL_free(connection->context);
            liso_logger_log(ERROR, "SSL_accpt", "Error accepting client SSL context.\n", ssl_port, engine.logger.loggerfd);
            return -1;
        }
    }
    return 1; 
}
