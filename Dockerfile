# ===========================
# Stage 1: Build Drogon App
# ===========================
FROM ubuntu:22.04 AS builder

ENV TZ=America/New_York
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# Install build tools and dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libjsoncpp-dev \
    uuid-dev \
    openssl \
    postgresql-all \
    libssl-dev \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# Install Drogon
WORKDIR /opt



RUN git clone https://github.com/drogonframework/drogon.git && \
    cd drogon && \
    git submodule update --init && \
    mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc) && make install

# Copy project source
WORKDIR /app
COPY . .

# Clone jwt-cpp (header-only)
RUN git clone https://github.com/Thalhammer/jwt-cpp.git external/jwt-cpp

# Build project
RUN mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DJWT_CPP_INCLUDE_DIR=../external/jwt-cpp/include && \
    make -j$(nproc)

# ===========================
# Stage 2: Runtime
# ===========================
FROM ubuntu:22.04
ENV TZ=America/New_York
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# Install runtime dependencies only
RUN apt-get update && apt-get install -y \
    libjsoncpp-dev \
    uuid-dev \
    libssl-dev \
    postgresql-all \
    openssl \
    zlib1g-dev \
    libmysqlclient-dev \

    && rm -rf /var/lib/apt/lists/*

# Copy compiled binary from builder
WORKDIR /app
COPY --from=builder /app/build/echo /app/echo
COPY --from=builder /app/config.json /app/config.json

# Expose Drogon port
EXPOSE 5555

# Run
CMD ["./echo"]