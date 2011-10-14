/**
 * CS 15-441 Computer Networks
 *
 * @file    liso_cgi.h
 * @author  Hong Jai Cho <hongjaic@andrew.cmu.edu>
 */

#ifndef LISO_CGI_H
#define LISO_CGI_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include "liso_logger.h"
#include "es_connection.h"
#include <unistd.h>
#include "http_parser.h"

#define FILENAME "./cgi_script.py"

extern char *ARGV[];
extern char *ENVP[];

int cgi_init(es_connection *connection);
int cgi_write(es_connection *connection, int writesize);
int cgi_read(es_connection *connection, char *buf);
int cgi_close_parent_pipe(es_connection *connection);
int cgi_send_response(es_connection *connection, int i);


#endif
