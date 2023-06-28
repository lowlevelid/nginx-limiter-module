#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

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
    ngx_null_command // command termination
};

// module context
static ngx_http_module_t ngx_http_limiter_ctx = {
    ngx_http_limiter_preconf, // module preconfig
    ngx_http_limiter_postconf, // module postconfig

    NULL, // create main config
    NULL, // init main config

    NULL, // create server config
    NULL, // merge server config

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

// http handler
static ngx_int_t ngx_http_limiter_handler(ngx_http_request_t* r) {

    ngx_buf_t* buf;
    ngx_chain_t out;

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

    return ngx_http_output_filter(r, &out);

}

static char* ngx_http_limiter(ngx_conf_t* cf, ngx_command_t* cmd, void* conf) {
    ngx_http_core_loc_conf_t* clcf;

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_limiter_handler;

    return NGX_CONF_OK;
}