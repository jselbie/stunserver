FROM ubuntu:latest

EXPOSE 3478/tcp 3478/udp

USER root

RUN set -ex && \
    apt-get update && \
    apt-get install -y build-essential && \
    apt-get install -y libboost-all-dev && \
    apt-get install -y libssl-dev && \
    apt-get install -y g++ && \
    apt-get install -y make && \
    apt-get install -y git && \
    apt-get clean -y && \
    rm -rf /var/lib/apt/lists/*

RUN cd /opt && git clone https://github.com/jselbie/stunserver.git && cd stunserver && make

WORKDIR /opt/stunserver

HEALTHCHECK CMD /opt/stunserver/stunclient localhost

ENTRYPOINT ["/opt/stunserver/stunserver"]
