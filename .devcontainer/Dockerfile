FROM mcr.microsoft.com/devcontainers/base:1-ubuntu-24.04

ARG NGINX_VERSION=1.24.0
ARG NGINX_SHA256=77a2541637b92a621e3ee76776c8b7b40cf6d707e69ba53a940283e30ff2f55d
ARG DEBIAN_FRONTEND=noninteractive

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        curl \
        gdb \
        git \
        jq \
        less \
        liblua5.4-dev \
        libpcre2-dev \
        locales \
        lsof \
        lua5.4 \
        nginx \
        pkg-config \
        procps \
        strace \
        tzdata \
        valgrind \
        vim \
        zlib1g-dev \
    && locale-gen en_US.UTF-8 \
    && rm -rf /var/lib/apt/lists/*

RUN curl --fail --location --silent --show-error \
        --output "/tmp/nginx-${NGINX_VERSION}.tar.gz" \
        "https://nginx.org/download/nginx-${NGINX_VERSION}.tar.gz" \
    && echo "${NGINX_SHA256}  /tmp/nginx-${NGINX_VERSION}.tar.gz" | sha256sum --check --strict \
    && tar --extract --gzip --file "/tmp/nginx-${NGINX_VERSION}.tar.gz" --directory /opt \
    && rm "/tmp/nginx-${NGINX_VERSION}.tar.gz"

ENV LANG=en_US.UTF-8 \
    LANGUAGE=en_US:en \
    LC_ALL=en_US.UTF-8 \
    LUA_VERSION=5.4 \
    NGINX_VERSION=${NGINX_VERSION} \
    LWS_WORKSPACE=/workspaces/nginx-lws \
    LWS_NGINX_SOURCE=/opt/nginx-${NGINX_VERSION} \
    LWS_NGINX_BUILD=/opt/nginx-build/normal \
    LWS_NGINX_ASAN_BUILD=/opt/nginx-build/asan \
    LWS_NGINX_ASAN_PREFIX=/opt/nginx \
    LWS_RUNTIME=/opt/nginx-lws \
    LWS_LOG_DIR=/var/log/nginx-lws \
    LWS_RUN_DIR=/run/nginx-lws \
    PATH=/workspaces/nginx-lws/.devcontainer/bin:${PATH}
