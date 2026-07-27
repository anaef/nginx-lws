#!/usr/bin/env bash

LWS_WORKSPACE="${LWS_WORKSPACE:-/workspaces/nginx-lws}"
NGINX_VERSION="${NGINX_VERSION:-1.28.3}"
LWS_NGINX_SOURCE="${LWS_NGINX_SOURCE:-/opt/nginx-${NGINX_VERSION}}"
LWS_NGINX_BUILD="${LWS_NGINX_BUILD:-/opt/nginx-build/normal}"
LWS_NGINX_ASAN_BUILD="${LWS_NGINX_ASAN_BUILD:-/opt/nginx-build/asan}"
LWS_NGINX_ASAN_PREFIX="${LWS_NGINX_ASAN_PREFIX:-/opt/nginx}"
LWS_RUNTIME="${LWS_RUNTIME:-/opt/nginx-lws}"
LWS_LOG_DIR="${LWS_LOG_DIR:-/var/log/nginx-lws}"
LWS_RUN_DIR="${LWS_RUN_DIR:-/run/nginx-lws}"

LWS_NORMAL_NGINX="/usr/sbin/nginx"
LWS_NORMAL_CONF="${LWS_WORKSPACE}/.devcontainer/nginx.dev.conf"
LWS_ASAN_CONF_SOURCE="${LWS_WORKSPACE}/.devcontainer/nginx.asan.conf"
LWS_NORMAL_MODULE="${LWS_RUNTIME}/modules/lws_module.so"
LWS_NORMAL_PID="${LWS_RUN_DIR}/nginx.pid"

lws_prepare_runtime_dirs() {
	mkdir -p \
		"${LWS_NGINX_BUILD}" \
		"${LWS_NGINX_ASAN_BUILD}" \
		"${LWS_NGINX_ASAN_PREFIX}" \
		"${LWS_RUNTIME}/modules" \
		"${LWS_LOG_DIR}" \
		"${LWS_RUN_DIR}/client_body" \
		"${LWS_RUN_DIR}/fastcgi" \
		"${LWS_RUN_DIR}/proxy" \
		"${LWS_RUN_DIR}/scgi" \
		"${LWS_RUN_DIR}/uwsgi"

	touch "${LWS_LOG_DIR}/access.log" "${LWS_LOG_DIR}/error.log"
}
