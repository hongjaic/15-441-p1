/**
 *  CS 15-441 Computer Networks
 *  
 *  @file   es_connection.h
 *  @author Hong Jai Cho <hongjaic@andrew.cmu.edu>
 *  
 *  A struct to maintain the state of a connection.
 */

#ifndef ES_CONNECTION_H
#define ES_CONNECTION_H
#include "http_request.h"

#define BUF_SIZE        8192
#define RW_BUF_SIZE     8192
#define MAX_URI_LENGTH  2048
#define MAX_DIR_LENGTH    2304

typedef struct es_connection {
    int bufindex;
    char buf[RW_BUF_SIZE];
    int bufAvailable;
    int requestOverflow;
    int hasRequestHeaders;
    int maxExceeded;
    int sentResponseHeaders;
    int iindex;
    int postfinish;
    off_t oindex;
    off_t ioIndex;
    int sendContentSize;
    int responseIndex;
    int responseLeft;
    char *nextData;
    char response[RW_BUF_SIZE];
    char dir[MAX_DIR_LENGTH];
    http_request *request;
} es_connection;

void init_connection(es_connection *connection);
void cleanup_connection(es_connection *connection);

#endif
