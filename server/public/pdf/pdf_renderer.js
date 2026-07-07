import * as pdfjsLib from "../vendor/pdfjs/pdf.js";

export function configure_pdfjs(worker_src) {
  pdfjsLib.GlobalWorkerOptions.workerSrc = worker_src;
}

export class PdfRenderer {
  constructor({ canvas, page_label, zoom_label }) {
    this.canvas = canvas;
    this.context = canvas.getContext("2d");
    this.page_label = page_label;
    this.zoom_label = zoom_label;
    this.document = null;
    this.loading_task = null;
    this.render_task = null;
    this.page_number = 1;
    this.scale = 1.0;
    this.rotation = 0;
    this.render_sequence = 0;
  }

  async load(url) {
    this.render_sequence += 1;
    const sequence = this.render_sequence;
    if (this.render_task) {
      this.render_task.cancel();
      this.render_task = null;
    }
    if (this.loading_task) {
      this.loading_task.destroy();
      this.loading_task = null;
    }
    this.loading_task = pdfjsLib.getDocument({ url });
    const document = await this.loading_task.promise;
    if (sequence !== this.render_sequence) {
      await document.destroy();
      return;
    }
    if (this.document) {
      await this.document.destroy();
    }
    this.document = document;
    this.page_number = Math.min(this.page_number, this.document.numPages);
    this.page_number = Math.max(this.page_number, 1);
    await this.render();
  }

  async render() {
    if (!this.document) {
      return;
    }
    const sequence = ++this.render_sequence;
    if (this.render_task) {
      this.render_task.cancel();
      this.render_task = null;
    }
    const page = await this.document.getPage(this.page_number);
    if (sequence !== this.render_sequence) {
      return;
    }
    const viewport = page.getViewport({
      scale: this.scale,
      rotation: this.rotation
    });
    const output_scale = window.devicePixelRatio || 1;
    this.canvas.width = Math.floor(viewport.width * output_scale);
    this.canvas.height = Math.floor(viewport.height * output_scale);
    this.canvas.style.width = `${Math.floor(viewport.width)}px`;
    this.canvas.style.height = `${Math.floor(viewport.height)}px`;
    const transform = output_scale !== 1
      ? [output_scale, 0, 0, output_scale, 0, 0]
      : null;
    this.render_task = page.render({
      canvasContext: this.context,
      transform,
      viewport
    });
    try {
      await this.render_task.promise;
    } catch (error) {
      if (error && error.name === "RenderingCancelledException") {
        return;
      }
      throw error;
    } finally {
      if (sequence === this.render_sequence) {
        this.render_task = null;
      }
    }
    this.update_labels();
  }

  async next_page() {
    if (!this.document || this.page_number >= this.document.numPages) {
      return;
    }
    this.page_number += 1;
    await this.render();
  }

  async previous_page() {
    if (!this.document || this.page_number <= 1) {
      return;
    }
    this.page_number -= 1;
    await this.render();
  }

  async zoom_in() {
    this.scale = Math.min(this.scale + 0.1, 3.0);
    await this.render();
  }

  async zoom_out() {
    this.scale = Math.max(this.scale - 0.1, 0.3);
    await this.render();
  }

  async rotate_clockwise() {
    this.rotation = (this.rotation + 90) % 360;
    await this.render();
  }

  update_labels() {
    if (this.page_label) {
      const total = this.document ? this.document.numPages : 0;
      this.page_label.textContent = `${this.page_number}/${total}`;
    }
    if (this.zoom_label) {
      this.zoom_label.textContent = `${Math.round(this.scale * 100)}%`;
    }
  }
}
