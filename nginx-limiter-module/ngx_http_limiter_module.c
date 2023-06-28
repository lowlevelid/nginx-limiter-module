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