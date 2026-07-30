# syntax=docker/dockerfile:1

# =============================================================================
# Stage 1: builder — полный тулчейн для сборки C++
# =============================================================================
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        git \
        ninja-build \
        pkg-config \
        libboost-system-dev \
        libboost-thread-dev \
        libpqxx-dev \
        libpq-dev \
        libsqlite3-dev \
        ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . .

RUN cmake -S . -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DUSE_POSTGRES=ON \
        -DUSE_SQLITE=ON \
        -DBUILD_TESTS=OFF \
    && cmake --build build --parallel \
    && cmake --install build --prefix /install

# =============================================================================
# Stage 2: runtime — минимальный образ только с рантайм-зависимостями
# =============================================================================
FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        libboost-system1.74.0 \
        libboost-thread1.74.0 \
        libpqxx-6.4 \
        libpq5 \
        libsqlite3-0 \
        ca-certificates \
        curl \
    && rm -rf /var/lib/apt/lists/* \
    && groupadd --system app && useradd --system --gid app --home /app app

WORKDIR /app
COPY --from=builder /install/bin/url_shortener_server /app/url_shortener_server

RUN mkdir -p /app/data && chown -R app:app /app
USER app

ENV SERVER_HOST=0.0.0.0 \
    SERVER_PORT=8080 \
    THREAD_POOL_SIZE=8 \
    DB_TYPE=postgres \
    SQLITE_PATH=/app/data/url_shortener.db

EXPOSE 8080

HEALTHCHECK --interval=15s --timeout=3s --start-period=10s --retries=5 \
    CMD curl -f http://localhost:8080/health || exit 1

ENTRYPOINT ["/app/url_shortener_server"]
