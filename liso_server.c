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
#include "liso_select_engine.h"


#define ECHO_PORT 9999
#define BUF_SIZE 8192
#define MAX_CONNECTIONS 1024
#define MAX_URI_LENGTH  2048

#define USAGE "\nUsage: %s <PORT> <SSL_PORT> <LOG_FILE> <LOCK_FILE> <WWW> <CGI>\n\n"

liso_select_engine engine;
char *www;
int port;
int ssl_port;

char* ARGV[] = {
                    FILENAME,
                    NULL
               };

char* ENVP[] = {
                    "CONTENT_LENGTH=",
                    "CONTENT-TYPE=",
                    "GATEWAY_INTERFACE=CGI/1.1",
                    "QUERY_STRING=action=opensearch&search=HT&namespace=0&suggest=",
                    "REMOTE_ADDR=128.2.215.22",
                    "REMOTE_HOST=gs9671.sp.cs.cmu.edu",
                    "REQUEST_METHOD=GET",
                    "SCRIPT_NAME=/w/api.php",
                    "HOST_NAME=en.wikipedia.org",
                    "SERVER_PORT=80",
                    "SERVER_PROTOCOL=HTTP/1.1",
                    "SERVER_SOFTWARE=Liso/1.0",
                    "HTTP_ACCEPT=application/json, text/javascript, */*; q=0.01",
                    "HTTP_REFERER=http://en.wikipedia.org/w/index.php?title=Special%3ASearch&search=test+wikipedia+search",
                    "HTTP_ACCEPT_ENCODING=gzip,deflate,sdch",
                    "HTTP_ACCEPT_LANGUAGE=en-US,en;q=0.8",
                    "HTTP_ACCEPT_CHARSET=ISO-8859-1,utf-8;q=0.7,*;q=0.3",
                    "HTTP_COOKIE=clicktracking-session=v7JnLVqLFpy3bs5hVDdg4Man4F096mQmY; mediaWiki.user.bucket%3Aext.articleFeedback-tracking=8%3Aignore; mediaWiki.user.bucket%3Aext.articleFeedback-options=8%3Ashow; mediaWiki.user.bucket:ext.articleFeedback-tracking=8%3Aignore; mediaWiki.user.bucket:ext.articleFeedback-options=8%3Ashow",
                    "HTTP_USER_AGENT=Mozilla/5.0 (X11; Linux i686) AppleWebKit/535.1 (KHTML, like Gecko) Chrome/14.0.835.186 Safari/535.1",
                    "HTTP_CONNECTION=keep-alive",
                    "HTTP_HOST=en.wikipedia.org",
                    NULL
               };

int main(int argc, char* argv[])
{
    char *flog, *flock, *cgi;

    if (argc != 7)
    {
        fprintf(stderr, USAGE, argv[0]);
        exit(EXIT_FAILURE);
    }

    port = atoi(argv[1]);
    ssl_port = atoi(argv[2]);
    flog = argv[3];
    flock = argv[4];
    www = argv[5];
    cgi = argv[6];

    liso_engine_create(port, flog, flock);

    liso_logger_log("liso", "main", "liso select loop begin", port, engine.logger.loggerfd);

    liso_engine_event_loop();

    close_socket(engine.sock);

    liso_logger_close(&(engine.logger));

    return EXIT_SUCCESS;
}
