const layout_storage_key = "lamportsfactory.layout";
const layout_order_storage_key = "lamportsfactory.layoutOrder";

export function stored_layout_mode() {
  try {
    const value = window.localStorage.getItem(layout_storage_key);
    return value === "split" || value === "preview" ? value : null;
  } catch (_) {
    return null;
  }
}

export function stored_layout_order() {
  try {
    const value = window.localStorage.getItem(layout_order_storage_key);
    return value === "preview-first" || value === "terminal-first" ? value : null;
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

function store_layout_order(value) {
  try {
    window.localStorage.setItem(layout_order_storage_key, value);
  } catch (_) {
  }
}

export function apply_layout(state) {
  if (state.layout_mode === "split" && !state.ttyd_enabled) {
    state.layout_mode = "preview";
  }
  if (state.layout_order !== "terminal-first") {
    state.layout_order = "preview-first";
  }
  if (state.main_area) {
    state.main_area.dataset.layout = state.layout_mode;
    state.main_area.dataset.layoutOrder = state.layout_order;
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
  if (state.layout_swap_button) {
    state.layout_swap_button.hidden = !show_terminal;
    state.layout_swap_button.classList.toggle("active", state.layout_order === "terminal-first");
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
  if (state.layout_swap_button) {
    state.layout_swap_button.addEventListener("click", () => {
      state.layout_order = state.layout_order === "terminal-first"
        ? "preview-first"
        : "terminal-first";
      store_layout_order(state.layout_order);
      apply_layout(state);
    });
  }
}
