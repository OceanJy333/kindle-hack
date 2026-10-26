# input: dated Debian archive; output: host tools and ARMhf Qt 6.4.2 sysroot; pos: reproducible cross toolchain.
FROM debian:bookworm-20260202-slim@sha256:98f4b71de414932439ac6ac690d7060df1f27161073c5036a7553723881bffbe
ARG SNAPSHOT=20260201T000000Z
COPY index-debian-packages.py /opt/index-debian-packages.py
RUN printf 'deb [check-valid-until=no] http://snapshot.debian.org/archive/debian/%s bookworm main\ndeb [check-valid-until=no] http://snapshot.debian.org/archive/debian-security/%s bookworm-security main\n' "$SNAPSHOT" "$SNAPSHOT" > /etc/apt/sources.list \
    && rm -f /etc/apt/sources.list.d/debian.sources \
    && dpkg --add-architecture armhf \
    && apt-get -o Acquire::Check-Valid-Until=false update \
    && apt-get install -y --no-install-recommends \
       gcc-arm-linux-gnueabihf g++-arm-linux-gnueabihf gcc g++ make bison gawk \
       python3 texinfo file ca-certificates curl xz-utils patch pkg-config cmake ninja-build \
       qt6-base-dev-tools qt6-declarative-dev-tools e2fsprogs libzstd1 \
    && mkdir /opt/armhf /tmp/armhf-debs \
    && apt-get -y -d -o Dir::State::status=/dev/null -o Dir::Cache::archives=/tmp/armhf-debs \
       install qt6-base-dev:armhf qt6-declarative-dev:armhf qt6-svg-dev:armhf \
       libqrencode-dev:armhf libssl-dev:armhf zlib1g-dev:armhf \
       qml6-module-qtqml:armhf qml6-module-qtqml-models:armhf \
       qml6-module-qtquick:armhf qml6-module-qtquick-window:armhf \
       qml6-module-qtquick-controls:armhf qml6-module-qtquick-layouts:armhf \
       qml6-module-qtquick-templates:armhf qml6-module-qtqml-workerscript:armhf \
    && python3 /opt/index-debian-packages.py --deb-dir /tmp/armhf-debs --query-apt-cache --output /opt/debian-package-index.json \
    && for archive in /tmp/armhf-debs/*.deb; do dpkg-deb -x "$archive" /opt/armhf; done \
    && (cd /tmp/armhf-debs && sha256sum *.deb > /opt/armhf-packages.sha256) \
    && rm -rf /tmp/armhf-debs /var/lib/apt/lists/*
WORKDIR /lab/native-runtime
