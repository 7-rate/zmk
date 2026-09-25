FROM docker.io/zmkfirmware/zmk-dev-arm:4.1-branch

# Avoid interactive apt prompts when this image is built non-interactively (e.g. CI)
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates curl && \
	curl -1sLf 'https://dl.cloudsmith.io/public/task/task/setup.deb.sh' | bash && \
	apt-get update && apt-get install -y --no-install-recommends task && \
	rm -rf /var/lib/apt/lists/*

COPY .bashrc tmp
RUN mv /tmp/bashrc ~/.bashrc

# Also used as the west workspace root for CI firmware builds (see
# .github/workflows/firmware-release.yml); matches the devcontainer workspace folder.
WORKDIR /workspaces/zmk
