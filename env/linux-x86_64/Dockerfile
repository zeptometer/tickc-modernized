ARG BASE_IMAGE=debian:trixie-slim@sha256:020c0d20b9880058cbe785a9db107156c3c75c2ac944a6aa7ab59f2add76a7bd
FROM ${BASE_IMAGE}

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        binutils \
        bison \
        diffutils \
        file \
        flex \
        gcc \
        libc6-dev \
        make \
        patch \
        perl \
        valgrind \
        xutils-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
