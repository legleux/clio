FROM ubuntu:20.04

RUN apt-get update && \
    apt-get install -y apt-transport-https ca-certificates wget gnupg
RUN mkdir /usr/local/share/keyrings/ && \
    wget -q -O - "https://repos.ripple.com/repos/api/gpg/key/public" | \
    gpg --dearmor > ripple-key.gpg && \
    mv ripple-key.gpg /usr/local/share/keyrings

RUN echo "deb [signed-by=/usr/local/share/keyrings/ripple-key.gpg] https://repos.ripple.com/repos/rippled-deb-test-mirror focal stable" | \
    tee -a /etc/apt/sources.list.d/clio.list
RUN apt-get update && apt-get install -y clio
COPY docker-config.json "/opt/clio/etc/config.json"

COPY /entrypoint.sh /entrypoint.sh
ENTRYPOINT [ "/entrypoint.sh" ]
