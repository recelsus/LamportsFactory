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

FROM debian:bookworm-slim

ARG USER_NAME=app
ARG USER_ID=1000
ARG GROUP_ID=1000

ENV DEBIAN_FRONTEND=noninteractive \
    APP_HOME=/app \
    TECTONIC_VERSION=0.13.1

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       ca-certificates \
       curl \
       latexmk \
       pkg-config \
       python3 \
       python3-pip \
       texlive-latex-base \
       texlive-latex-recommended \
       texlive-fonts-recommended \
       tini \
       xz-utils \
    && curl -fsSL "https://github.com/tectonic-typesetting/tectonic/releases/download/tectonic%40${TECTONIC_VERSION}/tectonic-${TECTONIC_VERSION}-x86_64-unknown-linux-gnu.tar.gz" \
         -o /tmp/tectonic.tar.gz \
    && tar -xzf /tmp/tectonic.tar.gz -C /tmp \
    && tectonic_path="$(find /tmp -maxdepth 3 -type f -name tectonic -perm -111 | head -n1)" \
    && [ -n "${tectonic_path}" ] \
    && install -m 0755 "${tectonic_path}" /usr/local/bin/tectonic \
    && rm -rf /tmp/tectonic* \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

RUN groupadd --gid "${GROUP_ID}" "${USER_NAME}" \
    && useradd --uid "${USER_ID}" --gid "${GROUP_ID}" --home-dir "${APP_HOME}" --shell /bin/bash "${USER_NAME}"

WORKDIR "${APP_HOME}"
RUN mkdir -p "${APP_HOME}/workspace" "${APP_HOME}/public"

COPY --from=builder /tmp/out/lamportsfactory /usr/local/bin/lamportsfactory
COPY --from=builder /tmp/out/public/ "${APP_HOME}/public/"

RUN chown -R "${USER_ID}":"${GROUP_ID}" "${APP_HOME}"

USER "${USER_NAME}"

VOLUME ["/app/workspace"]

ENTRYPOINT ["/usr/bin/tini", "--"]
CMD ["lamportsfactory"]
