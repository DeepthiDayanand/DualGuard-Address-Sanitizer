set -e

params=(
        "aes 16 OpenSSL3.3.0 AES"
        "bn 16 OpenSSL3.3.0 RSA"
        "bn 16 OpenSSL1.0.2f RSA"
        "chacha 64 OpenSSL3.3.0 ChaCha20"
        "curve25519 32 OpenSSL3.3.0 Curve25519"
        "poly1305 64 OpenSSL3.3.0 Poly1305"
        "sha256 4096 OpenSSL3.3.0 SHA256"
        "salsa20 16 Libsodium1.0.20 Salsa20"
        "sha256 64 Libsodium1.0.20 SHA256"
        "poly1305 4096 NaCL Poly1305"
        "salsa20 16 NaCL Salsa20"  
        "sha256 64 NaCL SHA256"  
        "mceliece 64 PQClean McEliece"
        "kyber 64 PQClean Kyber"
        )

COMPILE="compile_information.txt"
current_path=$(pwd)
if [ $1 == "compile" ]; then
    echo "Start:" > $COMPILE
    for param in "${params[@]}"; do
        IFS=' ' read -r arg1 arg2 arg3 arg4 <<< "$param"
        echo bash compile.sh $param
        echo $param >> $COMPILE
        bash compile_llsct_free.sh $param >> $COMPILE
        echo "\n" >> $COMPILE
    done
elif [ $1 == "run" ]; then
    cpu_list=$(taskset -cp $$ | tr -d ' ')

    last_field=$(echo "$cpu_list" | awk -F',' '{print $NF}')
    last_cpu=0
    if [[ "$last_field" == *-* ]]; then
        last_cpu=$(echo "$last_field" | cut -d'-' -f2)
    else
        last_cpu=$last_field
    fi

    echo "[" > result.json
    for param in "${params[@]}"; do
        IFS=' ' read -r arg1 arg2 arg3 arg4 <<< "$param"
        echo "{" >> result.json
        echo "  \"benchmark\":\"$arg4\"," >> result.json
        echo "  \"lib\": \"$arg3\"," >> result.json
        echo "  \"size\": \"$arg2\"," >> result.json
        VAR="$current_path/$arg3/$arg1"
        for opt in  origin LightSLH FilterSLH_IR ASLH aSLH BaggyBranch stacking naive-baggy baggy-ASLH baggy-aSLH baggy-seq-spec stacking_sfi sfi-aSLH naive-sfi DP-original stackingDP naive-DP DP-ASLH DP-aSLH DP-seq DP-dual ; do
            echo -e -n "  \"$opt\": \"" >> result.json
            echo python3 run_bench.py "$VAR/$arg1""_test_$opt"   "$arg1""_test" 50 0.5 10 $last_cpu
            python3 run_bench.py "$VAR/$arg1""_test_$opt"   "$arg1""_test" 50 0.5 10 $last_cpu >> result.json
            echo "\","  >> result.json
        done
        sed -i '$ s/.$//' result.json
        echo "}," >> result.json
    done
    sed -i '$ s/.$//' result.json
    echo "]" >> result.json
fi
