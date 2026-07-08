#!/usr/bin/env sh
set -eu

normalize_base_url() {
  value="${1:-/}"
  case "$value" in
    "") value="/" ;;
    /*) ;;
    *) value="/$value" ;;
  esac
  if [ "$value" != "/" ]; then
    value="${value%/}"
  fi
  printf '%s' "$value"
}

is_enabled() {
  case "$(printf '%s' "${1:-}" | tr '[:upper:]' '[:lower:]')" in
    enable|enabled|1|true|yes|on) return 0 ;;
    *) return 1 ;;
  esac
}

BASE_URL="$(normalize_base_url "${BASE_URL:-/}")"
export BASE_URL

APP_INTERNAL_ADDR="${APP_INTERNAL_ADDR:-127.0.0.1}"
APP_INTERNAL_PORT="${APP_INTERNAL_PORT:-18080}"
TTYD_INTERNAL_ADDR="${TTYD_INTERNAL_ADDR:-127.0.0.1}"
TTYD_INTERNAL_PORT="${TTYD_INTERNAL_PORT:-17681}"
TTYD_TMUX_SESSION="${TTYD_TMUX_SESSION:-lamportsfactory-main}"
TTYD_FONT_FAMILY="${TTYD_FONT_FAMILY:-JetBrainsMono Nerd Font, Symbols Nerd Font Mono, monospace}"
REVERSE_PROXY_PORT="${REVERSE_PROXY_PORT:-8080}"
TTYD_ENABLED="${TTYD_ENABLED:-enable}"
WORKSPACE_DIR="${WORKSPACE_DIR:-/app/workspace}"
COMPILER_BACKEND="$(printf '%s' "${COMPILER_BACKEND:-latex}" | tr '[:upper:]' '[:lower:]')"
case "$COMPILER_BACKEND" in
  typst)
    MAIN_DOCUMENT="${MAIN_DOCUMENT:-sample/main.typ}"
    DOCUMENT_EXTENSION="${DOCUMENT_EXTENSION:-.typ}"
    ;;
  *)
    MAIN_DOCUMENT="${MAIN_DOCUMENT:-sample/main.tex}"
    DOCUMENT_EXTENSION="${DOCUMENT_EXTENSION:-.tex}"
    ;;
esac
TEXMFHOME="${TEXMFHOME:-/app/texmf}"
OSFONTDIR="${OSFONTDIR:-/app/fonts}"

export SERVER_ADDR="$APP_INTERNAL_ADDR"
export SERVER_PORT="$APP_INTERNAL_PORT"
export COMPILER_BACKEND
export MAIN_DOCUMENT
export DOCUMENT_EXTENSION
export TEXMFHOME
export OSFONTDIR
export TERM="${TERM:-xterm-256color}"
export COLORTERM="${COLORTERM:-truecolor}"
export LANG="${LANG:-C.UTF-8}"
export LC_ALL="${LC_ALL:-C.UTF-8}"
export XDG_CONFIG_HOME="${NVIM_CONFIG_ROOT:-/app/nvim/config-root}"
export XDG_DATA_HOME="${NVIM_DATA_HOME:-/app/nvim/data}"
export XDG_CACHE_HOME="${NVIM_CACHE_HOME:-/app/nvim/cache}"
export XDG_STATE_HOME="${NVIM_STATE_HOME:-/app/nvim/state}"

mkdir -p "$WORKSPACE_DIR" "$TEXMFHOME" "$OSFONTDIR" "$XDG_CONFIG_HOME" "$XDG_DATA_HOME" "$XDG_CACHE_HOME" "$XDG_STATE_HOME"

fc-cache -f "$OSFONTDIR" >/dev/null 2>&1 || true
mktexlsr "$TEXMFHOME" >/dev/null 2>&1 || true

if is_enabled "${NVIM_BOOTSTRAP_ON_INIT:-enable}"; then
  /usr/local/bin/nvim-bootstrap || echo "nvim bootstrap failed; continuing" >&2
fi

terminal_path="${BASE_URL}/terminal/"
if [ "$BASE_URL" = "/" ]; then
  app_location="/"
  terminal_path="/terminal/"
else
  app_location="${BASE_URL}/"
fi

cat > /tmp/nginx.conf <<EOF
worker_processes 1;
pid /tmp/nginx.pid;
error_log /dev/stderr warn;

events {
  worker_connections 1024;
}

http {
  access_log /dev/stdout;
  client_body_temp_path /tmp/nginx-client-body;
  proxy_temp_path /tmp/nginx-proxy;
  fastcgi_temp_path /tmp/nginx-fastcgi;
  uwsgi_temp_path /tmp/nginx-uwsgi;
  scgi_temp_path /tmp/nginx-scgi;

  map \$http_upgrade \$connection_upgrade {
    default upgrade;
    '' close;
  }

  server {
    listen ${REVERSE_PROXY_PORT};
    server_name _;

    location ${terminal_path} {
      proxy_http_version 1.1;
      proxy_set_header Upgrade \$http_upgrade;
      proxy_set_header Connection \$connection_upgrade;
      proxy_set_header Host \$host;
      proxy_set_header X-Real-IP \$remote_addr;
      proxy_set_header X-Forwarded-For \$proxy_add_x_forwarded_for;
      proxy_set_header X-Forwarded-Proto \$scheme;
      proxy_pass http://${TTYD_INTERNAL_ADDR}:${TTYD_INTERNAL_PORT}${terminal_path};
    }

    location ${app_location} {
      proxy_http_version 1.1;
      proxy_set_header Host \$host;
      proxy_set_header X-Real-IP \$remote_addr;
      proxy_set_header X-Forwarded-For \$proxy_add_x_forwarded_for;
      proxy_set_header X-Forwarded-Proto \$scheme;
      proxy_pass http://${APP_INTERNAL_ADDR}:${APP_INTERNAL_PORT};
    }
  }
}
EOF

children=""

lamportsfactory &
children="$children $!"

if is_enabled "$TTYD_ENABLED"; then
cat > /tmp/lamportsfactory-ttyd-shell.sh <<EOF
#!/usr/bin/env sh
set -eu
cd "$WORKSPACE_DIR" || exit 1
export TERM="\${TERM:-xterm-256color}"
export COLORTERM="\${COLORTERM:-truecolor}"
export LANG="\${LANG:-C.UTF-8}"
export LC_ALL="\${LC_ALL:-C.UTF-8}"
exec tmux -f /etc/lamportsfactory-tmux.conf new-session -A -s "$TTYD_TMUX_SESSION" -c "$WORKSPACE_DIR" "bash -l"
EOF
  chmod 0755 /tmp/lamportsfactory-ttyd-shell.sh
  ttyd -i "$TTYD_INTERNAL_ADDR" -p "$TTYD_INTERNAL_PORT" -b "$terminal_path" -W -w "$WORKSPACE_DIR" -t "fontFamily=$TTYD_FONT_FAMILY" /tmp/lamportsfactory-ttyd-shell.sh &
  children="$children $!"
fi

cleanup() {
  for pid in $children; do
    kill "$pid" >/dev/null 2>&1 || true
  done
}
trap cleanup INT TERM EXIT

nginx -c /tmp/nginx.conf -g 'daemon off;' &
nginx_pid="$!"
children="$children $nginx_pid"

wait "$nginx_pid"
