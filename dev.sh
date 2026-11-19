#!/bin/bash
# Docker build environment for DualGuard.
#   ./dev.sh image          build the image
#   ./dev.sh shell          interactive shell in the container
#   ./dev.sh run <cmd...>   run a command in the container (from the repo root)
#   ./dev.sh bg <name> <cmd...>  same, but detached (docker logs -f <name>)
set -e

ROOT_DIR=$(cd "$(dirname "$0")" && pwd)
IMAGE=dualguard-dev

# Mount the repo at the same absolute path: LLVM's build tree records absolute paths.
# The parent is mounted too because SPEC_PATH defaults to ../cpu2017.
DOCKER_ARGS=(--rm --init
             -v "$(dirname "$ROOT_DIR")":"$(dirname "$ROOT_DIR")"
             -w "$ROOT_DIR"
             --cap-add=SYS_NICE)

case "$1" in
    image)
        docker build --network=host -t $IMAGE \
            --build-arg USER_NAME="$(id -un)" \
            --build-arg USER_UID="$(id -u)" \
            --build-arg USER_GID="$(id -g)" \
            "$ROOT_DIR/docker"
        ;;
    shell)
        docker run -it "${DOCKER_ARGS[@]}" $IMAGE bash
        ;;
    run)
        shift
        docker run "${DOCKER_ARGS[@]}" $IMAGE "$@"
        ;;
    bg)
        # Detached, so it survives the terminal/session that started it.
        # Follow with: docker logs -f <name>
        shift; NAME=$1; shift
        docker run -d --name "$NAME" "${DOCKER_ARGS[@]}" $IMAGE "$@"
        ;;
    *)
        sed -n '2,5p' "$0"
        exit 1
        ;;
esac
