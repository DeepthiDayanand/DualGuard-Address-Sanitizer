# Build environment for DualGuard (source stays on the host, bind-mounted in).
FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
# Avoid intermittent "Hash Sum mismatch" errors seen on this network under WSL.
RUN printf 'Acquire::http::Pipeline-Depth "0";\nAcquire::http::No-Cache "true";\nAcquire::Retries "5";\n' \
        > /etc/apt/apt.conf.d/99network
# Packages that pass the checksum stay in apt's cache, so each retry only refetches the corrupted ones.
RUN apt-get update && for i in 1 2 3 4 5 6; do apt-get install -y --no-install-recommends \
        build-essential gcc-11 g++-11 gcc-12 g++-12 \
        cmake ninja-build git python3 python3-matplotlib python3-numpy \
        wget curl ca-certificates unzip xz-utils bzip2 perl \
        libtinfo-dev libtinfo6 libncurses-dev zlib1g-dev libxml2-dev \
        util-linux file less vim \
        && break; echo "apt retry $i"; sleep 5; done \
    && command -v g++-11 && rm -rf /var/lib/apt/lists/*

# Match the host user so files written into the bind mount aren't root-owned.
ARG USER_NAME=dev
ARG USER_UID=1000
ARG USER_GID=1000
RUN userdel -r ubuntu 2>/dev/null || true; \
    groupadd -g ${USER_GID} ${USER_NAME} && \
    useradd -m -u ${USER_UID} -g ${USER_GID} -s /bin/bash ${USER_NAME}

USER ${USER_NAME}
