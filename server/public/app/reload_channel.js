import { app_url } from "./url.js";
import { set_status, set_status_detail, format_time_label } from "./status.js";
import { update_documents, render_document_list } from "./file_tree.js";

function parse_json(text) {
  try {
    return JSON.parse(text);
  } catch (_) {
    return null;
  }
}

export async function fetch_snapshot(state, callbacks) {
  try {
    const response = await fetch(app_url(state, `/api/mtime`), {
      credentials: "include",
      cache: "no-store"
    });
    if (!response.ok) {
      return;
    }
    const snapshot = await response.json();
    console.debug("snapshot", snapshot);
    update_documents(state, snapshot.files);
    if (snapshot.main_document && snapshot.main_document !== state.current_document) {
      state.current_document = snapshot.main_document;
      render_document_list(state, callbacks.select_document);
    }
    if (typeof snapshot.pdf_mtime === "number" && snapshot.pdf_mtime !== state.last_pdf_mtime) {
      state.last_pdf_mtime = snapshot.pdf_mtime;
      callbacks.reload_pdf(snapshot.pdf_mtime);
    }
    if (snapshot.building) {
      set_status(state, "building…", "building");
      const queued = snapshot.queued || 0;
      set_status_detail(state, queued > 0 ? `running build (queued ${queued})` : "running build");
      return;
    }
    if (snapshot.last_success && typeof snapshot.pdf_mtime === "number") {
      set_status(state, `updated ${format_time_label(snapshot.pdf_mtime)}`, "idle");
      set_status_detail(state, "latest PDF delivered");
      return;
    }
    if (!snapshot.last_success && snapshot.last_error) {
      set_status(state, "build failed", "error");
      set_status_detail(state, snapshot.last_error);
      return;
    }
    set_status(state, "idle", "idle");
    set_status_detail(state, "waiting for changes");
    await callbacks.load_documents();
  } catch (_) {
  }
}

function dispatch_event(state, name, payload, callbacks) {
  switch (name) {
    case "build_start":
      if (payload && payload.main_document) {
        state.current_document = payload.main_document;
        render_document_list(state, callbacks.select_document);
      }
      set_status(state, "building…", "building");
      set_status_detail(state, payload && payload.queued > 0
        ? `running build (queued ${payload.queued})`
        : "running build");
      break;
    case "build_ok": {
      if (payload && payload.main_document) {
        state.current_document = payload.main_document;
        render_document_list(state, callbacks.select_document);
      }
      const mtime = payload && typeof payload.pdf_mtime === "number"
        ? payload.pdf_mtime
        : Date.now();
      state.last_pdf_mtime = mtime;
      callbacks.reload_pdf(mtime);
      set_status(state, `updated ${format_time_label(mtime)}`, "idle");
      set_status_detail(state, `duration ${payload && payload.duration_ms ? payload.duration_ms : 0} ms`);
      break;
    }
    case "build_fail":
      if (payload && payload.main_document) {
        state.current_document = payload.main_document;
        render_document_list(state, callbacks.select_document);
      }
      set_status(state, "build failed", "error");
      set_status_detail(state, payload && payload.message ? payload.message : "build failed");
      break;
    case "pdf_updated":
      if (!payload || typeof payload.pdf_mtime !== "number") {
        break;
      }
      if (payload.main_document) {
        state.current_document = payload.main_document;
        render_document_list(state, callbacks.select_document);
      }
      state.last_pdf_mtime = payload.pdf_mtime;
      callbacks.reload_pdf(payload.pdf_mtime);
      break;
    default:
      break;
  }
}

function setup_sse(state, callbacks) {
  if (state.event_source) {
    state.event_source.close();
  }
  const source = new EventSource(app_url(state, `/events`), { withCredentials: true });
  ["build_start", "build_ok", "build_fail", "pdf_updated"].forEach((name) => {
    source.addEventListener(name, (event) => {
      dispatch_event(state, name, parse_json(event.data), callbacks);
    });
  });
  source.addEventListener("ping", () => {});
  source.onerror = () => {
    set_status(state, "connection lost", "error");
    set_status_detail(state, "retrying SSE connection");
    source.close();
    state.event_source = null;
    window.setTimeout(() => setup_channel(state, callbacks), state.reload_retry_ms);
  };
  state.event_source = source;
}

function setup_websocket(state, callbacks) {
  if (state.websocket) {
    state.websocket.close();
  }
  const protocol = window.location.protocol === "https:" ? "wss" : "ws";
  const socket = new WebSocket(`${protocol}://${window.location.host}${app_url(state, "/ws")}`);
  socket.onmessage = (event) => {
    const payload = parse_json(event.data);
    if (payload) {
      dispatch_event(state, payload.event || payload.type, payload.data || payload, callbacks);
    }
  };
  socket.onopen = () => {
    set_status(state, "connected", "idle");
    set_status_detail(state, "watching for changes");
  };
  socket.onclose = () => {
    set_status(state, "connection lost", "error");
    set_status_detail(state, "retrying WebSocket connection");
    state.websocket = null;
    window.setTimeout(() => setup_channel(state, callbacks), state.reload_retry_ms);
  };
  socket.onerror = () => {
    set_status(state, "connection error", "error");
    set_status_detail(state, "retrying WebSocket connection");
    socket.close();
  };
  state.websocket = socket;
}

function setup_poll(state, callbacks) {
  if (state.poll_timer) {
    window.clearInterval(state.poll_timer);
  }
  fetch_snapshot(state, callbacks);
  state.poll_timer = window.setInterval(
    () => fetch_snapshot(state, callbacks),
    state.reload_interval_ms
  );
}

export function setup_channel(state, callbacks) {
  if (state.reload_mode === "ws") {
    setup_websocket(state, callbacks);
  } else if (state.reload_mode === "poll") {
    setup_poll(state, callbacks);
  } else {
    setup_sse(state, callbacks);
  }
}
