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
#include "liso_signal.h"


#define ECHO_PORT 9999
#define BUF_SIZE 8192
#define MAX_CONNECTIONS 1024
#define MAX_URI_LENGTH  2048

#define USAGE "\nUsage: %s <PORT> <SSL_PORT> <LOG_FILE> <LOCK_FILE> <WWW> <CGI> <private key file> <certificate file>\n\n"

liso_select_engine engine;
char *www;
int port;
int ssl_port;
char *cgi;
char *key;
char *cert;

char * ARGV[2];
char *ENVP[23]; 

int main(int argc, char* argv[])
{
    char *flog, *flock;

    if (argc != 9)
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
    key = argv[7];
    cert = argv[8];

    ARGV[0] = cgi;
    ARGV[1] = NULL;

    liso_engine_create(port, flog, flock);

    liso_logger_log("liso", "main", "liso select loop begin", port, engine.logger.loggerfd);

    liso_engine_event_loop();

    close_socket(engine.sock);

    liso_logger_close(&(engine.logger));

    return EXIT_SUCCESS;
}
