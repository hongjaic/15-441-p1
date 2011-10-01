CS 15-441 Computer Networks

Author: Hong Jai Cho <hongjaic@andrew.cmu.edu>

----------------------------------------------------------------------------------
CP1
----------------------------------------------------------------------------------
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
----------------------------------------------------------------------------------

----------------------------------------------------------------------------------
CP2
----------------------------------------------------------------------------------
Description:
My liso_server currently has a very similar architecture to the echo_server.
It uses the select function to mulitplex among clients.

It supports three types of HTTP requests -- HEAD, GET, and POST. 

For GET, my liso server reads the request, and parses it after reading the 
entire request. After parsing, the status code is set right away based on the 
request details, and when this happens, the write fd for the specific client is 
set. In the next iteration of the loop, a response message is sent to the 
client. After the response is set, the file that the client requested is also
sent.

HEAD requests are handled almost exactly same as the GET requests are handled.
The only difference is that only the reponse message is sent, and the file is
not sent.

POST is handled a little differently. After parsing the request is complete,
data gets written to the disk (buffer size at a time). After writing is
complete, the write fd for the specific client is set. In the next iteration
of the loop the response message is sent.

I have separate http paser, hash for parser, file io, and error handler, and
logger modules.
----------------------------------------------------------------------------------
