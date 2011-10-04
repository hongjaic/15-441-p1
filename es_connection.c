/**
 * CS 15-441 Conputer Networks
 *
 * @file    es_connection.h
 * @author  Hong Jai Cho <hongjaic@andrew.cmu.edu>
 */

#include "es_connection.h"

void init_connection(es_connection *connection)
{
    connection->bufindex = 0;
    memset(connection->buf, 0, BUF_SIZE);
    connection->bufAvailable = 1;
    connection->requestOverflow = 0;
    connection->hasRequestHeaders = 0;
    connection->sentResponseHeaders = 0;
    connection->iindex = -1;
    connection->postfinish = 0;
    connection->oindex = -1;
    connection->ioIndex = -1;
    connection->sendContentSize = 0;
    connection->responseIndex = 0;
    connection->responseLeft = 0;
    connection->nextData = NULL;
    memset(connection->response, 0, BUF_SIZE);
    memset(connection->dir, 0, MAX_DIR_LENGTH);
    connection->request = (http_request *)malloc(sizeof(http_request));
    (connection->request)->headers = (liso_hash *)malloc(sizeof(liso_hash));
    init_hash((connection->request)->headers);
    (connection->request)->status = 0;
}

void cleanup_connection(es_connection *connection)
{
    connection->bufindex = 0;
    memset(connection->buf, 0, BUF_SIZE);
    connection->bufAvailable = 1;
    connection->requestOverflow = 0;
    connection->hasRequestHeaders = 0;
    connection->sentResponseHeaders = 0;
    connection->iindex = -1;
    connection->postfinish = 0;
    connection->oindex = -1;
    connection->ioIndex = -1;
    connection->sendContentSize = 0;
    connection->responseIndex = 0;
    connection->responseLeft = 0;
    connection->nextData = NULL;
    memset(connection->response, 0, BUF_SIZE);
    memset(connection->dir, 0, MAX_DIR_LENGTH);
    collapse((connection->request)->headers);
    free((connection->request)->headers);
    free(connection->request);
}
