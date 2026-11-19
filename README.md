# DualGuard: Comprehensive and Efficient Memory Safety across Sequential and Speculative Execution

This directory contains the source code and evaluation artifacts for the peer-reviewed DualGuard paper. The repository has been anonymized for review and includes DualGuard, its toolchain, and the benchmarks used to reproduce the paper's results. The SPEC CPU 2017 benchmark suite is not included due to copyright restrictions.

DualGuard can be used either through a pre-built Docker image or by building the toolchain from source. The pre-built image is the recommended option for users who want to reproduce the evaluation without manually configuring the development environment. For users who want to modify DualGuard or rebuild individual components, a separate Docker-based build environment is provided.

## Contents

1. [Getting Started](#1-getting-started)
   - [Using Pre-built DualGuard with Docker](#11-using-pre-built-dualguard-with-docker)
   - [Building DualGuard from Source](#12-building-dualguard-from-source)
   - [System Requirements](#121-system-requirements)
   - [Building the Toolchain](#122-building-the-toolchain)
   - [Building from Source inside a Docker Build Environment](#123-building-from-source-inside-a-docker-build-environment)
2. [Smoke Test](#2-smoke-test)
3. [Running the Benchmarks](#3-running-the-benchmarks)
   - [Running the Benchmarks in the Docker Build Environment](#31-running-the-benchmarks-in-the-docker-build-environment)
4. [CVE-2019-3859 Evaluation](#4-cve-2019-3859-evaluation)

---

## 1. Getting Started

DualGuard can be set up in two ways:

- **Pre-built Docker image:** Recommended for reproducing the evaluation. The image includes an already-built DualGuard toolchain.
- **Building from source:** Intended for users who want to modify DualGuard, rebuild its components, or configure the development environment themselves.

The SPEC CPU 2017 benchmark suite must be installed separately in either configuration because it cannot be distributed with the repository.

### 1.1 Using Pre-built DualGuard with Docker

Make sure Docker is installed on the test machine. The pre-built Docker image is available from the anonymized [Docker Hub repository](https://hub.docker.com/r/dualguard/dualguard).

Pull the image and start a container:

```bash
# Pull the Docker image from Docker Hub
docker pull dualguard/dualguard:latest

# Start a container
docker run --name dualguard -dit dualguard/dualguard

# Open a bash shell inside the container
docker exec -it dualguard bash
```

Inside the container, the artifact is located at `/root/dualguard`.

DualGuard has already been built in the Docker image. However, the SPEC CPU 2017 benchmark suite must be installed manually due to copyright restrictions. Install it according to the suite's installation instructions.

After installing SPEC CPU 2017:

1. Edit `SPEC_PATH` to point to the benchmark installation directory.
2. Run the setup script to configure the SPEC benchmark harness.

```bash
# Set up the benchmark harness for SPEC
bash ./setup.sh
```

The environment is now ready to run the benchmarks.

### 1.2 Building DualGuard from Source

Users who want to build DualGuard from source should first configure the required dependencies and then build the toolchain.

#### 1.2.1 System Requirements

The system has been tested on **Ubuntu 24.04**, which provides the best compatibility with the software dependencies used by DualGuard.

The following requirements apply:

- Ubuntu 24.04 (recommended).
- At least 30 GB of available disk space.
- Git, CMake, Ninja, Python 3, and the other dependencies listed below.
- SPEC CPU 2017, installed separately, if reproducing the full benchmark evaluation.

The instructions below assume that the user is running Ubuntu 24.04.

#### 1.2.2 Building the Toolchain

First, install the required dependencies:

```bash
sudo apt update
sudo apt install git build-essential python3 ninja-build wget cmake libtinfo-dev libtinfo6
```

Next, download the source code from the anonymized [DualGuard repository](https://anonymous.4open.science/r/DualGuard-B244) and extract the archive:

```bash
# Download the source code
wget https://anonymous.4open.science/api/repo/DualGuard-B244/zip -O dualguard.zip

# Extract the source code
unzip dualguard.zip -d dualguard
```

Navigate to the root directory of the extracted source code and execute the build script.

**Important:** Before running the build script, install the SPEC CPU 2017 benchmark suite and edit `SPEC_PATH` to point to its installation directory.

```bash
cd dualguard
bash ./build.sh
```

The build script compiles the required toolchain and benchmark components.

#### 1.2.3 Building from Source inside a Docker Build Environment

For systems that do not run Ubuntu 24.04 natively, or for users who want to modify DualGuard and rebuild its components, the repository provides a Docker-based development environment in `docker/`.

This environment uses an Ubuntu 24.04 container while keeping the source tree on the host machine. The repository is bind-mounted into the container, so changes made on the host are available during subsequent builds.

The `dev.sh` script provides the main interface for managing the development environment.

| Command | Description |
| --- | --- |
| `./dev.sh image` | Build the `dualguard-dev` image using `docker/Dockerfile`. |
| `./dev.sh shell` | Open an interactive shell inside the container. |
| `./dev.sh run <cmd...>` | Execute a command inside the container, starting from the repository root. |
| `./dev.sh bg <name> <cmd...>` | Execute a command in a detached container with the specified name. |

**Container configuration notes:**

- **Repository mounting:** The repository is mounted at the same absolute path inside the container as on the host. This is necessary because LLVM's build tree records absolute paths. The repository's parent directory is also mounted because `SPEC_PATH` defaults to `../cpu2017`.
- **User permissions:** The image creates a user with the host's UID and GID. Files generated inside the container are therefore owned by the host user rather than root. Rebuild the image using `./dev.sh image` if the environment is built under one user and subsequently run under another.
- **CPU permissions:** Containers are started with `--cap-add=SYS_NICE`, which allows benchmark scripts to pin processes to a CPU using `taskset`.

**Setup procedure**

1. Install Docker and configure your user to run Docker commands without `sudo`:

   ```bash
   sudo usermod -aG docker $USER
   ```

   Log out and back in for the new group membership to take effect. On WSL, run:

   ```bash
   wsl --shutdown
   ```

   Until the new group membership takes effect, commands can be prefixed with `sg docker -c`, for example:

   ```bash
   sg docker -c "./dev.sh image"
   ```

2. Build the development image:

   ```bash
   ./dev.sh image
   ```

   The build takes a few minutes. Package downloads are retried automatically if an archive arrives corrupted.

3. (Optional) Install SPEC CPU 2017 at `../cpu2017`, or edit `SPEC_PATH` in `build.sh` and `run.sh` to point to the installation directory.

   The SPEC installation must be located under the repository's parent directory to make it accessible inside the container.

4. Build the toolchain and benchmarks.

   The build process compiles SVF, the patched LLVM 14 toolchain, the analyses, Baggy Bounds, DeltaPointers, and the LightSLH benchmarks. Building LLVM alone can take several hours.

   Run the build in the background so that it continues even if the terminal is closed:

   ```bash
   ./dev.sh bg dualguard-build bash -c './build.sh > build.log 2>&1; echo "EXIT $?" >> build.log'
   ```

   Monitor the build:

   ```bash
   tail -f build.log
   ```

   The final line of `build.log` indicates the exit status. A successful build ends with:

   ```text
   EXIT 0
   ```

   If SPEC CPU 2017 is not installed, the final step (`Setting up SPEC benchmarks ...`) fails. However, all components required for the LightSLH benchmarks have already been built at that point.

   The failed setup step also leaves a temporary file at `../cpu2017`. Delete this file before installing SPEC CPU 2017 in that location.

   **Build troubleshooting:** The build script downloads several large archives, including LLVM, Z3, and the cryptographic libraries used by the benchmarks. If a download fails or an archive is corrupted, remove the incomplete download and rerun the build script.

**Rebuilding individual components**

After modifying a component, its build can be rerun independently inside the Docker environment.

For example:

```bash
# Rebuild LLVM
./dev.sh run bash -c 'cd llvm-project-14.0.0.src/build && ninja -j10'

# Rebuild the DualGuard passes
./dev.sh run bash -c 'cd Unified-Memory-Safety-Validation/build && ninja'
```

---

## 2. Smoke Test

Before running the full evaluation through `run.sh`, perform a smoke test to verify that the development environment is configured correctly.

The smoke test uses one representative LightSLH benchmark configuration instead of the full set of 14 benchmarks.

**Procedure:**

1. Open `REPO_ROOT_DIR/LightSLH-benchmarks/bench/wrapper/bench_llsct_free.sh`.
2. Locate the `params` array, which contains 14 benchmark configurations by default.
3. Remove all entries except:

   ```text
   aes 16 OpenSSL3.3.0 AES
   ```

4. Run the LightSLH benchmark script:

   ```bash
   ./run.sh lightslh
   ```

   If using the Docker build environment, execute:

   ```bash
   ./dev.sh run ./run.sh lightslh
   ```

The smoke test compiles and runs all protection variants for the selected workload. Depending on the underlying hardware, it should complete in under 20 minutes.

After confirming that the benchmark runs successfully, restore the original `params` array before starting the full evaluation.

---

## 3. Running the Benchmarks

DualGuard supports two benchmark suites:

- **SPEC CPU 2017:** Requires a separate installation of the SPEC CPU 2017 benchmark suite.
- **LightSLH:** Included in the repository and can be executed without SPEC CPU 2017.

**Important:** Before running the SPEC benchmarks, edit `SPEC_PATH` in `run.sh` to point to the SPEC CPU 2017 installation directory.

From the repository root, execute the following commands:

```bash
# Run SPEC CPU 2017 benchmarks
./run.sh spec

# Run LightSLH benchmarks
./run.sh lightslh

# Run both benchmark suites
./run.sh
```

**Note:** If SPEC CPU 2017 is unavailable, execute `./run.sh lightslh` to run the LightSLH benchmarks independently.

### 3.1 Running the Benchmarks in the Docker Build Environment

When DualGuard has been built using `dev.sh`, execute the benchmark scripts inside the same Docker development environment.

The full LightSLH evaluation compiles every benchmark under every protection variant and subsequently measures each configuration. This process takes several hours, so it is recommended to execute the benchmarks in detached containers.

**LightSLH benchmarks:**

```bash
./dev.sh bg dualguard-lightslh bash -c './run.sh lightslh > lightslh.log 2>&1; echo "EXIT $?" >> lightslh.log'

# Monitor the benchmark run
tail -f lightslh.log
```

**SPEC CPU 2017 benchmarks:**

```bash
./dev.sh bg dualguard-spec bash -c './run.sh spec > spec.log 2>&1; echo "EXIT $?" >> spec.log'
```

The SPEC benchmark run requires a valid SPEC CPU 2017 installation and a correctly configured `SPEC_PATH`.

**Monitoring and managing benchmark runs**

A benchmark run is complete when its log ends with:

```text
EXIT <code>
```

An exit code of `0` indicates successful completion.

The following Docker commands can be used to monitor and manage benchmark execution:

```bash
# List running containers
docker ps

# Stop a running benchmark container
docker stop <name>
```

**Timing and reproducibility**

Timing measurements are pinned using `taskset` to the last CPU available to the container.

For more stable measurements:

- Keep the host machine otherwise idle during benchmark execution.
- Avoid running other CPU-intensive processes concurrently.
- If necessary, restrict the CPUs available to the container by adding `--cpuset-cpus` to `DOCKER_ARGS` in `dev.sh`.

**Benchmark output**

The LightSLH benchmark results are written to:

```text
LightSLH-benchmarks/bench/wrapper/result.json
```

Per-benchmark compilation output is stored in `compile_information.txt` in the same directory.

---

## 4. CVE-2019-3859 Evaluation

This evaluation examines DualGuard's effectiveness against spatial memory-safety violations using CVE-2019-3859.

The evaluation focuses specifically on **spatial memory safety** using AddressSanitizer (ASan). It does not evaluate information-flow security.

**Test setup**

The reproducer is located at:

```text
cve-2019-3859/reproducer.c
```

The harness injects a malformed, already-decrypted SSH response into libssh2's packet queue and exercises `libssh2_userauth_list()`.

The evaluation compares three configurations:

| Build | Result |
| --- | --- |
| libssh2 1.8.0, unprotected | Returned an empty authentication-method list without crashing. |
| libssh2 1.8.0, ASan | Reported a heap-buffer-overflow read in `memmove`. |
| libssh2 1.8.1 | Rejected the malformed response with `Unexpected userauth list size`. |

**Interpretation**

The test demonstrates that the vulnerable libssh2 1.8.0 implementation contains a heap-buffer-overflow that is detected by AddressSanitizer. The patched libssh2 1.8.1 implementation rejects the malformed response before the vulnerable memory operation occurs.

This is an in-process test that isolates the packet parser. It is not an end-to-end SSH session evaluation.

**Limitations**

- The test evaluates spatial memory safety, not information-flow security.
- The reproducer exercises the vulnerable parsing path directly rather than establishing a complete SSH session.
- DualGuard's protection results and runtime overhead have not yet been measured for this evaluation.

For additional setup details and limitations, see the CVE-2019-3859 ASan evaluation notes.
