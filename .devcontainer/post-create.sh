#!/usr/bin/env bash
set -euo pipefail

workspace="${LWS_WORKSPACE:-/workspaces/nginx-lws}"
uid="$(id -u)"
gid="$(id -g)"

managed_dirs=(
	"${LWS_NGINX_SOURCE:-/opt/nginx-${NGINX_VERSION:-1.28.3}}"
	"${LWS_NGINX_BUILD:-/opt/nginx-build/normal}"
	"${LWS_NGINX_ASAN_BUILD:-/opt/nginx-build/asan}"
	"${LWS_NGINX_ASAN_PREFIX:-/opt/nginx}"
	"${LWS_RUNTIME:-/opt/nginx-lws}"
	"${LWS_LOG_DIR:-/var/log/nginx-lws}"
	"${LWS_RUN_DIR:-/run/nginx-lws}"
)

for directory in "${managed_dirs[@]}"; do
	sudo install -d -m 0755 -o "${uid}" -g "${gid}" "${directory}"
	sudo chown -R "${uid}:${gid}" "${directory}"
done

aliases_file="${HOME}/.bash_aliases"
if ! grep -q '# nginx-lws devcontainer' "${aliases_file}" 2>/dev/null; then
	cat >> "${aliases_file}" <<'EOF'

# nginx-lws devcontainer
export EDITOR="vim"
export VISUAL="vim"
EOF
fi

"${workspace}/.devcontainer/bin/lws-build" --no-restart
