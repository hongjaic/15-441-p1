CS 15-441 Computer Networks

Author: Hong Jai Cho <hongjaic@andrew.cmu.edu>

Description:
My echo_server, like the echo_servers of all the other students, utilizes the
select() function to multiplex among clients.

I maintain two main file descriptor sets, the readfds(rfds) and the
writefd(wfds), each with temporary fds. The temporary fds ensures stable
multiplexing by maitaining original fds' value while the I/O done.

To make sure that the server does not get blown up when too many people try to
connect, I maintain a counter to limit the number of connections (including
stdin, stdout, stderr, server socket) to 1024. If a client tries to cnnect to
the server when the limit is reached, the connection is deferred.

I handle the reads and writes in separate if cases. Write for a specific fd is
only possible if the read has happened.

I close the connection only when the read is finished (recv returns 0), and
close the file descriptors then. When this happens, I make sure that the server
does not randomly decide to write by skipping the write portion with a continue
statement.

At this time, I've modularized the error handler only. For the error handler,
instead of using the switch statement, I maintain a hastble that contains all
the relevant errno names and errno messages. When I encouter an error, I can
simply do a lookup and print a message. In the future when I have to do more
robust error handling, I might use the switch statement on top of what I have
right now.
