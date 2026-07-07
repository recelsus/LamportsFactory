#!/usr/bin/env sh
set -eu

config_root="${NVIM_CONFIG_ROOT:-/app/nvim/config-root}"
config_dir="${config_root}/nvim"
data_home="${NVIM_DATA_HOME:-/app/nvim/data}"
cache_home="${NVIM_CACHE_HOME:-/app/nvim/cache}"
state_home="${NVIM_STATE_HOME:-/app/nvim/state}"
marker="${data_home}/.lamportsfactory-latex-bootstrap"

export XDG_CONFIG_HOME="$config_root"
export XDG_DATA_HOME="$data_home"
export XDG_CACHE_HOME="$cache_home"
export XDG_STATE_HOME="$state_home"

mkdir -p "$config_root" "$data_home" "$cache_home" "$state_home"

if [ ! -d "$config_dir/.git" ]; then
  if [ -z "${NVIM_CONFIG_REPO:-}" ]; then
    echo "NVIM_CONFIG_REPO is not set; skipping nvim config bootstrap" >&2
    exit 0
  fi

  git clone "$NVIM_CONFIG_REPO" "$config_dir"
  if [ -n "${NVIM_CONFIG_REF:-}" ]; then
    git -C "$config_dir" checkout "$NVIM_CONFIG_REF"
  fi
fi

if [ ! -f "$config_dir/init.lua" ] && [ ! -f "$config_dir/init.vim" ]; then
  echo "nvim config was not found; skipping nvim latex bootstrap" >&2
  exit 0
fi

if [ -f "$marker" ] && [ "${NVIM_LATEX_BOOTSTRAP_FORCE:-0}" != "1" ]; then
  exit 0
fi

if [ -f "$config_dir/lazy-lock.json" ]; then
  nvim --headless "+Lazy! restore" +qa
else
  nvim --headless "+Lazy! install" +qa
fi

nvim --headless \
  "+lua require('lazy').load({ plugins = { 'mason.nvim', 'nvim-treesitter' } })" \
  "+MasonInstall texlab" \
  "+TSInstallSync latex bibtex lua" \
  +qa

touch "$marker"
