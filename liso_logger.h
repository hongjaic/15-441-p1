/**
 * CS 15-441 Computer Networks
 *
 * @file    liso_logger.h
 * @author  Hong Jai Cho (hongjaic@andrew.cmu.edu)
 */

#ifndef LISO_LOGGER_H
#define LISO_LOGGER_H
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct liso_logger
{
    FILE *loggerfd;
} liso_logger;

void liso_logger_create(liso_logger *logger, char *flog);

void liso_logger_log(char *type, char *location, char *content, int port, FILE *logger);

void liso_logger_close(liso_logger *logger);
#endif
