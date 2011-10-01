/**
 * CS 15-441 Computer Networks
 *
 * @file    http_parser.h
 * @author  Hong Jai Cho <hongjaic@andrew.cmu.edu>
 *
 * This file defines the methods that are used for parsing and handling http
 * requests.
 */

#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "liso_hash.h"
#include "liso_file_io.h"
#include <ctype.h>
#include <sys/stat.h>

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

typedef struct http_request
{
    int clientfd;
    char method[METHODLENGTH];
    char uri[MAX_URI_LENGTH];
    char version[VERSION_LENGTH];
    liso_hash *headers;
    int content_length;
    char *body;
    int status;
    struct http_request *next_request;
} http_request;

#endif
