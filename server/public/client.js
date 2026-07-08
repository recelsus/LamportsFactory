import { app_state } from "./app/state.js";
import { set_status, set_status_detail } from "./app/status.js";
import { stored_layout_mode, stored_layout_order, setup_layout_toggle, apply_layout } from "./app/layout.js";
import { setup_pdf_viewer, reload_pdf as reload_pdf_viewer } from "./app/pdf_preview.js";
import { load_config, load_documents, select_document as select_document } from "./app/api.js";
import { fetch_snapshot as fetch_state_snapshot, setup_channel } from "./app/reload_channel.js";

function bind_elements() {
  app_state.status_bar = document.getElementById("status-bar");
  app_state.status_detail = document.getElementById("status-detail");
  app_state.pdf_canvas = document.getElementById("pdf-canvas");
  app_state.pdf_toolbar = document.getElementById("pdf-toolbar");
  app_state.pdf_page_label = document.getElementById("pdf-page-label");
  app_state.pdf_zoom_label = document.getElementById("pdf-zoom-label");
  app_state.terminal_pane = document.getElementById("terminal-pane");
  app_state.terminal_frame = document.getElementById("terminal-frame");
  app_state.main_area = document.querySelector(".main-area");
  app_state.layout_toggle = document.getElementById("layout-toggle");
  app_state.layout_swap_button = document.getElementById("layout-swap-button");
  app_state.refresh_button = document.getElementById("refresh-button");
  app_state.document_list = document.getElementById("document-list");
}

function create_callbacks() {
  const callbacks = {};
  callbacks.reload_pdf = (token) => reload_pdf_viewer(app_state, token);
  callbacks.fetch_snapshot = () => fetch_state_snapshot(app_state, callbacks);
  callbacks.load_documents = () => load_documents(app_state, callbacks.select_document);
  callbacks.select_document = (document_path) => select_document(app_state, document_path, callbacks);
  return callbacks;
}

function setup_refresh_button(callbacks) {
  if (!app_state.refresh_button) {
    return;
  }
  app_state.refresh_button.addEventListener("click", async () => {
    await callbacks.load_documents();
    await callbacks.fetch_snapshot();
  });
}

async function initialize() {
  bind_elements();
  const callbacks = create_callbacks();
  set_status(app_state, "loading…", "idle");
  set_status_detail(app_state, "fetching configuration");
  await load_config(app_state);
  app_state.layout_mode = stored_layout_mode() || app_state.layout_mode;
  app_state.layout_order = stored_layout_order() || app_state.layout_order;
  setup_layout_toggle(app_state);
  setup_refresh_button(callbacks);
  await setup_pdf_viewer(app_state);
  apply_layout(app_state);
  await callbacks.load_documents();
  await callbacks.fetch_snapshot();
  setup_channel(app_state, callbacks);
  if (app_state.sync_timer) {
    window.clearInterval(app_state.sync_timer);
  }
  app_state.sync_timer = window.setInterval(
    callbacks.fetch_snapshot,
    Math.max(2000, app_state.reload_interval_ms)
  );
}

document.addEventListener("DOMContentLoaded", initialize);
