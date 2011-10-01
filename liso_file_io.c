/**
 * CS 15-441 Computer Networks
 *
 * @file    liso_file_io.c
 * @author  Hong Jai Cho <hongjaic@andrew.cmu.edu>
 */

#include "liso_file_io.h"

int send_file(int fd, char *file, int offset)
{
    int next;
    int writeret = 0;
    char buf[MAXIOSIZE];
    FILE *fp = fopen(file, "r");
    
    if (fp != NULL) 
    {
        fseek(fp, offset, SEEK_SET);
        next = fread(buf, 1, MAXIOSIZE, fp);
        
        writeret = send(fd, buf, next, MSG_DONTWAIT);

        if (writeret < 0)
        {
            /* handle this */
        }
        
        fclose(fp);
    }

    return writeret;
}

int write_file(char *file, char *buf, int offset, int size)
{
    int wrote;
    FILE *fp = fopen(file, "w");

    if (fp != NULL)
    {
        fseek(fp, offset, SEEK_SET);
        wrote = fwrite(buf, 1, MAXIOSIZE, fp);

        if (wrote < MAXIOSIZE)
        {
            if (feof(fp) != 0)
            {
                clearerr(fp);
                fclose(fp);
                return wrote;
            }

            if (ferror(fp) != 0)
            {
                clearerr(fp);
                fclose(fp);
                return -1;
            }
        }

        fclose(fp);
    }

    return wrote;
}

char *file_type(char *file)
{
    char *file_type;

    if ((strstr(file, ".html") != NULL) ||(strstr(file, ".htm") != NULL))
    {
        file_type = "text/html";
    }
    else if ((strstr(file, ".ogg")!= NULL) || (strstr(file, ".ogv") != NULL)  )
    {
        file_type = "video/ogg";
    }
    else if (strstr(file, ".txt") != NULL)
    {
        file_type = "text/plain";
    }
    else if (strstr(file, ".css") != NULL)
    {
        file_type = "text/css";
    }
    else if (strstr(file, ".gif") != NULL)
    {
        file_type = "image/gif";
    }
    else if ((strstr(file, ".jpg") != NULL) ||(strstr(file, ".jpeg") != NULL))
    {
        file_type = "image/jpeg";
    }
    else if (strstr(file, ".js") != NULL)
    {
        file_type = "application/javascript";
    }
    else if(strstr(file, "others") != NULL)
    {
        file_type = "application/octet-stream";
    }

    return file_type;
}

int file_size(char *file)
{
    int size;

    FILE *fp = fopen(file, "r");

    size = fseek(fp, 0, SEEK_END);

    fclose(fp);

    return size;
}
