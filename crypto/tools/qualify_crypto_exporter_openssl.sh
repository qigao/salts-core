#!/usr/bin/env bash
# Independent exporter BYTE oracle on one real TLS connection.
# Crypto is the verified installed C11 TLS CLIENT; only the separate
# TEST-ONLY server links OpenSSL 3 SSL_export_keying_material().
# Compare omitted context and the CNet RFC9266 present-but-empty context.
set -euo pipefail

crypto_client="${1:?installed Crypto C11 client required}"
openssl_oracle_server="${2:?test-only OpenSSL3 reference server required}"
work="${3:?fixture scratch directory required}"
base_port="${4:-9830}"

test -x "$crypto_client"
test -x "$openssl_oracle_server"
test -s "$work/ca.pem"
test -s "$work/leaf.pem"
test -s "$work/leaf-key.pem"
test -s "$work/leaf.der"
openssl version | grep -q '^OpenSSL 3[.]'
leaf_sha256="$(openssl dgst -sha256 -r "$work/leaf.der" | awk '{print $1}')"
[[ "$leaf_sha256" =~ ^[0-9a-f]{64}$ ]]

server_pid=""
cleanup() {
  if [[ -n "$server_pid" ]]; then
    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
    server_pid=""
  fi
  # Nothing derived from TLS keying material may leave private scratch.
  rm -f "$work"/native-exporter-*.secret \
        "$work"/openssl-exporter-*.secret \
        "$work"/native-exporter-*.private \
        "$work"/openssl-exporter-*.private
}
trap cleanup EXIT

for protocol in 12 13; do
  port="$((base_port + protocol - 12))"
  native_none="$work/native-exporter-$protocol-none.secret"
  native_empty="$work/native-exporter-$protocol-empty.secret"
  openssl_none="$work/openssl-exporter-$protocol-none.secret"
  openssl_empty="$work/openssl-exporter-$protocol-empty.secret"

  "$openssl_oracle_server" "$protocol" "$port" \
    "$work/leaf.pem" "$work/leaf-key.pem" \
    "$openssl_none" "$openssl_empty" \
    >"$work/openssl-exporter-$protocol.private" 2>&1 &
  server_pid=$!
  sleep 1
  if ! kill -0 "$server_pid" 2>/dev/null; then
    echo "Independent OpenSSL3 exporter oracle could not start" >&2
    exit 1
  fi

  # This client is linked ONLY to Crypto::TLS (one private GmSSL DSO).
  # It independently verifies real CA/SAN, h2, DER, SHA256 and HTTP.
  if ! "$crypto_client" "$protocol" "$port" "$work/ca.pem" \
      localhost good "$leaf_sha256" "$work/leaf.der" \
      "$native_none" "$native_empty" \
      >"$work/native-exporter-$protocol.private" 2>&1; then
    echo "Crypto client failed independent OpenSSL3 exporter TLS/HTTP test" >&2
    exit 1
  fi
  if ! wait "$server_pid"; then
    server_pid=""
    echo "Independent OpenSSL3 oracle did not finish TLS/HTTP" >&2
    exit 1
  fi
  server_pid=""

  # Both independently implemented exporters must agree on BOTH RFC
  # context forms, on the SAME session, for TLS 1.2 and TLS 1.3.
  for mode in none empty; do
    native_file="$work/native-exporter-$protocol-$mode.secret"
    openssl_file="$work/openssl-exporter-$protocol-$mode.secret"
    test -s "$native_file"
    test -s "$openssl_file"
    native_hex="$(tr -d '\r\n' < "$native_file")"
    openssl_hex="$(tr -d '\r\n' < "$openssl_file")"
    if [[ ! "$native_hex" =~ ^[0-9A-F]{64}$ ]] ||
       [[ ! "$openssl_hex" =~ ^[0-9A-F]{64}$ ]] ||
       [[ "$native_hex" != "$openssl_hex" ]]; then
      echo "Crypto/OpenSSL3 exporter mismatch or missing data (TLS $protocol context $mode)" >&2
      exit 1
    fi
    unset native_hex openssl_hex
  done

  native_none_hex="$(tr -d '\r\n' < "$native_none")"
  native_empty_hex="$(tr -d '\r\n' < "$native_empty")"
  # TLS 1.2 RFC5705 context-present adds a zero-length vector to the PRF;
  # TLS 1.3 RFC8446 always hashes an empty exporter context.
  if [[ "$protocol" == 12 && "$native_none_hex" == "$native_empty_hex" ]] ||
     [[ "$protocol" == 13 && "$native_none_hex" != "$native_empty_hex" ]]; then
    echo "Crypto TLS $protocol RFC exporter empty-context semantics mismatch" >&2
    exit 1
  fi
  unset native_none_hex native_empty_hex
  rm -f "$native_none" "$native_empty" "$openssl_none" "$openssl_empty" \
        "$work/native-exporter-$protocol.private" \
        "$work/openssl-exporter-$protocol.private"
  echo "Crypto/OpenSSL3 TLS $protocol SAME-SESSION exporter parity PASS (no-context + CNet empty-context + HTTP)"
done

echo "Independent OpenSSL3 RFC5705/8446 and CNet RFC9266 exporter parity: PASS"
