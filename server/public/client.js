const app_state = {
  reload_mode: "sse",
  reload_interval_ms: 1500,
  reload_retry_ms: 2000,
  last_pdf_mtime: null,
  last_refresh_at: 0,
  event_source: null,
  websocket: null,
  poll_timer: null,
  sync_timer: null,
  status_bar: null,
  status_detail: null,
  pdf_frame: null,
  tex_list: null,
  tex_files: [],
  current_main: null
};

function set_status(text, tone) {
  if (!app_state.status_bar) {
    return;
  }
  app_state.status_bar.textContent = text;
  app_state.status_bar.dataset.state = tone || "idle";
}

function set_status_detail(text) {
  if (!app_state.status_detail) {
    return;
  }
  app_state.status_detail.textContent = text;
}

function format_time_label(epoch_ms) {
  const date = new Date(epoch_ms);
  return `${date.toLocaleTimeString()}`;
}

function build_pdf_url(token) {
  const params = new URLSearchParams();
  if (app_state.current_main) {
    params.set("tex", app_state.current_main);
  }
  params.set("t", token.toString());
  return `/pdf?${params.toString()}`;
}

function reload_pdf(token) {
  if (!app_state.pdf_frame) {
    return;
  }
  app_state.last_refresh_at = Date.now();
  app_state.pdf_frame.src = build_pdf_url(token);
}

async function load_tex_files() {
  try {
    const response = await fetch(`/api/files`, {
      credentials: "include",
      cache: "no-store"
    });
    if (!response.ok) {
      set_status("error", "error");
      set_status_detail(`failed to load files (${response.status})`);
      return;
    }
    const data = await response.json();
    console.debug("files", data);
    app_state.tex_files = Array.isArray(data.files)
      ? data.files.map((item) => (item.path ? item.path : item))
      : [];
    if (data.current) {
      app_state.current_main = data.current;
    }
    render_tex_list();
    set_status_detail(`documents available: ${app_state.tex_files.length}`);
  } catch (error) {
    set_status("error", "error");
    set_status_detail(`load files error: ${error.message}`);
  }
}

function render_tex_list() {
  if (!app_state.tex_list) {
    return;
  }
  app_state.tex_list.innerHTML = "";
  if (app_state.tex_files.length === 0) {
    const li = document.createElement("li");
    li.className = "tex-item";
    li.textContent = "No .tex files found";
    app_state.tex_list.appendChild(li);
    return;
  }
  app_state.tex_files.forEach((file) => {
    const li = document.createElement("li");
    li.className = "tex-item";
    const button = document.createElement("button");
    button.type = "button";
    button.className = "tex-item-button";
    button.textContent = file;
    if (file === app_state.current_main) {
      button.classList.add("active");
    }
    button.addEventListener("click", () => {
      select_tex_document(file);
    });
    li.appendChild(button);
    app_state.tex_list.appendChild(li);
  });
}

async function fetch_snapshot() {
  try {
    const response = await fetch(`/api/mtime`, {
      credentials: "include",
      cache: "no-store"
    });
    if (!response.ok) {
      return;
    }
    const snapshot = await response.json();
    console.debug("snapshot", snapshot);
    if (snapshot.tex_main && snapshot.tex_main !== app_state.current_main) {
      app_state.current_main = snapshot.tex_main;
      render_tex_list();
    }
    if (typeof snapshot.pdf_mtime === "number" && snapshot.pdf_mtime !== app_state.last_pdf_mtime) {
      app_state.last_pdf_mtime = snapshot.pdf_mtime;
      reload_pdf(snapshot.pdf_mtime);
    }
    if (snapshot.building) {
      set_status("building…", "building");
      const queued = snapshot.queued || 0;
      set_status_detail(queued > 0 ? `running build (queued ${queued})` : "running build");
      return;
    }
    if (snapshot.last_success && typeof snapshot.pdf_mtime === "number") {
      set_status(`updated ${format_time_label(snapshot.pdf_mtime)}`, "idle");
      set_status_detail("latest PDF delivered");
      return;
    }
    if (!snapshot.last_success && snapshot.last_error) {
      set_status("build failed", "error");
      set_status_detail(snapshot.last_error);
      return;
    }
    set_status("idle", "idle");
    set_status_detail("waiting for changes");
    await load_tex_files();
  } catch (_) {
  }
}

function handle_build_start(payload) {
  if (payload && payload.tex_main) {
    app_state.current_main = payload.tex_main;
    render_tex_list();
  }
  set_status("building…", "building");
  const queued = payload && typeof payload.queued === "number" ? payload.queued : 0;
  set_status_detail(queued > 0 ? `running build (queued ${queued})` : "running build");
}

function handle_build_ok(payload) {
  if (payload && payload.tex_main) {
    app_state.current_main = payload.tex_main;
    render_tex_list();
  }
  const mtime = payload && typeof payload.pdf_mtime === "number" ? payload.pdf_mtime : Date.now();
  app_state.last_pdf_mtime = mtime;
  reload_pdf(mtime);
  set_status(`updated ${format_time_label(mtime)}`, "idle");
  set_status_detail(`duration ${payload && payload.duration_ms ? payload.duration_ms : 0} ms`);
}

function handle_build_fail(payload) {
  if (payload && payload.tex_main) {
    app_state.current_main = payload.tex_main;
    render_tex_list();
  }
  set_status("build failed", "error");
  set_status_detail(payload && payload.message ? payload.message : "build failed");
}

function handle_pdf_updated(payload) {
  if (!payload || typeof payload.pdf_mtime !== "number") {
    return;
  }
  if (payload.tex_main) {
    app_state.current_main = payload.tex_main;
    render_tex_list();
  }
  app_state.last_pdf_mtime = payload.pdf_mtime;
  reload_pdf(payload.pdf_mtime);
}

function dispatch_event(name, payload) {
  switch (name) {
    case "build_start":
      handle_build_start(payload);
      break;
    case "build_ok":
      handle_build_ok(payload);
      break;
    case "build_fail":
      handle_build_fail(payload);
      break;
    case "pdf_updated":
      handle_pdf_updated(payload);
      break;
    default:
      break;
  }
}

function parse_json(text) {
  try {
    return JSON.parse(text);
  } catch (_) {
    return null;
  }
}

function setup_sse() {
  if (app_state.event_source) {
    app_state.event_source.close();
  }
  const source = new EventSource(`/events`, { withCredentials: true });
  source.addEventListener("build_start", (event) => {
    dispatch_event("build_start", parse_json(event.data));
  });
  source.addEventListener("build_ok", (event) => {
    dispatch_event("build_ok", parse_json(event.data));
  });
  source.addEventListener("build_fail", (event) => {
    dispatch_event("build_fail", parse_json(event.data));
  });
  source.addEventListener("pdf_updated", (event) => {
    dispatch_event("pdf_updated", parse_json(event.data));
  });
  source.addEventListener("ping", () => {});
  source.onerror = () => {
    set_status("connection lost", "error");
    set_status_detail("retrying SSE connection");
    source.close();
    app_state.event_source = null;
    window.setTimeout(setup_channel, app_state.reload_retry_ms);
  };
  app_state.event_source = source;
}

function setup_websocket() {
  if (app_state.websocket) {
    app_state.websocket.close();
  }
  const protocol = window.location.protocol === "https:" ? "wss" : "ws";
  const socket = new WebSocket(`${protocol}://${window.location.host}/ws`);
  socket.onmessage = (event) => {
    const payload = parse_json(event.data);
    if (payload) {
      dispatch_event(payload.event || payload.type, payload.data || payload);
    }
  };
  socket.onopen = () => {
    set_status("connected", "idle");
    set_status_detail("watching for changes");
  };
  socket.onclose = () => {
    set_status("connection lost", "error");
    set_status_detail("retrying WebSocket connection");
    app_state.websocket = null;
    window.setTimeout(setup_channel, app_state.reload_retry_ms);
  };
  socket.onerror = () => {
    set_status("connection error", "error");
    set_status_detail("retrying WebSocket connection");
    socket.close();
  };
  app_state.websocket = socket;
}

function setup_poll() {
  if (app_state.poll_timer) {
    window.clearInterval(app_state.poll_timer);
  }
  fetch_snapshot();
  app_state.poll_timer = window.setInterval(fetch_snapshot, app_state.reload_interval_ms);
}

function setup_channel() {
  if (app_state.reload_mode === "ws") {
    setup_websocket();
  } else if (app_state.reload_mode === "poll") {
    setup_poll();
  } else {
    setup_sse();
  }
}

async function select_tex_document(tex_path) {
  if (!tex_path || tex_path === app_state.current_main) {
    await fetch_snapshot();
    return;
  }
  try {
    set_status("switching document…", "building");
    set_status_detail(tex_path);
    const response = await fetch(`/api/main?tex=${encodeURIComponent(tex_path)}`, {
      method: "POST",
      credentials: "include",
      cache: "no-store"
    });
    if (!response.ok) {
      throw new Error("switch failed");
    }
    const data = await response.json();
    if (data.tex_main) {
      app_state.current_main = data.tex_main;
    } else {
      app_state.current_main = tex_path;
    }
    app_state.last_pdf_mtime = null;
    render_tex_list();
    reload_pdf(Date.now());
    await fetch_snapshot();
  } catch (_) {
    set_status("document switch failed", "error");
    set_status_detail("unable to switch preview");
  }
}

async function initialize() {
  app_state.status_bar = document.getElementById("status-bar");
  app_state.status_detail = document.getElementById("status-detail");
  app_state.pdf_frame = document.getElementById("pdf-frame");
  app_state.tex_list = document.getElementById("tex-list");
  set_status("loading…", "idle");
  set_status_detail("fetching configuration");
  // load_config is no longer used; keep compatibility with old clients
  // await load_config();
  await load_tex_files();
  await fetch_snapshot();
  setup_channel();
  if (app_state.sync_timer) {
    window.clearInterval(app_state.sync_timer);
  }
  app_state.sync_timer = window.setInterval(fetch_snapshot,
    Math.max(2000, app_state.reload_interval_ms));
}

document.addEventListener("DOMContentLoaded", initialize);
