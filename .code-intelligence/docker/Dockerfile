FROM ubuntu

WORKDIR /tmp/vast

RUN apt-get update && \
    apt-get -y --no-install-recommends install \
      build-essential \
      ca-certificates \
      cmake \
      flatbuffers-compiler-dev \
      g++-10 \
      gcc-10 \
      clang-12 \
      git-core \
      gnupg2 \
      jq \
      libcaf-dev \
      libbroker-dev \
      libflatbuffers-dev \
      libfmt-dev \
      libpcap-dev tcpdump \
      libsimdjson-dev \
      libssl-dev \
      libunwind-dev \
      libxxhash-dev \
      lsb-release \
      ninja-build \
      pkg-config \
      python3-dev \
      python3-pip \
      python3-venv \
      robin-map-dev \
      libprotobuf-dev 

#need to installed separetely otherwise there will be unmet dependency conflict with libunwind-dev
RUN apt-get update && apt-get install -y libc++-dev

RUN rm -rf /var/lib/apt/lists/* *.deb

COPY ci-fuzz-2.31.0 /opt/ci-fuzz-2.31.0

ADD install_arrow.sh /
RUN chmod +x /install_arrow.sh
RUN /install_arrow.sh

ADD install_spdlog.sh /
RUN chmod +x /install_spdlog.sh
RUN /install_spdlog.sh

ADD install_yamlcpp.sh /
RUN chmod +x /install_yamlcpp.sh
RUN /install_yamlcpp.sh

RUN rm -rf /opt/ci-fuzz-2.31.0
RUN rm -rf /tmp/vast/*
