# ─────────────────────────────────────────────────────────────
# JUCE C++ audio-plugin dev container, with Claude Code onboard.
# Base: Ubuntu 24.04 (glibc, wide package availability).
# ─────────────────────────────────────────────────────────────
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive \
    TZ=Etc/UTC

# ── Build toolchain + JUCE 8 Linux deps + headless-GUI stack ──
RUN apt-get update && apt-get install -y --no-install-recommends \
    # Core build toolchain
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    git \
    curl \
    wget \
    ca-certificates \
    gnupg \
    unzip \
    ripgrep \
    clang \
    clangd \
    clang-format \
    clang-tidy \
    lld \
    llvm \
    gdb \
    lldb \
    # JUCE 8 Linux dependencies (GUI + audio backends)
    libasound2-dev \
    libjack-jackd2-dev \
    libfreetype-dev \
    libfontconfig1-dev \
    libx11-dev \
    libxcomposite-dev \
    libxcursor-dev \
    libxext-dev \
    libxinerama-dev \
    libxrandr-dev \
    libxrender-dev \
    libglu1-mesa-dev \
    mesa-common-dev \
    libgl1-mesa-dev \
    libcurl4-openssl-dev \
    libgtk-3-dev \
    libwebkit2gtk-4.1-dev \
    # Headless GUI: run/see the plugin editor over VNC in the browser
    xvfb \
    x11vnc \
    novnc \
    websockify \
    fluxbox \
    # Node.js (for the Claude Code npm route + any WebView UI tooling)
    nodejs \
    npm \
    && rm -rf /var/lib/apt/lists/*

# ── Claude Code ──────────────────────────────────────────────
# The npm package pulls the same native binary as the standalone
# installer. Node 22+ is required for recent versions; install a
# current Node via NodeSource, then the CLI (never as root/sudo
# into a system prefix — use a user-writable global prefix).
RUN curl -fsSL https://deb.nodesource.com/setup_22.x | bash - \
    && apt-get install -y --no-install-recommends nodejs \
    && rm -rf /var/lib/apt/lists/*

# ── Non-root dev user ────────────────────────────────────────
# Ubuntu 24.04 ships a default 'ubuntu' user at UID 1000; reuse it.
ARG USERNAME=ubuntu
ARG USER_UID=1000
ARG USER_GID=1000
RUN groupmod -o -g "${USER_GID}" "${USERNAME}" 2>/dev/null || true \
    && usermod  -o -u "${USER_UID}" "${USERNAME}" 2>/dev/null || true \
    && mkdir -p /workspace && chown -R "${USER_UID}:${USER_GID}" /workspace

USER ${USERNAME}
WORKDIR /workspace

# User-writable npm global prefix, then Claude Code.
ENV NPM_CONFIG_PREFIX=/home/${USERNAME}/.npm-global
ENV PATH=/home/${USERNAME}/.npm-global/bin:${PATH}
RUN mkdir -p "${NPM_CONFIG_PREFIX}" \
    && npm install -g @anthropic-ai/claude-code@latest \
    && claude --version

# Headless display defaults (overridable at runtime).
ENV DISPLAY=:99 \
    SCREEN_GEOMETRY=1600x1000x24 \
    VNC_PORT=5900 \
    NOVNC_PORT=6080

# noVNC (browser client) → 6080, raw VNC → 5900
EXPOSE 6080 5900
