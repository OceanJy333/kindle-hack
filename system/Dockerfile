# input: Debian Bookworm build packages; output: ARM cross-build image; pos: independent-system toolchain.
FROM debian:bookworm-20260202-slim@sha256:98f4b71de414932439ac6ac690d7060df1f27161073c5036a7553723881bffbe
ARG SNAPSHOT=20260201T000000Z
RUN printf 'deb [check-valid-until=no] http://snapshot.debian.org/archive/debian/%s bookworm main\ndeb [check-valid-until=no] http://snapshot.debian.org/archive/debian-security/%s bookworm-security main\n' "$SNAPSHOT" "$SNAPSHOT" > /etc/apt/sources.list \
    && rm -f /etc/apt/sources.list.d/debian.sources \
    && apt-get -o Acquire::Check-Valid-Until=false update \
    && apt-get install -y --no-install-recommends \
    build-essential gcc-arm-linux-gnueabi gcc-arm-linux-gnueabihf \
    libc6-dev-armel-cross libc6-dev-armhf-cross bison flex bc \
    libssl-dev libncurses-dev pkg-config patch python3 qemu-user-static \
    busybox-static u-boot-tools device-tree-compiler cpio xz-utils bzip2 \
    && ln -s /usr/bin/qemu-arm-static /usr/local/bin/qemu-arm \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /work
CMD ["sleep", "infinity"]
