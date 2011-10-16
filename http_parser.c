/**
 * CS 15-441 Computer Networks
 *
 * @file    http_parser.c
 * @author  Hong Jai Cho <hongjaic@andrew.cmu.edu>
 */

#include "http_parser.h"

void generate_time(char *time_s);

void init_request(http_request *request, int clientfd)
{
    request->clientfd = clientfd;
    request->content_length = 0;
}

int build_response(es_connection *connection, char *response)
{
    int content_size;
    int status;
    char *content_size_s = get_value((connection->request)->headers, "content-length");
    char date[100];
    char *server = "Liso/1.0";
    char *content_type;
    char last_modified[100];
    char errorhtml[100];
    struct stat statbuf;

    char *conn;

    conn = get_value(connection->request->headers, "connection");

    if (conn == NULL)
    {
        conn = "Close";
    }

    if((status = (connection->request)->status) == 0)
        return -1;

    if(status != 200)
    {
        sprintf(errorhtml, "%s%d %s%s", "<html><body>", status, status_message(status), "</body></html>");
        content_size = strlen(errorhtml);
        generate_time(date);
        
        sprintf(response, "HTTP/1.1 %d %s\r\nConnection: Close\r\nDate: %s\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n%s", 
                    status, status_message(status), date, content_size, errorhtml);
        return 0;
    }

    if (strcmp((connection->request)->method, HEAD) == 0 || strcmp((connection->request)->method, GET) == 0)
    {
        stat(connection->dir, &statbuf);
        strftime(last_modified, 100, "%a, %d, %b, %Y, %H:%M:%s GMT", gmtime(&(statbuf.st_mtime)));
    }
    else if (strcmp((connection->request)->method, POST) == 0)
    {
        generate_time(last_modified);
    }

    if (content_size_s == NULL)
    {
        content_size = 0;
    }
    else
    {
        content_size = atoi(content_size_s);
    }

    if (strcmp((connection->request)->method, HEAD) == 0 || strcmp((connection->request)->method, GET) == 0)
    {
        content_size = statbuf.st_size;
        connection->sendContentSize = content_size;
    }

    content_type = file_type((connection->request)->uri);

    generate_time(date);

    if(strcmp((connection->request)->method, POST) == 0)
    {
        sprintf(response, 
                "HTTP/1.1 %d %s\r\nConnection: %s\r\nDate: %s\r\nServer: %s\r\nContent-Type: %s\r\nLast-Modified:%s\r\n\r\n",
                status, status_message(status),
                conn,
                date,
                server,
                content_type,
                last_modified
                );

    }
    else 
    { 
        sprintf(response, 
                "HTTP/1.1 %d %s\r\nConnection: %s\r\nDate: %s\r\nServer: %s\r\nContent-Length: %d\r\nContent-Type: %s\r\nLast-Modified: %s\r\n\r\n",
                status, status_message(status),
                conn,
                date,
                server,
                content_size,
                content_type,
                last_modified
                );
    }

    return 1;
}

void parse_request_line(char *request_line, http_request *request)
{
    if (sscanf(request_line, "%s %s %s", request->method, request->uri, request->version) == EOF)
    {
        memset(request->method, 0, METHODLENGTH);
        memset(request->uri, 0, MAX_URI_LENGTH);
        memset(request->version, 0, VERSION_LENGTH);
        request->status = 500;
        return;
    }

    if(strcmp(request->uri, "/") == 0)
    {
        memset(request->uri, 0, MAX_URI_LENGTH);
        sscanf("/index.html", "%s", request->uri);
    }

    if (request_method_is_implemented(request->method) == 0)
    {
        if (request_method_valid(request->method) == 1)
        {
            request->status = 501;
        }
        else
        {
            request->status = 400;
        }
        return;
    }

    if (strlen(request->uri) >= MAX_URI_LENGTH )
    {
        request->status = 414;
    }
}

void parse_headers(char *headers, http_request *request)
{
    char header[64];
    char value[256];
    char content_length[32];
    char c;

    char *limit = strstr(headers, "\r\n\r\n");

    int j = 0, l = 0;

    char *i = headers;
    char *k;

    if (*headers == '\0')
    {
        return;
    }

    while (i < limit)

    {
        c = *i;
        if (c == ':') 
        {
            if (*(i+1) != ' ')
            {
                request->status = 400;
                return;
            }

            header[j] = '\0';
            i += 2;
            k = strstr(i, "\r\n");
            for ( ; i < k; i++)
            {
                value[l] = *i;
                l++;
            }

            value[l] = '\0';

            hash_add(request->headers, header, value);

            i += 2;
            l = 0;
            j = 0;

            memset(header, 0, 64);
            memset(value, 0, 256);
        }
        else if (c == '\r')
        {
            request->status = 400;
            return;
        }
        else 
        {
            header[j] = c;
            j++;
            i++;
        }
    }

    i = NULL;

    if ((i = strstr(headers, "Content-Length:")) == NULL)
    {

        request->content_length = 0;
    }
    else
    {
        k = strstr(i, "\r\n");
        i += 16;
        j = 0;

        for ( ; i < k; i++)
        {
            content_length[j++] = *i;
        }

        request->content_length = atoi(content_length);
    }
}

int parse_body(char *request, http_request *request_wrapper)
{
    int i;
    char *head;
    int content_size;
    char *body;
    char *content_size_s = get_value(request_wrapper->headers, "Content-Length");

    if (content_size_s == NULL)
    {
        return 0;
    }

    content_size = atoi(content_size_s);
    head = strstr(request, "\r\n\r\n") + 4;

    body = (char *)malloc(content_size);


    for (i = 0; i < content_size; i++)
    {
        *(body + i) = *(head + i);
    }

    request_wrapper->body = body;

    return 1;
}

void parse_request(char *request, char *request_line, char * headers)
{
    int i;
    char *request_begin;
    char *request_end;
    char *headers_begin;
    char *headers_end;

    request_begin = request;
    request_end = strstr(request, "\r\n");
    headers_begin = request_end + 2;
    headers_end = strstr(request, "\r\n\r\n") + 4;

    i = 0;
    for ( ; request_begin < request_end; request_begin++)
    {
        *(request_line + i) = *request_begin;
        i++;
    }

    *(request_line + i) = '\0';

    if (headers_end != NULL && (headers_end - 4) != request_end)
    {
        i = 0;
        for ( ; headers_begin < headers_end; headers_begin++)
        {
            *(headers + i) = *headers_begin;
            i++;
        }

        *(headers + i) = '\0';
    }
    else
    {
        *headers = '\0';
    }
}

void parse_http(es_connection *connection)
{
    char request_line[MAXLINE];
    char headers[MAXLINE];

    parse_request(connection->buf, request_line, headers);
    parse_request_line(request_line, connection->request);
    parse_headers(headers, connection->request);
    sprintf(connection->dir, "%s%s", www, connection->request->uri);
}

void determine_status(es_connection *connection)
{
    int errnoSave;
    int filefd;
    char *content_len;
    int content_len_i;

    if ((connection->request)->status == 0 || (connection->request)->status == 200) {
        if (request_method_is_implemented((connection->request)->method) == 0)
        {
            if (request_method_valid((connection->request)->method) == 1)
            {
                (connection->request)->status = 501;
            }
            else
            {
                (connection->request)->status = 400;
            }

            return;
        }

        if (!(strcmp("HTTP/1.1", (connection->request)->version) == 0 || strcmp("HTTP/1.0", (connection->request)->version) == 0))
        {
            (connection->request)->status = 505;
            return;
        }

        if (strcmp(POST, (connection->request)->method) == 0 && contains_key(((connection->request)->headers), "content-length") == 0)
        {
            (connection->request)->status = 411;
            return;
        }

        if (strcmp(POST, (connection->request)->method) == 0 && contains_key(((connection->request)->headers), "content-length") == 1)
        {
            content_len = get_value((connection->request)->headers, "content-length");
            if ((content_len_i = atoi(content_len)) == 0)
            {
                if (isdigit(*content_len) == 0)
                {
                    (connection->request)->status = 400;
                    return;
                }
            }
        }

        if (strcmp(POST, (connection->request)->method) != 0)
        {
            if ((filefd = open(connection->dir, O_RDONLY)) < 0)
            {
                errnoSave = errno;

                if (errnoSave == EACCES)
                {
                    (connection->request)->status = 401;
                }
                else if (errnoSave == ENOENT || errnoSave == 20)
                {
                    (connection->request)->status = 404;
                }
                else
                {
                    (connection->request)->status = 500;
                }

                close(filefd);
                return;
            }

            close(filefd);
        }

        (connection->request)->status = 200;
    }
}

int request_method_is_implemented(char *method)
{
    if (strcmp(HEAD, method) == 0 || strcmp(GET, method) == 0 || strcmp(POST, method) == 0)
    {
        return 1;
    }

    return 0;
}

int request_method_valid(char *method)
{
    if (request_method_is_implemented(method) == 1)
    {
        return 1;
    }

    if (strcmp(OPTIONS, method) == 0 || strcmp(PUT, method) == 0 || strcmp(DELETE, method) == 0)
    {
        return 1;
    }

    if (strcmp(TRACE, method) == 0 || strcmp(CONNECT, method) == 0 || strcmp(PATCH, method) == 0)
    {
        return 1;
    }

    return 0;
}

char *status_message(int status)
{
    char *message;

    switch (status)
    {
        case 100: message = "Continue";
                  break;
        case 101: message = "Switching Protocols";
                  break;
        case 102: message = "Processing";
                  break;
        case 103: message = "Checkpoint";
                  break;
        case 122: message = "Request-URI too long";
                  break;
        case 200: message = "OK";
                  break;
        case 201: message = "Created";
                  break;
        case 202: message = "Accepted";
                  break;
        case 203: message = "Non-Authoritative Information";
                  break;
        case 204: message = "No Content";
                  break;
        case 205: message = "Reset Content";
                  break;
        case 206: message = "Partial Content";
                  break;
        case 207: message = "Multi-Status";
                  break;
        case 226: message = "IM Used";
                  break;
        case 300: message = "Multiple Choices";
                  break;
        case 301: message = "Moved Permanently";
                  break;
        case 302: message = "Found";
                  break;
        case 303: message = "See Other";
                  break;
        case 304: message = "Not Modified";
                  break;
        case 305: message = "Use Proxy";
                  break;
        case 306: message = "Switch Proxy";
                  break;
        case 307: message = "Temporary Redirect";
                  break;
        case 308: message = "Resume Incomplete";
                  break;
        case 400: message = "Bad Request";
                  break;
        case 401: message = "Unauthorized";
                  break;
        case 402: message = "Payment Required";
                  break;
        case 403: message = "Forbidden";
                  break;
        case 404: message = "Not Found";
                  break;
        case 405: message = "Method Not Allowed";
                  break;
        case 406: message = "Not Acceptable";
                  break;
        case 407: message = "Proxy Authentication Required";
                  break;
        case 408: message = "Request Timeout";
                  break;
        case 409: message = "Conflict";
                  break;
        case 410: message = "Gone";
                  break;
        case 411: message = "Length Required";
                  break;
        case 412: message = "Precondition Failed";
                  break;
        case 413: message = "Request Entity Too Large";
                  break;
        case 414: message = "Request-URI TOO Long";
                  break;
        case 415: message = "Unsupported Media Type";
                  break;
        case 416: message = "Requested Range Not Satisfiable";
                  break;
        case 417: message = "Expectation Failed";
                  break;
        case 418: message = "I'm a teapot";
                  break;
        case 422: message = "Unprocessible Entity";
                  break;
        case 423: message = "Locked";
                  break;
        case 424: message = "Falled Dependency";
                  break;
        case 425: message = "Unordered Collection";
                  break;
        case 426: message = "Upgrade Required";
                  break;
        case 444: message = "No Response";
                  break;
        case 449: message = "Retry With";
                  break;
        case 450: message = "Blocked by Windows Parental Controls";
                  break;
        case 499: message = "Client Closed Request";
                  break;
        case 500: message = "Internal Server Error";
                  break;
        case 501: message = "Not Implemented";
                  break;
        case 502: message = "Bad Gateway";
                  break;
        case 503: message = "Service Unavailable";
                  break;
        case 504: message = "Gateway Timeout";
                  break;
        case 505: message = "HTTP Version Not Supported";
                  break;
        case 506: message = "Variant Also Negotiates";
                  break;
        case 507: message = "Insufficient Storage";
                  break;
        case 509: message = "Bandwidth Limit Exceeded";
                  break;
        case 510: message = "Not Extended";
                  break;
        case 598: message = "network read timeout error";
                  break;
        case 599: message = "network connect timeout error";
                  break;
        default:
                  message = "Undefined status";
                  break;
    }

    return message;
}


void parse_uri(char *uri, char *path_info, char *request_uri, char *query_string)
{
    int i;
    char *path_begin;
    char *path_end;
    char *request_begin;
    char *request_end;
    char *query_begin;
    char *query_end;

    request_begin = uri;
    request_end = strstr(uri, "?");
    if (request_end == NULL)
    {
        request_end = uri + strlen(uri);
    }

    path_begin = uri + 4;
    path_end = request_end;
    
    query_begin = strstr(uri, "?");
    if (query_begin != NULL)
    {
        query_begin += 1;
    }
    query_end = uri + strlen(uri);

    i = 0;
    for ( ; request_begin < request_end; request_begin++)
    {
        *(request_uri + i) = *request_begin;
        i++;
    }

    *(request_uri + i) = '\0';

    i = 0;
    for ( ; path_begin < path_end; path_begin++)
    {
        *(path_info + i) = *path_begin;
        i++;
    }

    *(path_info + i) = '\0';

    if (query_begin != NULL)
    {
        i = 0;
        for ( ; query_begin < query_end; query_begin++)
        {
            *(query_string + i) = *query_begin;
            i++;
        }

        *(query_string + i) = '\0';
    }
    else
    {
        *query_string = '\0';
    }
}

void generate_time(char *time_s)
{
    time_t time_time;
    struct tm *tt;

    time_time = time(NULL);
    tt = gmtime(&time_time);

    strftime(time_s, 100, "%a, %d, %b, %Y %H:%M:%s GMT", tt);
}

