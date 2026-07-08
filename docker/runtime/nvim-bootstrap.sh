#!/usr/bin/env sh
set -eu

config_root="${NVIM_CONFIG_ROOT:-/app/nvim/config-root}"
config_dir="${config_root}/nvim"
data_home="${NVIM_DATA_HOME:-/app/nvim/data}"
cache_home="${NVIM_CACHE_HOME:-/app/nvim/cache}"
state_home="${NVIM_STATE_HOME:-/app/nvim/state}"
backend="$(printf '%s' "${COMPILER_BACKEND:-latex}" | tr '[:upper:]' '[:lower:]')"
marker="${data_home}/.lamportsfactory-${backend}-bootstrap"
fzf_native_dir="${data_home}/nvim/lazy/telescope-fzf-native.nvim"
fzf_native_lib="${fzf_native_dir}/build/libfzf.so"

require_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "required command was not found for nvim bootstrap: $1" >&2
    exit 1
  fi
}

check_backend_tools() {
  require_command git
  require_command nvim
  require_command make
  require_command cc
  require_command tree-sitter
  case "$backend" in
    latex)
      require_command latexmk
      ;;
    typst)
      require_command typst
      ;;
    *)
      echo "no nvim bootstrap preflight profile for backend: $backend" >&2
      ;;
  esac
}

build_telescope_fzf_native() {
  if [ ! -d "$fzf_native_dir" ]; then
    return 0
  fi
  if [ -f "$fzf_native_lib" ]; then
    return 0
  fi
  echo "building telescope-fzf-native.nvim" >&2
  make -C "$fzf_native_dir"
}

install_treesitter_parsers() {
  TS_LANGS="$1" nvim --headless \
    "+lua require('lazy').load({ plugins = { 'nvim-treesitter' } })" \
    "+lua local langs = vim.split(vim.env.TS_LANGS or '', ' ', { trimempty = true }); local ok, ts = pcall(require, 'nvim-treesitter'); if ok and type(ts.install) == 'function' then local task = ts.install(langs, { summary = true }); if type(task) == 'table' and type(task.wait) == 'function' then local success = task:wait(300000); if success == false then error('nvim-treesitter parser installation failed') end end elseif vim.fn.exists(':TSInstallSync') == 2 then vim.cmd('TSInstallSync ' .. table.concat(langs, ' ')) elseif vim.fn.exists(':TSInstall') == 2 then vim.cmd('TSInstall ' .. table.concat(langs, ' ')) else error('nvim-treesitter install API was not found') end" \
    +qa
}

export XDG_CONFIG_HOME="$config_root"
export XDG_DATA_HOME="$data_home"
export XDG_CACHE_HOME="$cache_home"
export XDG_STATE_HOME="$state_home"

check_backend_tools

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
  echo "nvim config was not found; skipping nvim bootstrap" >&2
  exit 0
fi

if [ -f "$marker" ] && [ "${NVIM_BOOTSTRAP_FORCE:-0}" != "1" ] && [ -f "$fzf_native_lib" ]; then
  exit 0
fi

if [ -f "$config_dir/lazy-lock.json" ]; then
  nvim --headless "+Lazy! restore" +qa
else
  nvim --headless "+Lazy! install" +qa
fi

nvim --headless \
  "+lua require('lazy').load({ plugins = { 'mason.nvim', 'nvim-treesitter' } })" \
  "+lua assert(pcall(require, 'mason'), 'mason.nvim is not available')" \
  "+lua assert(pcall(require, 'nvim-treesitter'), 'nvim-treesitter is not available')" \
  +qa

build_telescope_fzf_native

case "$backend" in
  latex)
    nvim --headless \
      "+lua require('lazy').load({ plugins = { 'mason.nvim' } })" \
      "+MasonInstall texlab" \
      +qa
    install_treesitter_parsers "latex bibtex lua"
    ;;
  typst)
    nvim --headless \
      "+lua require('lazy').load({ plugins = { 'mason.nvim' } })" \
      "+MasonInstall tinymist" \
      +qa
    install_treesitter_parsers "typst lua"
    ;;
  *)
    echo "no nvim bootstrap profile for backend: $backend; skipping backend tools" >&2
    ;;
esac

touch "$marker"
