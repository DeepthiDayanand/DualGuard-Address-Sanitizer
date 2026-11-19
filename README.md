# DualGuard: Comprehensive and Efficient Memory Safety across Sequential and Speculative Execution

- [Intro](#intro)
- [Getting Started](#getting-started)
    - [Using Pre-built DualGuard with Docker](#using-pre-built-dualguard-with-docker)
    - [Building DualGuard from Source](#building-dualguard-from-source)
        - [System Requirements](#system-requirements)
        - [Building the Toolchain](#building-the-toolchain)
    - [Building from Source inside a Docker Build Environment](#building-from-source-inside-a-docker-build-environment)
- [Smoke Test](#smoke-test)
- [Running the Benchmarks](#running-the-benchmarks)
    - [Running the Benchmarks in the Docker Build Environment](#running-the-benchmarks-in-the-docker-build-environment)
- [CVE-2019-3859 Evaluation](#cve-2019-3859-evaluation)

## Intro

This repository contains the source code for the peer-reviewed paper and has been anonymized for review. The sections below explain how to use this repository and reproduce the paper’s results. It includes all the source code of DualGuard, tool chain, and benchmarks except SPEC CPU 2017 benchmark suite that cannot be shared due to copy right issue. DualGuard can be either run by using the pre-built Docker image or by building from source. We recommand using the pre-built Docker image to ease the burden on setting up the tool chain.

## Getting Started

### Using Pre-built DualGuard with Docker

Make sure the test machine has Docker installed. We provide the docker image inside the anonymized Dockerhub [repo](https://hub.docker.com/r/dualguard/dualguard). Get the container running with the following commands. 

```sh
# Pull Docker image from DockerHub
docker pull dualguard/dualguard:latest

# Run a container with the image
docker run --name dualguard -dit dualguard/dualguard

# Get into the container's bash
docker exec -it dualguard bash
```

Inside the container, the artifiact is located at `/root/dualguard`. DualGuard has been built in the image, but SPEC CPU 2017 benchmark suite have to be installed manually due to copy right reason. Please install the benchmark suite according to its instruction.

After the benchmark suite is successfully installed, edit `SPEC_PATH` to point to the installation directory, and run the setup script.

```sh
# Set up benchmark harness for SPEC
bash ./setup.sh
```

### Building DualGuard from Source

#### System Requirements

We have tested building the system under Ubuntu 24.04, which provides the best compatibility of softwares used. Please have 30 GB of disk space available on your system. The instructions in this documentation assumes the user is running this version of Ubuntu Linux.

#### Building the Toolchain

First, install the dependency.

```shell
sudo apt update
sudo apt install git build-essential python3 ninja-build wget cmake libtinfo-dev libtinfo6
```

Then, download the source code from this anonymous [repo](https://anonymous.4open.science/r/DualGuard-B244), and decompress the source code.

```shell
# Download the source code
wget https://anonymous.4open.science/api/repo/DualGuard-B244/zip -O dualguard.zip

# Decompress the source code
unzip dualguard.zip -d dualguard
```

Finally, navigate into the root directory of the source code and run the building script.

**Important:** Before running the building script, install SPEC CPU 2017 benchmark suite and edit `SPEC_PATH` to point to the installation directory.

```shell
cd dualguard
bash ./build.sh
```

### Building from Source inside a Docker Build Environment

If your host is not Ubuntu 24.04 (e.g. a newer distribution whose CMake/glibc no longer build LLVM 14 cleanly), or you want to modify DualGuard and rebuild it, use the build environment in `docker/`. Unlike the pre-built image, the source tree stays on the host and is bind-mounted into an Ubuntu 24.04 container, so edits made on the host are picked up by the next build.

`dev.sh` wraps everything:

| Command | What it does |
| --- | --- |
| `./dev.sh image` | Build the `dualguard-dev` image from `docker/Dockerfile` |
| `./dev.sh shell` | Open an interactive shell in the container |
| `./dev.sh run <cmd...>` | Run a command in the container, from the repo root |
| `./dev.sh bg <name> <cmd...>` | Same as `run`, but detached as a container named `<name>` |

Notes on how the container is set up:

- The repository is mounted at the **same absolute path** as on the host, because LLVM's build tree records absolute paths. Its **parent directory** is mounted too, because `SPEC_PATH` defaults to `../cpu2017`.
- The image creates a user with your UID/GID, so files written into the repo are owned by you, not root. Rebuild the image (`./dev.sh image`) if you build it as one user and run it as another.
- Containers are started with `--cap-add=SYS_NICE`, which the benchmark scripts need to pin processes to a CPU with `taskset`.

1. Install Docker, and make sure your user can run it without `sudo`:

    ```shell
    sudo usermod -aG docker $USER   # then log out and back in (on WSL: wsl --shutdown)
    ```

    Until the new group takes effect, you can prefix commands with `sg docker -c "..."`, e.g. `sg docker -c "./dev.sh image"`.

2. Build the image (a few minutes; apt downloads are retried automatically if a package arrives corrupted):

    ```shell
    ./dev.sh image
    ```

3. (Optional) Install SPEC CPU 2017 at `../cpu2017`, or edit `SPEC_PATH` in `build.sh` and `run.sh` to point to it. The installation must be under the repository's parent directory, otherwise it is not visible inside the container.

4. Build the toolchain and benchmarks. This builds SVF, the patched LLVM 14, the analyses, Baggy Bounds, Delta Pointers and the LightSLH benchmarks; building LLVM alone can take hours. Run it detached so it survives closing the terminal:

    ```shell
    ./dev.sh bg dualguard-build bash -c './build.sh > build.log 2>&1; echo "EXIT $?" >> build.log'
    tail -f build.log
    ```

    The last line of `build.log` is `EXIT 0` on success. If SPEC CPU 2017 is not installed, the final step ("Setting up SPEC benchmarks ...") fails and the log ends with a non-zero exit code; everything needed for the LightSLH benchmarks has already been built at that point. That failed step also leaves a stray file at `../cpu2017`, which you should delete before installing SPEC there.

    `build.sh` downloads several large archives (LLVM, Z3, the crypto libraries used as benchmarks). If one of them fails or is corrupted, delete the partial download and re-run the script.

To rebuild a single component after changing its source, run its build command inside the container, e.g.:

```shell
./dev.sh run bash -c 'cd llvm-project-14.0.0.src/build && ninja -j10'
./dev.sh run bash -c 'cd Unified-Memory-Safety-Validation/build && ninja'
```

## Smoke Test

To verify that the environment is configured correctly before launching the full evaluation via `run.sh`:

1. Open `REPO_ROOT_DIR/LightSLH-benchmarks/bench/wrapper/bench_llsct_free.sh`.
2. Locate the `params` array (which contains 14 benchmark configurations by default).
3. Remove all entries except "aes 16 OpenSSL3.3.0 AES".
4. Run `run.sh lightslh` (or `./dev.sh run ./run.sh lightslh` with the [Docker build environment](#building-from-source-inside-a-docker-build-environment)).

This smoke test tries all variants across a single representative workload and completes in under 20 minutes depending on the underlying hardware. After everything successfully run, restore `params` array.

## Running the Benchmarks

**Important:** edit `SPEC_PATH` in `run.sh` to point to the installation directory of SPEC CPU 2017 benchmark suite. Running the benchmarks with the following commands accordingly.

```shell
# run SPEC benchmarks
./run.sh spec

# run LightSLH benchmarks
./run.sh lightslh

# run both
./run.sh
```

**Note:** If SPEC CPU 2017 benchmark suite is not available, run `run.sh lightslh` to run the LightSLH benchmarks only.

### Running the Benchmarks in the Docker Build Environment

When DualGuard was built with `dev.sh`, run the benchmarks in the same container. The full LightSLH run compiles every benchmark under every protection variant and then measures each one, which takes many hours, so run it detached:

```shell
# LightSLH benchmarks
./dev.sh bg dualguard-lightslh bash -c './run.sh lightslh > lightslh.log 2>&1; echo "EXIT $?" >> lightslh.log'
tail -f lightslh.log

# SPEC benchmarks (requires SPEC CPU 2017, see above)
./dev.sh bg dualguard-spec bash -c './run.sh spec > spec.log 2>&1; echo "EXIT $?" >> spec.log'
```

A run is finished when the log ends with `EXIT <code>` (`0` on success). `docker ps` lists runs that are still in progress, and `docker stop <name>` cancels one.

Timing measurements are pinned with `taskset` to the last CPU available to the container. For stable numbers, keep the machine otherwise idle during the run; you can restrict the container's CPUs by adding `--cpuset-cpus` to `DOCKER_ARGS` in `dev.sh`.

The LightSLH results are written to `LightSLH-benchmarks/bench/wrapper/result.json`, with per-benchmark compile output in `compile_information.txt` next to it.

## CVE-2019-3859 Evaluation

The evaluation focuses on spatial memory safety with AddressSanitizer (ASan). It does not assess information flow.

The harness at [`cve-2019-3859/reproducer.c`](cve-2019-3859/reproducer.c) injects a malformed, already-decrypted SSH response into libssh2's packet queue and exercises `libssh2_userauth_list()`.

| Build | Result |
| --- | --- |
| libssh2 1.8.0, unprotected | Returned an empty method list; no crash. |
| libssh2 1.8.0, ASan | Reported a heap-buffer-overflow read in `memmove`. |
| libssh2 1.8.1 | Rejected the response with `Unexpected userauth list size`. |

This in-process test isolates the parser; it is not an end-to-end SSH session test. DualGuard results and runtime overhead have not yet been measured. See the [evaluation notes](CVE-2019-3859-ASAN-Evaluation.md) for setup details and limitations.
