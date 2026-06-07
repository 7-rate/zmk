FROM docker.io/zmkfirmware/zmk-dev-arm:4.1-branch

RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates curl && \
	curl -1sLf 'https://dl.cloudsmith.io/public/task/task/setup.deb.sh' | bash && \
	apt-get update && apt-get install -y --no-install-recommends task && \
	rm -rf /var/lib/apt/lists/*

COPY .bashrc tmp
RUN mv /tmp/.bashrc ~/.bashrc
