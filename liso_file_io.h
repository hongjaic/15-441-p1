/**
 * CS 15-441 Computer Networks
 *
 * @file    liso_file_io.h
 * @author  Hong Jai Cho <hongjaic@andrew.cmu.edu>
 *
 * This file contains the definitions of functions used for file io. Also
 * contains methods to send requested file to client.
 */

#ifndef LISO_FILE_H
#define LISO_FILE_H
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <openssl/ssl.h>

#define MAXIOSIZE   8192

int ssl_send_file(SSL *client_context, FILE *fp, int *offset, int size);
int send_file(int fd, char *file, int offset);
int write_file(char *file, char *buf, int offset, int size);
char *file_type(char *file);
int file_size(char *file);

#endif
