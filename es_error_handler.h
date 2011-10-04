/**
 *  CS 15-441 Computer Networks
 *
 *  @file   es_error_handler.h
 *  @author Hong Jai Cho <hongjaic@andrew.cmu.edu>
 *
 *  This file defines a set of socket programming functions error handlers.
 *  A lookup hash is maintained for a quick and easy lookup when printing
 *  error messages.
 */

#ifndef ES_ERROR_HANDLER_H
#define ES_ERROR_HANDLER_H
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include "liso_logger.h"

#define HASH_SIZE   200

/* Struct that contains errno name and message */
typedef struct errnoWrappers
{
    char * errname;
    char * errmsg;
} errnoWrappers;

/* lookup hash */
errnoWrappers wrappers[HASH_SIZE];

void es_error_handler_setup();
int es_socket_error_handle(int err, int port, FILE *fp);
int es_bind_error_handle(int err, int port, FILE *fp);
int es_listen_error_handle(int err, int port, FILE *fp);
int es_accept_error_handle(int err, int port, FILE *fp);
int es_recv_error_handle(int err, int port, FILE *fp);
int es_send_error_handle(int err, int port, FILE *fp);
int es_connect_error_handle(int err, int port, FILE *fp);
void printErrorMessage(int err);

#endif
