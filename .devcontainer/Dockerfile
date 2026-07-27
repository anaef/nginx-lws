FROM mcr.microsoft.com/devcontainers/base:3-ubuntu26.04

ARG NGINX_VERSION=1.28.3
ARG NGINX_PACKAGE_VERSION=1.28.3-2ubuntu1.8
ARG DEBIAN_FRONTEND=noninteractive

SHELL ["/bin/bash", "-o", "pipefail", "-c"]

RUN sed -i 's/^Types: deb$/Types: deb deb-src/' /etc/apt/sources.list.d/ubuntu.sources \
    && apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        curl \
        gdb \
        git \
        jq \
        less \
        liblua5.1-0-dev \
        liblua5.2-dev \
        liblua5.3-dev \
        liblua5.4-dev \
        liblua5.5-dev \
        libpcre2-dev \
        locales \
        lsof \
        lua5.1 \
        lua5.2 \
        lua5.3 \
        lua5.4 \
        lua5.5 \
        nginx=${NGINX_PACKAGE_VERSION} \
        pkg-config \
        procps \
        strace \
        tzdata \
        valgrind \
        vim \
        zlib1g-dev \
    && source_directory="$(mktemp -d)" \
    && cd "${source_directory}" \
    && apt-get source "nginx=${NGINX_PACKAGE_VERSION}" \
    && mv "nginx-${NGINX_VERSION}" "/opt/nginx-${NGINX_VERSION}" \
    && cd / \
    && rm -rf "${source_directory}" \
    && locale-gen en_US.UTF-8 \
    && rm -rf /var/lib/apt/lists/*

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
