/**
 *  CS 15-441 Computer Networks
 *  
 *  @file   es_connection.h
 *  @author Hong Jai Cho <hongjaic@andrew.cmu.edu>
 *  
 *  A struct to maintain the state of a connection.
 */

#define RW_BUF_SIZE 4096

typedef struct es_connection {
    int bufindex;
    char buf[RW_BUF_SIZE];
    int bufAvailable;
} es_connection;

