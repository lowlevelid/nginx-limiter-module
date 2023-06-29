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

// TODO: better parsing reply
#define REDIS_ERROR -1
#define REDIS_OK 0
#define REDIS_REPLY_OK "+OK"
#define REDIS_AUTH_REQUIRED "-NOAUTH Authentication required."
#define REDIS_AUTH_INVALID "-WRONGPASS invalid username-password pair or user is disabled."
#define REDIS_REPLY_EXPIRE_OK ":1"
#define REDIS_REPLY_GET_EMPTY "$-1"
#define REDIS_REPLY_GET_OK "$1"

// event handler
typedef void (*redis_on_success) (void*);
typedef void (*redis_on_error) (int);

struct redis {
    int redis_fd;
    struct addrinfo* service_info;
    int authenticated;
    redis_on_success on_connect_success;
    redis_on_error on_connect_error;
    redis_on_success on_auth_success;
    redis_on_error on_auth_error;
};

struct redis_reply {
    char* reply;
    char** reply_arr;
    int reply_arr_size;
};

typedef struct redis_reply* redis_reply_t;
typedef struct redis* redis_t;

redis_t redis_connect(const char* host, 
    const char* port, 
    char* password, 
    int db, 
    redis_on_success on_connect_success,
    redis_on_error on_connect_error,
    redis_on_success on_auth_success,
    redis_on_error on_auth_error);
void redis_close(redis_t r);
redis_reply_t redis_send_command(redis_t r, char* command);
void redis_reply_free(redis_reply_t reply);

int split_reply(char* line, char* delim, char** out, int* index_size);
char* to_lower(char* s);
char* to_upper(char* s);

redis_t redis_connect(const char* host, 
    const char* port, 
    char* password, 
    int db, 
    redis_on_success on_connect_success,
    redis_on_error on_connect_error,
    redis_on_success on_auth_success,
    redis_on_error on_auth_error) {
    redis_t r = (redis_t) malloc(sizeof(*r));
    if (r == NULL) {
        return NULL;
    }

    r->redis_fd = -1;
    r->authenticated = -1;
    r->on_auth_error = on_auth_error;
    r->on_auth_success = on_auth_success;
    r->on_connect_error = on_connect_error;
    r->on_connect_success = on_connect_success;

    int fd;
    struct addrinfo hints, *addr_info_p;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int address_info = getaddrinfo(
        host, 
        port, 
        &hints, 
        &addr_info_p);

    if (address_info < 0) {
        redis_close(r);
        return NULL;
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        if (r->on_connect_error != NULL) {
            r->on_connect_error(fd);
        }

        redis_close(r);
        return NULL;
    }

    int connected = connect(
        fd, 
        addr_info_p->ai_addr, 
        addr_info_p->ai_addrlen);

    if (connected < 0) {
        if (r->on_connect_error != NULL) {
            r->on_connect_error(connected);
        }

        redis_close(r);
        return NULL;
    }

    // on connect succeed
    if (r->on_connect_success != NULL) {
        r->on_connect_success(NULL);
    }

    // set file descriptor
    r->redis_fd = fd;
    r->service_info = addr_info_p;

    // send auth command
    if (strlen(password) > 0) {
        char base_auth_command[7] = "AUTH %s";
        char auth_command[sizeof(base_auth_command)+strlen(password)-1];
        sprintf(auth_command, base_auth_command, password);
        auth_command[sizeof(auth_command)-1] = 0x0;
        printf("auth_command text length %ld \n", sizeof(auth_command));
        printf("auth_command text %s\n", auth_command);

        // send auth command
        redis_reply_t auth_reply = redis_send_command(r, auth_command);
        if (auth_reply == NULL) {
            if (r->on_auth_error != NULL) {
                r->on_auth_error(-1);
            }

            printf("redis_send_command error \n");
            r->authenticated = -1;
        } else {
            printf("redis reply: %s\n", 
                auth_reply->reply);
            printf("redis reply OK? %d\n", 
                strcmp(REDIS_REPLY_OK, auth_reply->reply));
            printf("redis reply INVALID? %d\n", 
                strcmp(REDIS_AUTH_INVALID, auth_reply->reply));
            if (strcmp(REDIS_REPLY_OK, auth_reply->reply) == 0) {
                r->authenticated = 1;

                // on auth succeed
                if (r->on_auth_success != NULL) {
                    r->on_auth_success((void*) &r->authenticated);
                }

                // if db greater than 0, then select db
                if (db > 0) {
                    printf("select redis db %u \n", (unsigned char) db);

                    // unsigned char can be 3 bytes (0-255), 
                    // so add 2 more memory space should be enough
                    char base_select_command[11] = "SELECT %u";
                    char select_command[sizeof(base_select_command)];
                    int n = sprintf(select_command, base_select_command, (unsigned char) db);
                    if (n < 0) {
                        printf("format select command error %d\n", n);
                    } else {
                        select_command[sizeof(select_command)-1] = 0x0;
                        printf("select_command text length %ld \n", sizeof(select_command));
                        printf("select_command text %s\n", select_command);

                        // send select command
                        redis_reply_t select_reply = redis_send_command(r, select_command);
                        if (select_reply == NULL) {
                            printf("redis_send_command error \n");
                        } else {
                            printf("redis select reply OK? %d\n", 
                                strcmp(REDIS_REPLY_OK, select_reply->reply));
                        }

                        redis_reply_free(select_reply);
                    }
                }
            }
        }

        redis_reply_free(auth_reply);
    }

    return r;
}

redis_reply_t redis_send_command(redis_t r, char* command) {
    if (r == NULL || r->redis_fd < 0) {
        return NULL;
    }

    char base_command[strlen(command) + 2];
    sprintf(base_command, "%s\r\n", command);

    // send data to active socket
    int sent = send(r->redis_fd, 
        (void*) base_command, strlen(base_command), 0);
    if (sent < 0) {
        return NULL;
    }

    char reply[1024];

    // read data to active socket
    int n = recv(r->redis_fd, reply, sizeof(reply), 0);
    if (n < 0) {
        return NULL;
    }

    struct redis_reply* r_reply = (struct redis_reply*) malloc(sizeof(*r_reply));
    if (r_reply == NULL) {
        return NULL;
    }

    r_reply->reply_arr = (char**) malloc(2 * sizeof(char*));
    if (r_reply->reply_arr == NULL) {
        return NULL;
    }

    split_reply(reply, "\r\n", 
        r_reply->reply_arr, &r_reply->reply_arr_size);
    
    printf("r_reply->reply_arr_size %d\n", r_reply->reply_arr_size);

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
        free((void*) reply->reply_arr);
        free((void*) reply->reply);
        free((void*) reply);
    }
}

void redis_close(redis_t r) {
    if (r != NULL) {
        if (r->redis_fd > 0) {
            close(r->redis_fd);
        }

        freeaddrinfo(r->service_info);
        free((void*) r);
    }
}

int split_reply(char* line, char* delim, char** out, int* index_size) {
    char* conf_token = strtok(line, delim);
    int index = 0;
    while (conf_token != NULL) {
        // we only need two line for now
        if (index > 1) break;

        out[index] = conf_token;
        conf_token = strtok(NULL, delim);
        index++;
    }
    
    if (index_size != NULL)
        *index_size = index;

    return 0;
}

char* to_lower(char* s) {
    for(char* p=s; *p; p++) *p=tolower(*p);
    return s;
}

char* to_upper(char* s) {
    for(char* p=s; *p; p++) *p=toupper(*p);
    return s;
}

#endif