/*
The MIT License (MIT)

Copyright (c) 2023 Wuriyanto <wuriyanto48@yahoo.co.id>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef REDIS_H
#define REDIS_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// socket headers
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

// close()
#include <unistd.h>

#define REDIS_ERROR -1
#define REDIS_OK 0

struct redis {
    int redis_fd;
    struct addrinfo* service_info;
};

struct redis_reply {
    char* reply;
};

typedef struct redis_reply* redis_reply_t;

struct redis* redis_connect(const char* host, const char* port, char* password);
void redis_close(struct redis* r);
redis_reply_t redis_send_command(struct redis* r, char* command);
void redis_reply_free(redis_reply_t reply);

struct redis* redis_connect(const char* host, const char* port, char* password) {
    struct redis* r = (struct redis*) malloc(sizeof(*r));
    if (r == NULL) {
        return NULL;
    }

    r->redis_fd = -1;

    int fd;
    struct addrinfo hints, *addr_info_p;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int address_info = getaddrinfo(host, port, &hints, &addr_info_p);
    if (address_info < 0) {
        redis_close(r);
        return NULL;
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        redis_close(r);
        return NULL;
    }

    int connected = connect(fd, addr_info_p->ai_addr, addr_info_p->ai_addrlen);
    if (connected < 0) {
        redis_close(r);
        return NULL;
    }

    // set file descriptor
    r->redis_fd = fd;
    r->service_info = addr_info_p;

    return r;
}

struct redis_reply* redis_send_command(struct redis* r, char* command) {
    if (r == NULL || r->redis_fd < 0) {
        return NULL;
    }

    char base_command[strlen(command) + 2];
    sprintf(base_command, "%s\r\n", command);

    int sent = send(r->redis_fd, (void*) base_command, strlen(base_command), 0);
    if (sent < 0) {
        return NULL;
    }

    char reply[1024];

    int n = recv(r->redis_fd, reply, sizeof(reply), 0);
    if (n < 0) {
        return NULL;
    }

    struct redis_reply* r_reply = (struct redis_reply*) malloc(sizeof(*r_reply));

    // remove garbage, read only the necessary data
    int read_to = 0;
    char* reply_p = reply;
    while (*reply_p++ != '\r') read_to++;

    // add one space for null terminator
    read_to = read_to + 1;

    printf("reply len %d\n", read_to);
    r_reply->reply = (char*) malloc(read_to);

    strncpy(r_reply->reply, reply, read_to);

    // null termination
    r_reply->reply[read_to-1] = 0x0;

    return r_reply;
}

void redis_reply_free(redis_reply_t reply) {
    if (reply != NULL) {
        free((void*) reply->reply);
        free((void*) reply);
    }
}

void redis_close(struct redis* r) {
    if (r != NULL) {
        if (r->redis_fd > 0) {
            close(r->redis_fd);
        }

        freeaddrinfo(r->service_info);
        free((void*) r);
    }
}

#endif