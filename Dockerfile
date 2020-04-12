FROM ubuntu:18.04 as build

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

FROM ubuntu:18.04

EXPOSE 3478/tcp 3478/udp

RUN apt update && apt install libssl1.1 && apt-get clean

RUN mkdir /opt/stunserver
COPY --from=build /opt/stunserver/stunclient /opt/stunserver/stunclient
COPY --from=build /opt/stunserver/stunserver /opt/stunserver/stunserver

WORKDIR /opt/stunserver

HEALTHCHECK CMD /opt/stunserver/stunclient localhost

ENTRYPOINT ["/opt/stunserver/stunserver"]


