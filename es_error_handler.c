/**
 *  CS 15-441 Computer Networks
 *
 *  @file   es_error_handler.c
 *  @author Hong Jai Cho <hongjaic@andrew.cmu.edu>
 */

#include "es_error_handler.h"

void es_error_handler_setup() 
{
    /* errno names and message look up table */
    /* used for printing error messages */
    wrappers[EACCES].errname = "EACCES";
    wrappers[EACCES].errmsg = "inaccessible socket or permission denied";
    wrappers[EAFNOSUPPORT].errname = "EAFNOSUPPORT";
    wrappers[EAFNOSUPPORT].errmsg = "unsupported socket addressing family";
    wrappers[EINVAL].errname = "EINVAL";
    wrappers[EINVAL].errmsg = "invalid argument";
    wrappers[EMFILE].errname = "EMFILE";
    wrappers[EMFILE].errmsg = "too many open files";
    wrappers[ENOBUFS].errname = "ENOBUFS";
    wrappers[ENOBUFS].errmsg = "insufficient buffers in network software";
    wrappers[ENOMEM].errname = "ENOMEM";
    wrappers[ENOMEM].errmsg = "insufficient memory";
    wrappers[EPROTONOSUPPORT].errname = "EPROTONOSUPPORT";
    wrappers[EPROTONOSUPPORT].errmsg = "unsupported socket protocol";
    wrappers[EADDRINUSE].errname = "EADDRINUSE";
    wrappers[EADDRINUSE].errmsg = "socket address already in use";
    wrappers[EBADF].errname = "EBADF";
    wrappers[EBADF].errmsg = "file or socket not open or suitable";
    wrappers[ENOTSOCK].errname = "ENOTSOCK";
    wrappers[ENOTSOCK].errmsg = "file descriptor not associated with a socket";
    wrappers[EOPNOTSUPP].errname = "EOPNOTSUPP";
    wrappers[EOPNOTSUPP].errmsg = "operation not supported on socket";
    wrappers[EAGAIN].errname = "EAGAIN";
    wrappers[EAGAIN].errmsg = "resource temporarily unavailable";
    wrappers[ECONNABORTED].errname = "ECONNABORTED";
    wrappers[ECONNABORTED].errmsg = "connection aborted by local network software";
    wrappers[EINTR].errname = "EINTR";
    wrappers[EINTR].errmsg = "function failed due to interruption by signal";
    wrappers[ENFILE].errname = "ENFILE";
    wrappers[ENFILE].errmsg = "too many open HFS files in system";
    wrappers[EFAULT].errname = "EFAULT";
    wrappers[EFAULT].errmsg = "invalid argument address";
    wrappers[EPROTO].errname = "EPROTO";
    wrappers[EPROTO].errmsg = "protocol error";
    wrappers[EPERM].errname = "EPERM";
    wrappers[EPERM].errmsg = "firewall rules forbid connection";
    wrappers[ECONNREFUSED].errname = "ECONNREFUSED";
    wrappers[ECONNREFUSED].errmsg = "remote host refused conn";
    wrappers[ENOTCONN].errname = "ENOTCONN";
    wrappers[ENOTCONN].errmsg = "socket not connected yet";
    wrappers[ECONNRESET].errname = "ECONNRESET";
    wrappers[ECONNRESET].errmsg = "Connection reset by peer";
    wrappers[EDESTADDRREQ].errname = "EDESTADDRREQ";
    wrappers[EDESTADDRREQ].errmsg = "not connecting, no peer set";
    wrappers[EISCONN].errname = "EISCONN";
    wrappers[EISCONN].errmsg = "connected already";
    wrappers[EMSGSIZE].errname = "EMSGSIZE";
    wrappers[EMSGSIZE].errmsg = "atomic send, too big message";
    wrappers[EPIPE].errname = "EPIPE";
    wrappers[EPIPE].errmsg = "Local end shutdown";
    wrappers[EALREADY].errname = "EALREADY";
    wrappers[EALREADY].errmsg = "non-blocking, previous incomplete connection";
    wrappers[EINPROGRESS].errname = "EINPROGRESS";
    wrappers[EINPROGRESS].errmsg = "non-blocking, cannot immediately connect";
    wrappers[ENETUNREACH].errname = "ENETUNREACH";
    wrappers[ENETUNREACH].errmsg = "unreachable network";
}

void printErrorMessage(int err)
{
    fprintf(stderr, "ERROR: %s -- %s\n", wrappers[err].errname, wrappers[err].errmsg);
}

int es_socket_error_handle(int err)
{
    switch (err)
    {
        case EACCES:
            /* fall through */
        case EAFNOSUPPORT:
            /* fall through */
        case EINVAL:
            /* fall through */
        case EMFILE:
            /* fall through */
        case ENFILE:
            /* fall through */
        case ENOBUFS:
            /* fall through */
        case ENOMEM:
            /* fall through */
        case EPROTONOSUPPORT:
            break;
        default:
            fprintf(stderr, "ERROR: Unidentified error.\n");
            return -1;
    }

    printErrorMessage(err);

    return -1;
}

int es_bind_error_handle(int err)
{
    switch (err)
    {
        case EADDRINUSE:
            /* fall through */ 
        case EBADF:
            /* fall through */
        case EINVAL:
            /* fall through */
        case ENOTSOCK:
            break;
        default:
            fprintf(stderr, "ERROR: Unidentified error.\n");
            return -1;
    }

    printErrorMessage(err);
    
    return -1;
}

int es_listen_error_handle(int err)
{
    switch (err)
    {
        case EADDRINUSE:
            /* fall through */
        case EBADF:
            /* fall through */
        case ENOTSOCK:
            /* fall through */
        case EOPNOTSUPP:
            break;
        default:
            fprintf(stderr, "ERROR: Unidentified error.\n");
            return -1;
    }

    printErrorMessage(err);
    
    return -1;
}

int es_accept_error_handle(int err)
{
    switch (err)
    {
        case EAGAIN:
            /* fall through */
        case ECONNABORTED:
            /* fall thorugh */
        case EINTR:
            /* fall through */
        case EINVAL:
            /* fall through */
        case EMFILE:
            /* fall through */
        case ENFILE:
            /* fall through */
        case ENOTSOCK:
            /* fall through */
        case EOPNOTSUPP:
            /* fall through */
        case EFAULT:
            /* fall through */ 
        case ENOBUFS:
            /* fall through */
        case ENOMEM:
            /* fall through */
        case EPROTO:
            /* fall through */
        case EPERM:
            break;
        default:
            fprintf(stderr, "ERROR: Unidentified error.\n");
            return -1;
    }

    printErrorMessage(err);
    
    return -1;
}

int es_recv_error_handle(int err)
{
    int stayAlive = 0;

    switch (err)
    {
        case EAGAIN:
            /* fall through */
        case EBADF:
            /* fall through */
        case ECONNREFUSED:
            /* fall through */
        case EFAULT:
            /* fall through */
        case EINTR:
            /* fall through */
        case EINVAL:
            /* fall through */
        case ENOMEM:
            /* fall through */
        case ENOTCONN:
            /* fall through */
        case ENOTSOCK:
            break;
        case ECONNRESET:
            /* 
             * this error is handled a little differently so that the server
             * stays alive even if the client disconnects
             */
            stayAlive = 1;
            break;
        default:
            fprintf(stderr, "ERROR: Unidentified error.\n");
            return -1;
    }

    printErrorMessage(err);
    
    if (stayAlive == 1)
        return 1;
    return -1;
}

int es_send_error_handle(int err)
{
    int stayAlive = 0;

    switch (err)
    {
        case EACCES:
            /* fall through */
        case EAGAIN:
            /* fall through */
        case EBADF:
            /* fall through */
        case EDESTADDRREQ:
            /* fall through */
        case EFAULT:
            /* fall through */
        case EINTR:
            /* fall through */
        case EINVAL:
            /* fall through */
        case EISCONN:
            /* fall through */
        case EMSGSIZE:
            /* fall through */
        case ENOBUFS:
            /* fall through */
        case ENOMEM:
            /* fall through */
        case ENOTCONN:
            /* fall through */
        case ENOTSOCK:
            /* fall through */
        case EOPNOTSUPP:
            /* fall through */
        case EPIPE:
            break;
        case ECONNRESET:
             /* 
             * this error is handled a little differently so that the server
             * stays alive even if the client disconnects
             */
            stayAlive = 1;
            break;
        default:
            fprintf(stderr, "ERROR: Unidentified error.\n");
            return -1;
    }

    printErrorMessage(err);
    
    if (stayAlive == 1)
        return 1;
    return -1;
}

int es_connect_error_handle(int err)
{
    switch (err)
    {
        case EACCES:
            /* fall through */
        case EPERM:
            /* fall through */
        case EADDRINUSE:
            /* fall through */
        case EAFNOSUPPORT:
            /* fall through */
        case EAGAIN:
            /* fall through */
        case EALREADY:
            /* fall through */
        case EBADF:
            /* fall through */
        case ECONNREFUSED:
            /* fall through */
        case EFAULT:
            /* fall through */
        case EINPROGRESS:
            /* fall through */
        case EINTR:
            /* fall through */
        case EISCONN:
            /* fall through */
        case ENETUNREACH:
            /* fall through */
        case ENOTSOCK:
            break;
        default:
            fprintf(stderr, "ERROR: Unidentified error.\n");
            return -1;
    }

    printErrorMessage(err);
    
    return -1;
}
