/**
 * CS 15-441 Computer Networks
 *
 * @file    http_parser.h
 * @author  Hong Jai Cho <hongjaic@andrew.cmu.edu>
 *
 * This file defines the methods that are used for parsing and handling http
 * requests.
 */

#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "liso_hash.h"
#include "liso_file_io.h"
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include "es_connection.h"
#include "http_request.h"

#define HEAD    "HEAD"
#define GET     "GET"
#define POST    "POST"
#define PUT     "PUT"
#define OPTIONS "OPTIONS"
#define DELETE  "DELETE"
#define TRACE   "TRACE"
#define CONNECT "CONNECT"
#define PATCH   "PATCH"

#define METHODLENGTH  5
#define MAXLINE     8192
#define MAX_URI_LENGTH  2048
#define VERSION_LENGTH  9

void init_request(http_request *request, int clietfd);
int http_handle_request(http_request *request);
int build_response(es_connection *connection, char *response);
void parse_request_line(char *request_line, http_request *request);
void parse_headers(char *headers, http_request *request);
int parse_body(char *request, http_request *request_wrapper);
void parse_request(char *request, char *request_line, char *headers);
void determine_status(es_connection *connection);
int request_method_is_implemented(char *method);
int request_method_valid(char *method);
char *status_message(int status);

#endif
