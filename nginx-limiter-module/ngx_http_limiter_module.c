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

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#include "redis.h"

struct ngx_http_limiter_srv_conf_s {
    // redis config
    ngx_str_t host;
    ngx_str_t port;
    ngx_str_t pass;
    ngx_uint_t db;

    // limiter maximum
    ngx_uint_t max;
};

typedef struct ngx_http_limiter_srv_conf_s ngx_http_limiter_srv_conf_t;

static void* ngx_http_limiter_create_srv_conf(ngx_conf_t* cf);
static char* ngx_http_limiter_merge_srv_conf(ngx_conf_t* cf, void* parent, void* child);

static char* ngx_http_limiter(ngx_conf_t* cf, ngx_command_t* cmd, void* conf);
static ngx_int_t ngx_http_limiter_handler(ngx_http_request_t* r);
static ngx_int_t ngx_http_limiter_preconf(ngx_conf_t *cf);
static ngx_int_t ngx_http_limiter_postconf(ngx_conf_t *cf);

// module directive
static ngx_command_t ngx_http_limiter_commands[] = {
    {
        ngx_string("limiter"), // directive
        NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,

        ngx_http_limiter, // configuration setup function
        0,
        0,
        NULL,
    },
    {
        ngx_string("limiter_redis_host"), // directive
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,

        ngx_conf_set_str_slot, // configuration setup function
        NGX_HTTP_SRV_CONF_OFFSET,
        offsetof(ngx_http_limiter_srv_conf_t, host),
        NULL,
    },
    {
        ngx_string("limiter_redis_port"), // directive
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,

        ngx_conf_set_str_slot, // configuration setup function
        NGX_HTTP_SRV_CONF_OFFSET,
        offsetof(ngx_http_limiter_srv_conf_t, port),
        NULL,
    },
    {
        ngx_string("limiter_redis_pass"), // directive
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,

        ngx_conf_set_str_slot, // configuration setup function
        NGX_HTTP_SRV_CONF_OFFSET,
        offsetof(ngx_http_limiter_srv_conf_t, pass),
        NULL,
    },
    {
        ngx_string("limiter_redis_db"), // directive
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,

        ngx_conf_set_num_slot, // configuration setup function
        NGX_HTTP_SRV_CONF_OFFSET,
        offsetof(ngx_http_limiter_srv_conf_t, db),
        NULL,
    },
    {
        ngx_string("limiter_redis_max"), // directive
        NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_CONF_TAKE1,

        ngx_conf_set_num_slot, // configuration setup function
        NGX_HTTP_SRV_CONF_OFFSET,
        offsetof(ngx_http_limiter_srv_conf_t, max),
        NULL,
    },
    ngx_null_command // command termination
};

// module context
static ngx_http_module_t ngx_http_limiter_ctx = {
    ngx_http_limiter_preconf, // module preconfig
    ngx_http_limiter_postconf, // module postconfig

    NULL, // create main config
    NULL, // init main config

    ngx_http_limiter_create_srv_conf, // create server config
    ngx_http_limiter_merge_srv_conf, // merge server config

    NULL, // create location config
    NULL, // merge location config
};

// module definition
ngx_module_t ngx_http_limiter_module = {
    NGX_MODULE_V1,
    &ngx_http_limiter_ctx, // module context
    ngx_http_limiter_commands, // module directive
    NGX_HTTP_MODULE, // module type
    NULL, // init master
    NULL, // init module
    NULL, // init process
    NULL, // init thread
    NULL, // exit thread
    NULL, // exit process
    NULL, // init master
    NGX_MODULE_V1_PADDING
};

// http handler
static ngx_int_t ngx_http_limiter_handler(ngx_http_request_t* r) {

    ngx_buf_t* buf;
    ngx_chain_t out;

    ngx_http_limiter_srv_conf_t* limiter_srv_conf;

    // get limiter server conf
    limiter_srv_conf = ngx_http_get_module_srv_conf(r, ngx_http_limiter_module);
    printf("limiter_srv_conf host: %s\n", (const char*) limiter_srv_conf->host.data);
    printf("limiter_srv_conf port: %s\n", (const char*) limiter_srv_conf->port.data);
    printf("limiter_srv_conf pass: %s\n", (const char*) limiter_srv_conf->pass.data);
    printf("limiter_srv_conf db: %ld\n", limiter_srv_conf->db);
    printf("limiter_srv_conf max: %ld\n", limiter_srv_conf->max);

    // get user agent
    ngx_str_t user_agent = r->headers_in.user_agent->value;
    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, (const char*) user_agent.data);
    printf("user-agent: %s\n", user_agent.data);
    printf("user-agent-len: %ld\n", user_agent.len);

    printf("method: %ld\n", r->method);
    printf("method name: %s\n", (const char*) r->method_name.data);
    if (!(r->method & NGX_HTTP_GET)) {
        return NGX_HTTP_NOT_ALLOWED;
    }

    printf("x_forwarded_for N: %ld\n", r->headers_in.x_forwarded_for.nelts);

    char client_ipstr[INET6_ADDRSTRLEN];
    inet_ntop(r->connection->sockaddr->sa_family, r->connection->sockaddr, 
        client_ipstr, sizeof(client_ipstr));
    printf("user: %s\n", client_ipstr);


    char content_type[17] = "application/json";
    content_type[16] = 0x0;

    // set content type header
    r->headers_out.content_type.len = sizeof(content_type) - 1;
    r->headers_out.content_type.data = (u_char*) content_type;

    buf = ngx_palloc(r->pool, sizeof(ngx_buf_t));

    out.buf = buf;
    out.next = NULL;

    char* data = ngx_palloc(r->pool, sizeof(*data) * 6);
    if (data == NULL) {
        ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "failed to allocate data response");
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    data = "hello";

    char base_resp[32] = "{\"success\": true, \"data\": \"\%s\"}";
    char json_resp[35];

    sprintf(json_resp, base_resp, data);
    json_resp[34] = 0x0;

    buf->pos = (u_char*) json_resp; // first position in memory of the data
    buf->last = (u_char*) json_resp + sizeof(json_resp) - 1; // last position in memory of the data
    buf->memory = 1; // read only memory
    buf->last_buf = 1; // will no more buffers in the request

    r->headers_out.status = NGX_HTTP_OK;
    r->headers_out.content_length_n = sizeof(json_resp) - 1;

    ngx_http_send_header(r); // send headers

    // redis
    struct redis* redis = redis_connect((char*) limiter_srv_conf->host.data, 
        (char*) limiter_srv_conf->port.data, (char*) limiter_srv_conf->pass.data);
    if (redis == NULL) {
        printf("redis init failed\n");
    }

    printf("redis authenticated: %d\n", redis->authenticated);

    redis_close(redis);

    return ngx_http_output_filter(r, &out);

}

static char* ngx_http_limiter(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
    ngx_http_core_loc_conf_t* clcf;

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_limiter_handler;

    return NGX_CONF_OK;
}

// module preconfig
static ngx_int_t ngx_http_limiter_preconf(ngx_conf_t *cf) {
    ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "preconfig run srand()");
    return 0;
}

// module postconfig
static ngx_int_t ngx_http_limiter_postconf(ngx_conf_t *cf) {
    ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "postconfig closing...");
    return 0;
}

// module server create config
static void* ngx_http_limiter_create_srv_conf(ngx_conf_t* cf) {
    ngx_log_debug0(NGX_LOG_INFO, cf->log, 0, "limiter module: create server conf");

    ngx_http_limiter_srv_conf_t* conf;

    conf = ngx_pcalloc(cf->pool, sizeof(*conf));
    if (conf == NULL) {
        return NGX_CONF_ERROR;
    }

    conf->db = NGX_CONF_UNSET_UINT;
    conf->max = NGX_CONF_UNSET_UINT;

    return conf;
}

// module server merge config
static char* ngx_http_limiter_merge_srv_conf(ngx_conf_t* cf, void* parent, void* child) {
    ngx_http_limiter_srv_conf_t* prev = parent;
    ngx_http_limiter_srv_conf_t* conf = child;

    ngx_log_debug0(NGX_LOG_INFO, cf->log, 0, "limiter module: merge server conf");

    ngx_conf_merge_str_value(conf->host, prev->host, NULL);
    ngx_conf_merge_str_value(conf->port, prev->port, NULL);
    ngx_conf_merge_str_value(conf->pass, prev->pass, NULL);
    
    ngx_conf_merge_uint_value(conf->db, prev->db, 0);
    ngx_conf_merge_uint_value(conf->max, prev->max, 1);

    if (conf->max < 1) {
        ngx_conf_log_error(NGX_LOG_ERR, cf, 0, "limiter max config must bre greater than 0");
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}