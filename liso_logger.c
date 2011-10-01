/**
 * CS 15-441 Computer Networks
 *
 * @file    liso_logger.c
 * @author  Hong Jai Cho (hongjaic@andrew.cmu.edu)
 */

#include "liso_logger.h"

void liso_logger_create(liso_logger *logger, char *flog)
{
    logger->loggerfd = fopen(flog, "w");

    if (logger->loggerfd == NULL)
    {
        fprintf(stderr, "Error opening logger.\n");
    }
}

void liso_logger_log(char *type, char *location, char *content, int port, FILE *log)
{
    char *logstr;
    int logLength = 0;

    if (type != NULL)
        logLength += strlen(type);

    if (location != NULL)
        logLength += strlen(location);

    if (content != NULL)
        logLength += strlen(content);

    logLength += 10;

    logstr = (char *) malloc(logLength);

    if (type != NULL)
        strcpy(logstr, type);
        
    strcpy(logstr, "\\");

    if (location != NULL)
        strcpy(logstr, location);

    strcpy(logstr, ": ");

    if (content != NULL)
        strcpy(logstr, content);

    fprintf(log, "%s : Port %d\n", logstr, port);

    free(logstr);
}

void liso_logger_close(liso_logger *logger)
{
    fclose(logger->loggerfd);
}
