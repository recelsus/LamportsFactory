import { configure_pdfjs, PdfRenderer } from "./pdf_renderer.js";
import { bind_pdf_toolbar } from "./pdf_toolbar.js";

export function create_pdf_viewer(options) {
  configure_pdfjs(options.worker_src);
  const renderer = new PdfRenderer({
    canvas: options.canvas,
    page_label: options.page_label,
    zoom_label: options.zoom_label
  });
  const state = {
    current_url: "",
    filename: "preview.pdf"
  };

  const viewer = {
    async load(url, filename) {
      state.current_url = url;
      state.filename = filename || state.filename;
      await renderer.load(url);
    },
    async previous_page() {
      await renderer.previous_page();
    },
    async next_page() {
      await renderer.next_page();
    },
    async zoom_out() {
      await renderer.zoom_out();
    },
    async zoom_in() {
      await renderer.zoom_in();
    },
    async rotate() {
      await renderer.rotate_clockwise();
    },
    download() {
      if (!state.current_url) {
        return;
      }
      const link = document.createElement("a");
      link.href = state.current_url;
      link.download = state.filename;
      document.body.appendChild(link);
      link.click();
      link.remove();
    }
  };

  bind_pdf_toolbar(options.toolbar, viewer);
  return viewer;
}
