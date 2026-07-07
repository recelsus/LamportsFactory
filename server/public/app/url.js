export function normalize_base_url(value) {
  let base = value || "";
  if (base === "/") {
    return "";
  }
  if (base && !base.startsWith("/")) {
    base = `/${base}`;
  }
  return base.replace(/\/+$/, "");
}

export function app_url(state, path) {
  const clean_path = path.startsWith("/") ? path : `/${path}`;
  return `${state.base_url}${clean_path}`;
}

export function build_pdf_url(state, token) {
  const params = new URLSearchParams();
  if (state.current_main) {
    params.set("tex", state.current_main);
  }
  params.set("t", token.toString());
  return app_url(state, `/pdf?${params.toString()}`);
}
