# LamportsFactory

指定ディレクトリの LaTeX / Typst ファイルを監視して、生成されたPDFをブラウザでプレビューします。

## Image

```text
ghcr.io/recelsus/lamportsfactory:latex-latest
ghcr.io/recelsus/lamportsfactory:typst-latest
```

## Docker Compose

作業用ディレクトリを作成します。

```bash
mkdir -p workspace volumes/texmf volumes/fonts volumes/nvim
```
`docker-compose.yml` を作成します。

```yaml
services:
  lamports-factory:
    image: ghcr.io/recelsus/lamportsfactory:latex-latest
    container_name: lamports-factory
    environment:
      # path only. examples: / | /lamports-factory
      BASE_URL: /
      WORKSPACE_DIR: /app/workspace
      BUILD_DIR_NAME: build
      DOCUMENT_EXTENSION: .tex
      COMPILER_BACKEND: latex
      LATEX_BUILD_TOOL: latexmk
      LATEX_ENGINE: lualatex
      RELOAD_MODE: sse
      LAYOUT_MODE: split
      TTYD_ENABLED: enable
      # optional:
      # MAIN_DOCUMENT: sample/main.tex
      # NVIM_CONFIG_REPO: "https://github.com/yourname/nvim.git"
      # NVIM_BOOTSTRAP_ON_INIT: enable
    volumes:
      - ./workspace:/app/workspace
      - ./volumes/texmf:/app/texmf
      - ./volumes/fonts:/app/fonts
      - ./volumes/nvim:/app/nvim
    ports:
      - "8080:8080"
    restart: unless-stopped
```

```bash
docker compose up -d
```

## Typst で使う場合

Typst 版を利用する場合は、image と文書設定を変更します。

```yaml
image: ghcr.io/recelsus/lamportsfactory:typst-latest
environment:
  # path only. examples: / | /lamports-factory
  BASE_URL: /
  WORKSPACE_DIR: /app/workspace
  BUILD_DIR_NAME: build
  DOCUMENT_EXTENSION: .typ
  COMPILER_BACKEND: typst
  RELOAD_MODE: sse
  LAYOUT_MODE: split
  TTYD_ENABLED: enable
  # optional:
  # MAIN_DOCUMENT: sample/main.typ
  # NVIM_CONFIG_REPO: "https://github.com/yourname/nvim.git"
  # NVIM_BOOTSTRAP_ON_INIT: enable
  # TYPST_OPTS: "--root . --font-path /app/fonts"
  ...
```

## Directory

```text
workspace/       編集する .tex/.typ ファイル
volumes/texmf/   LaTeX用の追加パッケージやローカル texmf
volumes/fonts/   追加フォント
volumes/nvim/    nvim設定、プラグイン、キャッシュ
```

## Config

代表的な環境変数です。

```text
BASE_URL                 path のみ。例: / | /lamports-factory
MAIN_DOCUMENT            初期ビルド対象の文書。未指定時は backend ごとの既定値
DOCUMENT_EXTENSION       .tex | .typ
COMPILER_BACKEND         latex | typst
BUILD_DIR_NAME           各文書ディレクトリに作成する build ディレクトリ名
LAYOUT_MODE              preview | split
TTYD_ENABLED             enable | disable
NVIM_CONFIG_REPO         nvim 設定リポジトリ。未指定時は clone しない
NVIM_BOOTSTRAP_ON_INIT   enable | disable。未指定時は enable
```

LaTeX 版で利用する環境変数です。

```text
LATEX_BUILD_TOOL         latexmk | tectonic
LATEX_ENGINE             lualatex | xelatex | pdflatex | platex | uplatex
LATEXMK_OPTS             latexmk のオプションを直接指定
TECTONIC_OPTS            tectonic のオプションを直接指定
```

Typst 版で利用する環境変数です。

```text
TYPST_OPTS               typst compile に渡す追加オプション
```

## Third-Party Licenses

このプロジェクトでは以下の外部ライブラリを同梱または vendoring しています。

```text
cpp-httplib              MIT License
PDF.js                   Apache License 2.0
```

コンテナイメージには TeX Live、Typst、Neovim、ttyd、tmux、nginx などの外部ツールも含まれます。これらは各プロジェクトのライセンスに従います。

詳細は [THIRD_PARTY_NOTICES.md](./THIRD_PARTY_NOTICES.md) を参照してください。
