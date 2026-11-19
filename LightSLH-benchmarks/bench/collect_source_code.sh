#!/bin/bash
set -e

mkdir -p crypto_code && cd crypto_code

# === OpenSSL 3.3.0 ===
OPENSSL3_DST=../wrapper/OpenSSL3.3.0
mkdir -p "$OPENSSL3_DST"
if [ ! -d "openssl-3.3.0" ]; then
    if [ ! -f "openssl-3.3.0.tar.gz" ]; then
        echo "[+] Downloading OpenSSL 3.3.0"
        wget https://www.openssl.org/source/openssl-3.3.0.tar.gz
    fi
    tar -xzf openssl-3.3.0.tar.gz
fi
cd openssl-3.3.0
if [ ! -f "libcrypto.a" ]; then
    echo "[+] Building OpenSSL 3.3.0"
    ./Configure linux-x86_64 no-shared no-asm
    make -j4
fi
cd ..
cp openssl-3.3.0/libcrypto.a "$OPENSSL3_DST/"
cp openssl-3.3.0/libssl.a "$OPENSSL3_DST/"

# === OpenSSL 1.0.2f ===
OPENSSL1_DST=../wrapper/OpenSSL1.0.2f
if [ ! -d "openssl-1.0.2f" ]; then
    if [ ! -f "openssl-1.0.2f.tar.gz" ]; then
        echo "[+] Downloading OpenSSL 1.0.2f"
        wget https://www.openssl.org/source/old/1.0.2/openssl-1.0.2f.tar.gz
    fi
    tar -xzf openssl-1.0.2f.tar.gz
fi
cp ../patch/bn_exp.c openssl-1.0.2f/crypto/bn/bn_exp.c
cd openssl-1.0.2f
if [ ! -f "libcrypto.a" ]; then
    echo "[+] Building OpenSSL 1.0.2f"
    ./Configure linux-x86_64 no-shared no-asm
    make -j4
fi
cd ..
cp openssl-1.0.2f/libcrypto.a "$OPENSSL1_DST/"
cp openssl-1.0.2f/libssl.a "$OPENSSL1_DST/"

# === NaCl ===
if [ -d "nacl-20110221" ]; then
    echo "[=] NaCl already unpacked"
else
    if [ ! -f "nacl-20110221.tar.bz2" ]; then
        echo "[+] Downloading NaCl"
        wget https://hyperelliptic.org/nacl/nacl-20110221.tar.bz2 --no-check-certificate
    fi
    tar -xjf nacl-20110221.tar.bz2
fi

# === Libsodium 1.0.20 ===
LIBSODIUM_DST=../wrapper/Libsodium1.0.20
mkdir -p "$LIBSODIUM_DST"
if [ -f "$LIBSODIUM_DST/libsodium.a" ]; then
    echo "[=] Libsodium 1.0.20 already built and copied"
else
    if [ ! -d "libsodium-1.0.20" ]; then
        if [ ! -f "libsodium-1.0.20.tar.gz" ]; then
            echo "[+] Downloading Libsodium 1.0.20"
            wget https://download.libsodium.org/libsodium/releases/libsodium-1.0.20.tar.gz
        fi
        tar -xzf libsodium-1.0.20.tar.gz
    fi
    cd libsodium-1.0.20
    if [ ! -f "src/libsodium/.libs/libsodium.a" ]; then
        echo "[+] Building Libsodium 1.0.20"
        ./configure
        make -j4
    fi
    cd ..
    cp libsodium-1.0.20/src/libsodium/.libs/libsodium.a "$LIBSODIUM_DST/"
fi

# === PQClean ===
if [ -d "PQClean" ]; then
    echo "[=] PQClean already cloned"
else
    echo "[+] Cloning PQClean"
    git clone https://github.com/PQClean/PQClean.git
    cd PQClean
    git checkout 6dc282
    cd ..
fi

# === PQClean-Kyber ===
if [ -d "PQClean-Kyber" ]; then
    echo "[=] PQClean-Kyber already cloned"
else
    echo "[+] Cloning PQClean-Kyber"
    git clone https://github.com/PQClean/PQClean.git PQClean-Kyber
    cd PQClean-Kyber
    git checkout 915ef40
    cd ..
fi

echo "[✓] All crypto code components set up successfully"
