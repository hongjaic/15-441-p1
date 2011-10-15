/**
 * CS 15-441 Computer Networks
 *
 * @file    liso_cgi.c
 * @author  Hong Jai Cho <hongjaic@andrew.cmu.edu>
 */

#include "liso_cgi.h"

int cgi_init(es_connection *connection)
{

    //printf("CGI INI STARTED\n");
    pid_t pid;

    char *val;
    char path_info_s[2048];
    char request_uri_s[2048];
    char query_string_s[2048];

    http_request *request = connection->request;
    char *uri = request->uri;

    parse_uri(uri, path_info_s, request_uri_s, query_string_s);

    char content_length[2048];
    char content_type[2048];
    char gateway_interface[2048];
    char path_info[2048];
    char request_uri[2048];
    char query_string[2048];
    char remote_addr[2048];
    char request_method[2048];
    char server_port[2048];
    char http_accept[2048];
    char http_referer[2048];
    char http_accept_encoding[2048];
    char http_accept_language[2048];
    char http_accept_charset[2048];
    char http_host[2048];
    char http_cookie[2048];
    char http_user_agent[2048];
    char http_connection[2048];

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
        if ((val = get_value(connection->request->headers, "content-length")) != NULL)
        {
            sprintf(content_length, "%s%s", "CONTENT_LENGTH=", val);
            ENVP[0] = content_length;
        }
        else
        {
            ENVP[0] = "CONTENT_LENGTH=";
        }

        if ((val = get_value(connection->request->headers, "content-type")) != NULL)
        {
            sprintf(content_type, "%s%s", "CONTENT_TYPE=", val);
            ENVP[1] = content_type;
        }
        else
        {
            ENVP[1] = "CONTENT_TYPE=";
        }

        if ((val = get_value(connection->request->headers, "gateway-interface")) != NULL)
        {
            sprintf(gateway_interface, "%s%s", "GATEWAY_INTERFACE=", val);
            ENVP[2] = gateway_interface;
        }
        else
        {
            ENVP[2] = "GATEWAY_INTERFACE=";
        }

        if (path_info_s != NULL)
        {
            sprintf(path_info, "%s%s", "PATH_INFO=", path_info_s);
            ENVP[3] = path_info;
        }
        else
        {
            ENVP[3] = "PATH_INFO=";
        }

        if (query_string_s != NULL)
        {
            sprintf(query_string, "%s%s", "QUERY_STRING=", query_string_s);
            ENVP[4] = query_string;
        }
        else
        {
            ENVP[4] = query_string;
        }

        sprintf(remote_addr, "%s%s", "REMOTE_ADDR=", connection->remote_ip);
        ENVP[5] = remote_addr;
        
        if ((val = connection->request->method) != NULL)
        {
            sprintf(request_method, "%s%s", "REQUEST_METHOD=", val);
            ENVP[6] = request_method;
        }
        else
        {
            ENVP[6] = "REQUEST_METHOD=";
        }

        if (request_uri_s != NULL)
        {
            sprintf(request_uri, "%s%s", "REQUEST_URI=", request_uri_s);
            ENVP[7] = request_uri;
        }
        else
        {
            ENVP[7] = "REQUEST_URI";
        }

        ENVP[8] = "SCRIPT_NAME=/cgi";

        if (connection->context != NULL)
        {
            sprintf(server_port, "%s%d", "SERVER_PORT=", ssl_port);    
        }
        else
        {
            sprintf(server_port, "%s%d", "SERVER_PORT=", port);
        }

        ENVP[9] = server_port;


        ENVP[10] = "HTTP/1.1";

        ENVP[11] = "Liso/1.0";

        if ((val = get_value(connection->request->headers, "http-accept")) != NULL)
        {
            sprintf(http_accept, "%s%s", "HTTP_ACCEPT=", val);
            ENVP[12] = http_accept;
        }
        else
        {
            ENVP[12] = "HTTP_ACCEPT=";
        }

        if ((val = get_value(connection->request->headers, "http-referer")) != NULL)
        {
            sprintf(http_referer, "%s%s", "HTTP_REFERER=", val);
            ENVP[13] = http_referer;
        }
        else
        {
            ENVP[13] = "HTTP_REFERER=";
        }

        if ((val = get_value(connection->request->headers, "http-accept-encoding")) != NULL)
        {
            sprintf(http_accept_encoding, "%s%s", "HTTP_ACCEPT_ENCODING=", val);
            ENVP[14] = http_accept_encoding;
        }
        else
        {
            ENVP[14] = "HTTP_ACCEPT_ENCODING=";
        }

        if ((val = get_value(connection->request->headers, "http-accept-language")) != NULL)
        {
            sprintf(http_accept_language, "%s%s", "HTTP_ACCEPT_LANGUAGE=", val);
            ENVP[15] = http_accept_language;
        }
        else
        {
            ENVP[15] = "HTTP_ACCEPT_LANGUAGE=";
        }

        if ((val = get_value(connection->request->headers, "http-accept-charset")) != NULL)
        {
            sprintf(http_accept_charset, "%s%s", "HTTP_ACCEPT_CHARSET=", val);
            ENVP[16] = http_accept_charset;
        }
        else
        {
            ENVP[16] = "HTTP_ACCEPT_CHARSET=";
        }

        if ((val = get_value(connection->request->headers, "http-host")) != NULL)
        {
            sprintf(http_host, "%s%s", "HTTP_HOST=", val);
            ENVP[17] = http_host;
        }
        else
        {
            ENVP[17] = "HTTP_HOST=";
        }

        if ((val = get_value(connection->request->headers, "http-cookie")) != NULL)
        {
            sprintf(http_cookie, "%s%s", "HTTP_COOKIE=", val);
            ENVP[18] = http_cookie;
        }
        else
        {
            ENVP[18] = "HTTP_COOKIE=";
        }

        if ((val = get_value(connection->request->headers, "http-user-agent")) != NULL)
        {
            sprintf(http_user_agent, "%s%s", "HTTP_USER_AGENT=", val);
            ENVP[19] = http_user_agent;
        }
        else
        {
            ENVP[19] = "HTTP_USER_AGENT=";
        }

        if ((val = get_value(connection->request->headers, "http-connection")) != NULL)
        {
            sprintf(http_connection, "%s%s", "HTTP_CONNECTION=", val);
            ENVP[20] = http_connection;
        }
        else
        {
            ENVP[20] = "HTTP_CONNECTION=";
        }

        close((connection->stdout_pipe)[0]);
        close((connection->stdin_pipe)[1]);
        dup2((connection->stdout_pipe)[1], fileno(stdout));
        dup2((connection->stdin_pipe)[0], fileno(stdin));

        if(execve(cgi, ARGV, ENVP) < 0)
        {
            printf("errno%d\n", errno);
            //execve_error_handler();
            perror("execve");
            return -1;
        }
    }

    if (pid > 0)
    {
        FD_SET((connection->stdout_pipe)[0], &(engine.wfds));
        if((connection->stdout_pipe)[0] > engine.fdmax) engine.fdmax = (connection->stdout_pipe)[0];
        FD_SET((connection->stdin_pipe)[1], &(engine.rfds));
        if((connection->stdin_pipe)[1] >engine.fdmax) engine.fdmax = (connection->stdin_pipe)[1];

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
        FD_CLR((connection->stdout_pipe)[0], &(engine.wfds));
        FD_CLR((connection->stdin_pipe)[1], &(engine.rfds));
        return -1;
    }

    return writeret;
}

int cgi_send_response(es_connection *connection, int i)
{
    int sent;
    int read;
    char temp[8096];
    int errnoSave;

    read = cgi_read(connection, connection->response);

    if (read < 0)
    {
        return -1;
    }

    (connection->responseIndex) += read;

    sent = send(i, connection->response, connection->responseIndex, MSG_NOSIGNAL);

    if (sent < 0)
    {
        errnoSave = errno;

        FD_CLR(i, &(engine.rfds));
        FD_CLR(i, &(engine.wfds));
        FD_CLR((connection->stdout_pipe)[0], &(engine.wfds));
        FD_CLR((connection->stdin_pipe)[1], &(engine.rfds));

        cleanup_connection(connection);
        close_socket(i);

        if (errnoSave != ECONNRESET && errnoSave != EPIPE)
        {
            close_socket(engine.sock);
            exit(EXIT_FAILURE);
        }

        (engine.nfds)--;

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

    if ((readret = read((connection->stdout_pipe)[0], buf, 8096 - connection->responseIndex-1)) < 0)
    {
        FD_CLR((connection->stdout_pipe)[0], &(engine.wfds));
        FD_CLR((connection->stdin_pipe)[1], &(engine.rfds));
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

