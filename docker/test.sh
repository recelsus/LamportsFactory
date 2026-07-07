#!/usr/bin/env sh
set -eu

project_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
compose_file="${project_dir}/docker-compose.build.yml"
service_name="lamports-factory"

usage() {
  echo "Usage: ./docker/test.sh {up|down|build --image|build --app}" >&2
}

container_id() {
  docker compose -f "$compose_file" ps -q "$service_name" 2>/dev/null || true
}

is_running() {
  id="$(container_id)"
  [ -n "$id" ]
}

case "$1" in
  up)
    if [ "$#" -ne 1 ]; then
      usage
      exit 2
    fi
    docker compose -f "$compose_file" up -d
    ;;
  down)
    if [ "$#" -ne 1 ]; then
      usage
      exit 2
    fi
    docker compose -f "$compose_file" down
    ;;
  build)
    if [ "$#" -ne 2 ]; then
      usage
      exit 2
    fi
    case "$2" in
      --image)
        if is_running; then
          echo "lamports-factory is running; stop it before image build." >&2
          exit 1
        fi
        docker compose -f "$compose_file" build
        ;;
      --app)
        if ! is_running; then
          echo "lamports-factory is not running; start it before app build." >&2
          exit 1
        fi
        docker compose -f "$compose_file" exec "$service_name" build-server-in-container
        docker compose -f "$compose_file" restart "$service_name"
        ;;
      *)
        usage
        exit 2
        ;;
    esac
    ;;
  *)
    usage
    exit 2
    ;;
esac
