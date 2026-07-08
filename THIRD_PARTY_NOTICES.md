# Third-Party Notices

This project includes, vendors, downloads, or installs third-party software.

This file is a best-effort notice for the main third-party components used by LamportsFactory. Container images may also contain transitive dependencies installed by Debian packages or bundled by upstream binary distributions. Those components remain subject to their own license terms.

## Vendored Libraries

```text
cpp-httplib
License: MIT License
Source: https://github.com/yhirose/cpp-httplib
Local path: server/third_party/httplib
```

```text
PDF.js
License: Apache License 2.0
Source: https://github.com/mozilla/pdf.js
Local path: server/public/vendor/pdfjs
```

## Downloaded Runtime Binaries

```text
Neovim
License: Apache License 2.0, with Vim-license and third-party-library notices
Source: https://github.com/neovim/neovim
Used by: Docker runtime image
```

```text
ttyd
License: MIT License
Source: https://github.com/tsl0922/ttyd
Used by: Docker runtime image
```

```text
Typst
License: Apache License 2.0
Source: https://github.com/typst/typst
Used by: Typst Docker image
```

## Debian Packages

The Docker images are based on Debian and install packages through `apt`.

Package-specific copyright and license files are available inside the container under:

```text
/usr/share/doc/<package>/copyright
```

Major installed components include:

```text
TeX Live
License: Mixed free software licenses, including LPPL, GPL, public domain, and others depending on package
Source: https://www.tug.org/texlive/
Used by: LaTeX Docker image
```

```text
tmux
License: ISC-style license
Source: https://github.com/tmux/tmux
Used by: Docker runtime image
```

```text
nginx
License: BSD 2-Clause License
Source: https://nginx.org/
Used by: Docker runtime image
```

```text
Node.js
License: MIT License and bundled third-party notices
Source: https://nodejs.org/
Used by: Docker runtime image for nvim-related tooling
```

```text
Noto fonts
License: SIL Open Font License and related font notices depending on package
Source: https://fonts.google.com/noto
Used by: Docker runtime image
```

Other installed packages include tools such as `git`, `curl`, `jq`, `ripgrep`, `fzf`, `make`, `python3`, `npm`, `fontconfig`, `tini`, `unzip`, and build tools in the development image. See the Debian package copyright files in the image for exact license details.

## Notes

LamportsFactory does not claim ownership of third-party software listed here.

When redistributing container images, preserve the relevant license notices from upstream projects and Debian packages. For Apache-licensed components, retain license notices and NOTICE files when provided by upstream. For MIT, BSD, and ISC-style components, retain copyright and permission notices.
