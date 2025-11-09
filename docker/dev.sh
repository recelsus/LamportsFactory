#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
project_dir="$(cd "${script_dir}/.." && pwd)"
compose_file="${script_dir}/docker-compose.dev.yml"
compose_project_dir="${script_dir}"
service_name="lamports-factory"

if ! command -v docker >/dev/null 2>&1; then
  echo "docker command is unavailable" >&2
  exit 1
fi

container_id=""
if docker compose --project-directory "${compose_project_dir}" -f "${compose_file}" ps -q "${service_name}" >/dev/null 2>&1; then
  container_id="$(docker compose --project-directory "${compose_project_dir}" -f "${compose_file}" ps -q "${service_name}")"
fi

if [ -n "${container_id}" ]; then
  read -r -p "lamports-factory is running. Stop it now? [y/n]: " user_choice
  case "${user_choice}" in
    y|Y)
      docker compose --project-directory "${compose_project_dir}" -f "${compose_file}" down
      ;;
    *)
      echo "Kept running."
      ;;
  esac
else
  read -r -p "lamports-factory is idle. Build and start it? [y/n]: " user_choice
  case "${user_choice}" in
    y|Y)
      docker compose --project-directory "${compose_project_dir}" -f "${compose_file}" up -d --build
      ;;
    *)
      echo "Left idle."
      ;;
  esac
fi
