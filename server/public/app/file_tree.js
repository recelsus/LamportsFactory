export function normalize_document_file_list(files) {
  return Array.isArray(files)
    ? files.map((item) => (item && item.path ? item.path : item)).filter(Boolean)
    : [];
}

function same_string_list(left, right) {
  if (left.length !== right.length) {
    return false;
  }
  return left.every((item, index) => item === right[index]);
}

export function update_documents(state, files) {
  const next_files = normalize_document_file_list(files);
  if (next_files.length === 0 && state.documents.length > 0) {
    return false;
  }
  if (same_string_list(state.documents, next_files)) {
    return false;
  }
  state.documents = next_files;
  render_document_list(state);
  return true;
}

export function render_document_list(state, select_document) {
  if (!state.document_list) {
    return;
  }
  state.document_list.innerHTML = "";
  if (state.documents.length === 0) {
    const li = document.createElement("li");
    li.className = "document-item";
    li.textContent = "No documents found";
    state.document_list.appendChild(li);
    return;
  }
  const tree = build_file_tree(state.documents);
  render_tree_nodes(state, state.document_list, tree.children, select_document);
}

function create_tree_node(name, path, type) {
  return {
    name,
    path,
    type,
    children: [],
    child_map: new Map()
  };
}

function build_file_tree(files) {
  const root = create_tree_node("", "", "dir");
  files.forEach((file) => {
    const parts = file.split("/").filter(Boolean);
    let current = root;
    parts.forEach((part, index) => {
      const is_file = index === parts.length - 1;
      const path = parts.slice(0, index + 1).join("/");
      const key = `${is_file ? "file" : "dir"}:${part}`;
      if (!current.child_map.has(key)) {
        const node = create_tree_node(part, path, is_file ? "file" : "dir");
        current.child_map.set(key, node);
        current.children.push(node);
      }
      current = current.child_map.get(key);
    });
  });
  sort_tree(root);
  return root;
}

function sort_tree(node) {
  node.children.sort((left, right) => {
    if (left.type !== right.type) {
      return left.type === "dir" ? -1 : 1;
    }
    return left.name.localeCompare(right.name);
  });
  node.children.forEach(sort_tree);
}

function node_contains_current(state, node) {
  if (node.type === "file") {
    return node.path === state.current_document;
  }
  return node.children.some((child) => node_contains_current(state, child));
}

function render_tree_nodes(state, parent, nodes, select_document) {
  nodes.forEach((node) => {
    const li = document.createElement("li");
    li.className = node.type === "dir" ? "tree-dir" : "tree-file";
    if (node.type === "dir") {
      const details = document.createElement("details");
      details.open = node_contains_current(state, node) || node.path === "";
      const summary = document.createElement("summary");
      summary.className = "tree-dir-summary";
      summary.textContent = node.name;
      const child_list = document.createElement("ul");
      child_list.className = "document-list nested";
      render_tree_nodes(state, child_list, node.children, select_document);
      details.appendChild(summary);
      details.appendChild(child_list);
      li.appendChild(details);
    } else {
      const button = document.createElement("button");
      button.type = "button";
      button.className = "document-item-button";
      button.textContent = node.name;
      button.title = node.path;
      if (node.path === state.current_document) {
        button.classList.add("active");
      }
      button.addEventListener("click", () => {
        select_document(node.path);
      });
      li.appendChild(button);
    }
    parent.appendChild(li);
  });
}
