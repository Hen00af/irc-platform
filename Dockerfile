FROM ubuntu:24.04

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        curl \
        netcat-openbsd \
        vim \
        git \
        gdb \
    && rm -rf /var/lib/apt/lists/*
    
WORKDIR /workspace

CMD ["bash"]
