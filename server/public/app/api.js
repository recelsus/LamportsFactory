import { app_url, normalize_base_url } from "./url.js";
import { set_status, set_status_detail } from "./status.js";
import { update_documents, render_document_list } from "./file_tree.js";

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
    state.document_extension = config.document_extension || state.document_extension;
  } catch (_) {
  }
}

export async function load_documents(state, select_document) {
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
    update_documents(state, data.files);
    if (state.document_selected && data.current) {
      state.current_document = data.current;
    }
    render_document_list(state, select_document);
    set_status_detail(state, `documents available: ${state.documents.length}`);
  } catch (error) {
    set_status(state, "error", "error");
    set_status_detail(state, `load files error: ${error.message}`);
  }
}

export async function select_document(state, document_path, callbacks) {
  if (!document_path || document_path === state.current_document) {
    await callbacks.fetch_snapshot();
    return;
  }
  try {
    set_status(state, "switching document…", "building");
    set_status_detail(state, document_path);
    const response = await fetch(app_url(state, `/api/main?document=${encodeURIComponent(document_path)}`), {
      method: "POST",
      credentials: "include",
      cache: "no-store"
    });
    if (!response.ok) {
      throw new Error("switch failed");
    }
    const data = await response.json();
    state.current_document = data.main_document || document_path;
    state.document_selected = true;
    state.last_pdf_mtime = null;
    render_document_list(state, callbacks.select_document);
    callbacks.reload_pdf(Date.now());
    await callbacks.fetch_snapshot();
  } catch (_) {
    set_status(state, "document switch failed", "error");
    set_status_detail(state, "unable to switch preview");
  }
}
