import { app_url, build_pdf_url } from "./url.js";
import { set_status, set_status_detail } from "./status.js";

function pdf_download_filename(state) {
  const main = state.current_main || "preview.tex";
  const name = main.split("/").filter(Boolean).pop() || "preview.tex";
  return name.replace(/\.[^.]+$/, "") + ".pdf";
}

export async function setup_pdf_viewer(state) {
  if (!state.pdf_canvas || !state.pdf_toolbar) {
    return;
  }
  try {
    const module = await import(app_url(state, "/static/pdf/pdf_viewer.js"));
    state.pdf_viewer = module.create_pdf_viewer({
      canvas: state.pdf_canvas,
      toolbar: state.pdf_toolbar,
      page_label: state.pdf_page_label,
      zoom_label: state.pdf_zoom_label,
      worker_src: app_url(state, "/static/vendor/pdfjs/pdf.worker.js")
    });
  } catch (error) {
    set_status(state, "pdf viewer failed", "error");
    set_status_detail(state, error.message);
  }
}

export async function reload_pdf(state, token) {
  if (!state.pdf_viewer) {
    return;
  }
  state.last_refresh_at = Date.now();
  try {
    await state.pdf_viewer.load(build_pdf_url(state, token), pdf_download_filename(state));
  } catch (error) {
    set_status(state, "pdf load failed", "error");
    set_status_detail(state, error.message);
  }
}
