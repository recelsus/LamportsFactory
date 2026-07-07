FROM debian:bookworm-slim AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       build-essential \
       ca-certificates \
       cmake \
       ninja-build \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . /src

RUN cmake -S /src/server -B /tmp/build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build /tmp/build --target lf

RUN mkdir -p /tmp/out/public \
    && cp -r /src/server/public/. /tmp/out/public \
    && install -m 0755 /tmp/build/lf /tmp/out/lamportsfactory

FROM debian:bookworm-slim AS runtime

ARG USER_NAME=app
ARG USER_ID=1000
ARG GROUP_ID=1000
ARG NVIM_VERSION=v0.11.3
ARG TTYD_VERSION=1.7.7

ENV DEBIAN_FRONTEND=noninteractive \
    APP_HOME=/app \
    HOME=/app \
    TEX_DIR=/app/workspace \
    BUILD_DIR_NAME=build \
    TEX_MAIN=sample/main.tex \
    LATEX_ENGINE=lualatex \
    BASE_URL=/ \
    APP_INTERNAL_ADDR=127.0.0.1 \
    APP_INTERNAL_PORT=18080 \
    REVERSE_PROXY_PORT=8080 \
    TTYD_ENABLED=enable \
    TTYD_INTERNAL_ADDR=127.0.0.1 \
    TTYD_INTERNAL_PORT=17681 \
    NVIM_CONFIG_ROOT=/app/nvim/config-root \
    NVIM_DATA_HOME=/app/nvim/data \
    NVIM_CACHE_HOME=/app/nvim/cache \
    NVIM_STATE_HOME=/app/nvim/state \
    NVIM_BOOTSTRAP_ON_INIT=enable \
    TEXMFHOME=/app/texmf \
    OSFONTDIR=/app/fonts \
    PATH=/app/dev/bin:/opt/nvim/bin:/app/.local/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       biber \
       bash \
       ca-certificates \
       curl \
       fontconfig \
       fonts-noto-cjk \
       fonts-noto-core \
       fzf \
       git \
       jq \
       latex-cjk-all \
       latexmk \
       make \
       nginx-light \
       nodejs \
       npm \
       passwd \
       python3 \
       python3-pygments \
       ripgrep \
       texlive-fonts-recommended \
       texlive-lang-japanese \
       texlive-latex-base \
       texlive-latex-extra \
       texlive-latex-recommended \
       texlive-luatex \
       texlive-xetex \
       tini \
       unzip \
       xz-utils \
    && curl -fsSL "https://github.com/neovim/neovim/releases/download/${NVIM_VERSION}/nvim-linux-x86_64.tar.gz" \
       -o /tmp/nvim.tar.gz \
    && curl -fsSL "https://github.com/tsl0922/ttyd/releases/download/${TTYD_VERSION}/ttyd.x86_64" \
       -o /usr/local/bin/ttyd \
    && mkdir -p /opt/nvim \
    && tar -xzf /tmp/nvim.tar.gz -C /opt/nvim --strip-components=1 \
    && ln -sf /opt/nvim/bin/nvim /usr/local/bin/vi \
    && ln -sf /opt/nvim/bin/nvim /usr/local/bin/vim \
    && chmod 0755 /usr/local/bin/ttyd \
    && rm -f /tmp/nvim.tar.gz \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

RUN /usr/sbin/groupadd --gid "${GROUP_ID}" "${USER_NAME}" \
    && /usr/sbin/useradd --uid "${USER_ID}" --gid "${GROUP_ID}" --home-dir "${APP_HOME}" --shell /bin/bash "${USER_NAME}"

WORKDIR "${APP_HOME}"
RUN mkdir -p \
      "${APP_HOME}/workspace" \
      "${APP_HOME}/public" \
      "${APP_HOME}/texmf" \
      "${APP_HOME}/fonts" \
      "${APP_HOME}/nvim/config-root" \
      "${APP_HOME}/nvim/data" \
      "${APP_HOME}/nvim/cache" \
      "${APP_HOME}/nvim/state" \
      "${APP_HOME}/dev/bin" \
      "${APP_HOME}/dev/build" \
      /tmp/nginx-client-body \
      /tmp/nginx-proxy \
      /tmp/nginx-fastcgi \
      /tmp/nginx-uwsgi \
      /tmp/nginx-scgi

COPY --from=builder /tmp/out/lamportsfactory /usr/local/bin/lamportsfactory
COPY --from=builder /tmp/out/public/ "${APP_HOME}/public/"
COPY docker/entrypoint.sh /usr/local/bin/lamportsfactory-entrypoint
COPY docker/nvim-latex-bootstrap.sh /usr/local/bin/nvim-latex-bootstrap
COPY docker/build-server-in-container.sh /usr/local/bin/build-server-in-container

RUN chmod 0755 /usr/local/bin/lamportsfactory-entrypoint /usr/local/bin/nvim-latex-bootstrap /usr/local/bin/build-server-in-container \
    && chown -R "${USER_ID}":"${GROUP_ID}" "${APP_HOME}" /tmp/nginx-client-body /tmp/nginx-proxy /tmp/nginx-fastcgi /tmp/nginx-uwsgi /tmp/nginx-scgi

USER "${USER_NAME}"

VOLUME ["/app/workspace", "/app/texmf", "/app/fonts", "/app/nvim"]

EXPOSE 8080

ENTRYPOINT ["/usr/bin/tini", "--", "/usr/local/bin/lamportsfactory-entrypoint"]

FROM runtime AS dev

USER root

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       build-essential \
       cmake \
       ninja-build \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

USER app
