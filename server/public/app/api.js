import { app_url, normalize_base_url } from "./url.js";
import { set_status, set_status_detail } from "./status.js";
import { update_tex_files, render_tex_list } from "./file_tree.js";

export async function load_config(state) {
  try {
    const response = await fetch(app_url(state, `/api/config`), {
      credentials: "include",
      cache: "no-store"
    });
    if (!response.ok) {
      return;
    }
    const config = await response.json();
    state.base_url = normalize_base_url(config.base_url || state.base_url);
    state.reload_mode = config.reload_mode || state.reload_mode;
    state.reload_interval_ms = config.reload_interval_ms || state.reload_interval_ms;
    state.reload_retry_ms = config.reload_retry_ms || state.reload_retry_ms;
    state.layout_mode = config.layout_mode || state.layout_mode;
    state.ttyd_enabled = Boolean(config.ttyd_enabled);
    state.ttyd_url = config.ttyd_url || state.ttyd_url;
  } catch (_) {
  }
}

export async function load_tex_files(state, select_tex_document) {
  try {
    const response = await fetch(app_url(state, `/api/files`), {
      credentials: "include",
      cache: "no-store"
    });
    if (!response.ok) {
      set_status(state, "error", "error");
      set_status_detail(state, `failed to load files (${response.status})`);
      return;
    }
    const data = await response.json();
    console.debug("files", data);
    update_tex_files(state, data.files);
    if (data.current) {
      state.current_main = data.current;
    }
    render_tex_list(state, select_tex_document);
    set_status_detail(state, `documents available: ${state.tex_files.length}`);
  } catch (error) {
    set_status(state, "error", "error");
    set_status_detail(state, `load files error: ${error.message}`);
  }
}

export async function select_tex_document(state, tex_path, callbacks) {
  if (!tex_path || tex_path === state.current_main) {
    await callbacks.fetch_snapshot();
    return;
  }
  try {
    set_status(state, "switching document…", "building");
    set_status_detail(state, tex_path);
    const response = await fetch(app_url(state, `/api/main?tex=${encodeURIComponent(tex_path)}`), {
      method: "POST",
      credentials: "include",
      cache: "no-store"
    });
    if (!response.ok) {
      throw new Error("switch failed");
    }
    const data = await response.json();
    state.current_main = data.tex_main || tex_path;
    state.last_pdf_mtime = null;
    render_tex_list(state, callbacks.select_tex_document);
    callbacks.reload_pdf(Date.now());
    await callbacks.fetch_snapshot();
  } catch (_) {
    set_status(state, "document switch failed", "error");
    set_status_detail(state, "unable to switch preview");
  }
}
