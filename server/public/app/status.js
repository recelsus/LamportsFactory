export function set_status(state, text, tone) {
  if (!state.status_bar) {
    return;
  }
  state.status_bar.textContent = text;
  state.status_bar.dataset.state = tone || "idle";
}

export function set_status_detail(state, text) {
  if (!state.status_detail) {
    return;
  }
  state.status_detail.textContent = text;
}

export function format_time_label(epoch_ms) {
  const date = new Date(epoch_ms);
  return `${date.toLocaleTimeString()}`;
}
