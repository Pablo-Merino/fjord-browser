# syntax=docker/dockerfile:1
FROM archlinux:base-devel-20260816.0.574111@sha256:714acd1eef9ae997d95691b1c5220ada0076185b77857c1813f02de0fa83cf7b

ARG ARCH_SNAPSHOT=2026/08/20
ARG HOST_UID=1000
ARG HOST_GID=1000

RUN printf 'Server = https://archive.archlinux.org/repos/%s/$repo/os/$arch\n' "$ARCH_SNAPSHOT" > /etc/pacman.d/mirrorlist \
    && pacman -Sy --needed --noconfirm archlinux-keyring \
    && pacman -Su --needed --noconfirm \
        clang \
        cmake \
        fontconfig \
        freetype2 \
        git \
        grim \
        libxkbcommon \
        ninja \
        pkgconf \
        rustup \
        sway \
        ttf-dejavu \
        vulkan-headers \
        vulkan-icd-loader \
        vulkan-swrast \
        vulkan-tools \
        wayland \
        wayland-protocols \
        wpewebkit \
    && test "$(pacman -Q wpewebkit)" = "wpewebkit 2.52.6-1" \
    && pacman -Scc --noconfirm

# The package grants SYS_NICE for desktop sessions. Docker rejects execution
# without that capability, and the headless software renderer does not need it.
RUN setcap -r /usr/bin/sway

ENV RUSTUP_HOME=/opt/rustup

COPY rust-toolchain.toml /tmp/fjord-toolchain/rust-toolchain.toml

RUN rustup set profile minimal \
    && TOOLCHAIN=$(sed -n 's/^channel = "\([^"]*\)"/\1/p' /tmp/fjord-toolchain/rust-toolchain.toml) \
    && test -n "$TOOLCHAIN" \
    && rustup toolchain install "$TOOLCHAIN" --component clippy --component rustfmt \
    && rustup default "$TOOLCHAIN" \
    && chmod -R a+rX /opt/rustup

RUN groupadd --gid "$HOST_GID" fjord \
    && useradd --create-home --uid "$HOST_UID" --gid "$HOST_GID" fjord \
    && install -d --owner="$HOST_UID" --group="$HOST_GID" /home/fjord/.cargo

ENV CARGO_HOME=/home/fjord/.cargo
ENV PATH=/home/fjord/.cargo/bin:/usr/bin

USER fjord
WORKDIR /workspace
