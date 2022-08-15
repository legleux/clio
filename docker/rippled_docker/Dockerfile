FROM ubuntu:20.04 AS pre

RUN apt-get update && apt install -y \
  apt-transport-https \
  ca-certificates \
  gnupg \
  wget

RUN wget -q -O - "https://repos.ripple.com/repos/api/gpg/key/public" | apt-key add -
RUN echo "deb https://repos.ripple.com/repos/rippled-deb focal unstable" | tee -a /etc/apt/sources.list.d/ripple.list

RUN apt-get update && apt-get install -y rippled
RUN apt-get install -y vim curl ncdu jq

FROM ubuntu:20.04

<<<<<<< HEAD
COPY cfg/rippled-*.cfg /opt/ripple/etc/
COPY cfg/validators-*.txt /opt/ripple/etc/
=======
COPY config/rippled-*.cfg /opt/ripple/etc/
COPY config/validators-*.txt /opt/ripple/etc/
>>>>>>> push-docker-image
COPY check_state.sh /
COPY --from=pre /usr/bin/jq /usr/bin/jq
COPY --from=pre /lib/x86_64-linux-gnu/libjq.so.1 /lib/x86_64-linux-gnu/libjq.so.1
COPY --from=pre /lib/x86_64-linux-gnu/libonig.so.5 /lib/x86_64-linux-gnu/libonig.so.5
COPY --from=pre /opt/ripple/ /opt/ripple/

ADD ./entrypoint.sh /entrypoint.sh

# EXPOSE 5005 5006 6005 50051 51235

ENTRYPOINT ["./entrypoint.sh"]
