#!/usr/bin/env sh
set -eu

source_dir="${LF_SOURCE_DIR:-/src}"
build_dir="${LF_BUILD_DIR:-/app/dev/build}"
install_dir="${LF_DEV_BIN_DIR:-/app/dev/bin}"
build_type="${CMAKE_BUILD_TYPE:-Debug}"
jobs="${BUILD_JOBS:-$(nproc)}"

if [ ! -f "${source_dir}/server/CMakeLists.txt" ]; then
  echo "server/CMakeLists.txt was not found under ${source_dir}" >&2
  exit 1
fi

mkdir -p "$build_dir" "$install_dir"

cmake -S "${source_dir}/server" -B "$build_dir" -DCMAKE_BUILD_TYPE="$build_type"
cmake --build "$build_dir" --target lf -j "$jobs"
install -m 0755 "${build_dir}/lf" "${install_dir}/lamportsfactory"

echo "installed ${install_dir}/lamportsfactory"
