#!/usr/bin/env bash
# Real OpenSSL 3 network peer for the *installed Crypto C11 static facade*.
# OpenSSL is a test peer/certificate oracle, NEVER a runtime TLS fallback.
set -euo pipefail

sdk_client="${1:?installed C11 socket-client executable required}"
work="${2:?scratch evidence directory required}"
base_port="${3:-9740}"

test -x "$sdk_client"
openssl version | grep -q '^OpenSSL 3[.]'
rm -rf "$work"
mkdir -p "$work"

openssl req -x509 -newkey rsa:2048 -nodes -sha256 -days 2 \
  -keyout "$work/ca-key.pem" -out "$work/ca.pem" \
  -subj "/CN=Crypto independent OpenSSL CA" \
  -addext "basicConstraints=critical,CA:TRUE" \
  -addext "keyUsage=critical,keyCertSign,cRLSign" \
  >"$work/ca-gen.log" 2>&1
openssl req -new -newkey rsa:2048 -nodes -sha256 \
  -keyout "$work/leaf-key.pem" -out "$work/leaf.csr" \
  -subj "/CN=localhost" >"$work/leaf-gen.log" 2>&1
cat > "$work/leaf.ext" <<'EOF'
basicConstraints=critical,CA:FALSE
keyUsage=critical,digitalSignature,keyEncipherment
extendedKeyUsage=serverAuth
subjectAltName=DNS:localhost
EOF
openssl x509 -req -in "$work/leaf.csr" \
  -CA "$work/ca.pem" -CAkey "$work/ca-key.pem" -CAcreateserial \
  -sha256 -days 2 -extfile "$work/leaf.ext" -out "$work/leaf.pem" \
  >"$work/leaf-sign.log" 2>&1
openssl req -x509 -newkey rsa:2048 -nodes -sha256 -days 2 \
  -keyout "$work/wrong-ca-key.pem" -out "$work/wrong-ca.pem" \
  -subj "/CN=Wrong independent CA" \
  -addext "basicConstraints=critical,CA:TRUE" \
  -addext "keyUsage=critical,keyCertSign,cRLSign" \
  >"$work/wrong-ca-gen.log" 2>&1

# External oracle confirms fixture identity and trust before testing the facade.
openssl verify -CAfile "$work/ca.pem" -purpose sslserver \
  -verify_hostname localhost "$work/leaf.pem" \
  >"$work/fixture-positive.log"
# Independent OpenSSL3 DER/fingerprint oracle for the installed Crypto v1.2
# peer API. Do not share or upload generated private test keys.
openssl x509 -in "$work/leaf.pem" -outform DER -out "$work/leaf.der"
leaf_sha256="$(openssl dgst -sha256 -r "$work/leaf.der" | awk '{print $1}')"
[[ "$leaf_sha256" =~ ^[0-9a-f]{64}$ ]]
if openssl verify -CAfile "$work/wrong-ca.pem" \
  -verify_hostname localhost "$work/leaf.pem" \
  >"$work/fixture-negative.log" 2>&1; then
  echo "Independent wrong CA fixture unexpectedly trusted" >&2
  exit 1
fi

server_pid=""
cleanup() {
  if [[ -n "$server_pid" ]]; then
    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
    server_pid=""
  fi
}
trap cleanup EXIT

for protocol in 12 13; do
  port="$((base_port + protocol - 12))"
  options=(-accept "127.0.0.1:$port"
    -cert "$work/leaf.pem" -key "$work/leaf-key.pem"
    -cert_chain "$work/ca.pem" -www -quiet -alpn "h2,http/1.1")
  if [[ "$protocol" == 12 ]]; then
    options+=(-tls1_2 -cipher ECDHE-RSA-AES128-GCM-SHA256
      -sigalgs rsa_pkcs1_sha256)
  else
    options+=(-tls1_3 -ciphersuites TLS_AES_128_GCM_SHA256
      -sigalgs rsa_pss_rsae_sha256)
  fi
  openssl s_server "${options[@]}" \
    >"$work/openssl-server-$protocol.out" \
    2>"$work/openssl-server-$protocol.err" &
  server_pid=$!
  sleep 1
  if ! kill -0 "$server_pid" 2>/dev/null; then
    cat "$work/openssl-server-$protocol.err" >&2
    echo "Independent OpenSSL $protocol server failed to start" >&2
    exit 1
  fi

  "$sdk_client" "$protocol" "$port" "$work/ca.pem" localhost good \
    "$leaf_sha256" "$work/leaf.der" \
    >"$work/facade-$protocol-good.out" \
    2>"$work/facade-$protocol-good.err"
  "$sdk_client" "$protocol" "$port" "$work/wrong-ca.pem" localhost reject \
    "$leaf_sha256" "$work/leaf.der" \
    >"$work/facade-$protocol-wrong-ca.out" \
    2>"$work/facade-$protocol-wrong-ca.err"
  "$sdk_client" "$protocol" "$port" "$work/ca.pem" wrong.example reject \
    "$leaf_sha256" "$work/leaf.der" \
    >"$work/facade-$protocol-wrong-host.out" \
    2>"$work/facade-$protocol-wrong-host.err"
  cleanup
  echo "Crypto installed facade vs independent OpenSSL 3 TLS $protocol: PASS (good + wrong CA + wrong SAN)"
done

echo "OpenSSL 3 real network peer, trusted CA/hostname and strict rejection: PASS"
