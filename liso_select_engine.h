/**
 * CS 15-441 Computer Networks
 *
 * @file    liso_select_engine.h
 * @author  Hong Jai Cho (hongjaic@andrew.cmu.edu)
 */

#ifndef LISO_SELECT_ENGINE_H
#define LISO_SELECT_ENGINE_H
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
#include <openssl/ssl.h>
#include "liso_signal.h"
#include "liso_cgi.h"

#define ECHO_PORT 9999
#define BUF_SIZE 8192
#define MAX_CONNECTIONS 1024
#define MAX_URI_LENGTH  2048

typedef struct liso_select_engine
{
    SSL_CTX *ssl_context;
    int sock;
    int ssl_sock;
    fd_set rfds;
    fd_set temp_rfds;
    fd_set wfds;
    fd_set temp_wfds;
    int nfds;
    int fdmax;
    es_connection connections[MAX_CONNECTIONS];
    liso_logger logger;
    char buf[BUF_SIZE];
} liso_select_engine;

extern liso_select_engine engine;
extern char *www;
extern int port;
extern int ssl_port;
extern char *key;
extern char *cert;

int liso_engine_create(int port, char *flog, char *flock);
int liso_engine_event_loop();

int close_socket(int sock);

#endif
