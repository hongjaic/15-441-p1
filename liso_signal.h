/**
 * CS 15-441 Computer Networks
 *
 * @file    liso_signal.h
 * @author  Hong Jai Cho <hongjaic@andrew.cmu.edu>
 */

#ifndef LISO_SIGNAL_H
#define LISO_SIGNAL_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

void sigchld_handler(int sig);

#endif
