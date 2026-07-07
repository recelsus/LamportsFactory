export function bind_pdf_toolbar(root, actions) {
  if (!root) {
    return;
  }
  root.querySelectorAll("[data-pdf-action]").forEach((button) => {
    const action = button.dataset.pdfAction;
    button.addEventListener("click", () => {
      if (typeof actions[action] === "function") {
        actions[action]();
      }
    });
  });
}
