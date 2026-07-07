const layout_storage_key = "lamportsfactory.layout";

export function stored_layout_mode() {
  try {
    const value = window.localStorage.getItem(layout_storage_key);
    return value === "split" || value === "preview" ? value : null;
  } catch (_) {
    return null;
  }
}

function store_layout_mode(value) {
  try {
    window.localStorage.setItem(layout_storage_key, value);
  } catch (_) {
  }
}

export function apply_layout(state) {
  if (state.layout_mode === "split" && !state.ttyd_enabled) {
    state.layout_mode = "preview";
  }
  if (state.main_area) {
    state.main_area.dataset.layout = state.layout_mode;
  }
  const show_terminal = state.layout_mode === "split" && state.ttyd_enabled;
  if (state.terminal_pane) {
    state.terminal_pane.hidden = !show_terminal;
  }
  if (show_terminal && state.terminal_frame && !state.terminal_frame.src) {
    state.terminal_frame.src = state.ttyd_url;
  }
  if (state.layout_toggle) {
    state.layout_toggle.hidden = !state.ttyd_enabled;
    state.layout_toggle.querySelectorAll("[data-layout-mode]").forEach((button) => {
      button.classList.toggle("active", button.dataset.layoutMode === state.layout_mode);
    });
  }
}

export function setup_layout_toggle(state) {
  if (!state.layout_toggle) {
    return;
  }
  state.layout_toggle.querySelectorAll("[data-layout-mode]").forEach((button) => {
    button.addEventListener("click", () => {
      const value = button.dataset.layoutMode;
      if (value !== "split" && value !== "preview") {
        return;
      }
      state.layout_mode = value;
      store_layout_mode(value);
      apply_layout(state);
    });
  });
}
