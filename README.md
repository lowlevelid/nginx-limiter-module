### Nginx Rate Limiter

Experimental Nginx Rate Limit Module

[![Nginx HTTP Limiter module CI](https://github.com/lowlevelid/nginx-limiter-module/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/lowlevelid/nginx-limiter-module/actions/workflows/ci.yml)

#### Update and Install Dependencies for NGINX

- install development libraries along with source code compilers.
```shell
sudo apt-get update 
sudo apt-get install build-essential libpcre3 libpcre3-dev zlib1g zlib1g-dev libssl-dev libgd-dev libxml2 libxml2-dev uuid-dev
```

- Download NGINX Source Code, Configure and Adding Modules
```shell
./scripts/nginx
```

- Start NGINX
```shell
./nginx/install/sbin/nginx
```

- visit http://localhost:8090/test-rate-limit