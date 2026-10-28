#!/usr/bin/env bash
# Real installed Crypto TLS server vs independent OpenSSL3 SSL_connect()
# test-only client. Detect HRR from the public ServerHello random marker
# without emitting any raw TLS records, certificates or derived key bytes.
set -euo pipefail
server="${1:?installed opaque Crypto C11 TLS server required}"
client="${2:?OpenSSL3-only SSL_connect reference required}"
work="${3:?private TLS fixture scratch directory required}"
base="${4:-9950}"

test -x "$server"
test -x "$client"
test -s "$work/leaf.pem"
test -s "$work/leaf-key.pem"
test -s "$work/ca.pem"
openssl version | grep -q '^OpenSSL 3[.]'

server_pid=""
cleanup() {
  if [[ -n "$server_pid" ]]; then
    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
    server_pid=""
  fi
  rm -f "$work"/api-peer-*.secret "$work"/api-peer-*.private
}
trap cleanup EXIT

index=0
for variant in 12:p256 13:p256 13:hrr; do
  protocol="${variant%%:*}"
  mode="${variant#*:}"
  port="$((base + index))"
  index=$((index + 1))
  native_key="$work/api-peer-$variant-server.secret"
  client_key="$work/api-peer-$variant-client.secret"
  server_log="$work/api-peer-$variant-server.private"
  client_log="$work/api-peer-$variant-client.private"

  "$server" "$protocol" "$port" "$work/leaf.pem" "$work/leaf-key.pem" \
    "$native_key" >"$server_log" 2>&1 &
  server_pid=$!
  sleep 1
  if ! kill -0 "$server_pid" 2>/dev/null; then
    echo "Crypto TLS test server rejected $variant before connection" >&2
    exit 1
  fi
  if ! "$client" "$protocol" "$port" "$work/ca.pem" localhost "$mode" \
       "$client_key" >"$client_log" 2>&1; then
    # Only fixed diagnostic metadata may leave this private work directory.
    grep '^CRYPTO_OPENSSL_API_PEER_RESULT ' "$client_log" >&2 || true
    grep '^CRYPTO_TLS_PRIVATE_STATE ' "$server_log" | tail -25 >&2 || true
    grep '^CRYPTO_TLS_PRIVATE_HRR_STAGE ' "$server_log" | tail -25 >&2 || true
    grep '^CRYPTO_TLS_PRIVATE_HANDSHAKE_DIAG ' "$server_log" >&2 || true
    echo "Crypto/OpenSSL3 SSL_connect $variant failed; no exporter parity claimed" >&2
    exit 1
  fi
  if ! wait "$server_pid"; then
    server_pid=""
    grep '^CRYPTO_OPENSSL_API_PEER_RESULT ' "$client_log" >&2 || true
    grep '^CRYPTO_TLS_PRIVATE_STATE ' "$server_log" | tail -25 >&2 || true
    grep '^CRYPTO_TLS_PRIVATE_HRR_STAGE ' "$server_log" | tail -25 >&2 || true
    echo "Crypto TLS $variant server did not settle successfully" >&2
    exit 1
  fi
  server_pid=""

  test -s "$native_key"
  test -s "$client_key"
  native_hex="$(tr -d '\r\n' < "$native_key")"
  client_hex="$(tr -d '\r\n' < "$client_key")"
  if [[ ! "$native_hex" =~ ^[0-9A-F]{64}$ ]] ||
     [[ ! "$client_hex" =~ ^[0-9A-F]{64}$ ]] ||
     [[ "$native_hex" != "$client_hex" ]]; then
    echo "Crypto/OpenSSL3 real $variant exporter byte mismatch or omission" >&2
    exit 1
  fi
  unset native_hex client_hex
  rm -f "$native_key" "$client_key" "$server_log" "$client_log"
  echo "Crypto server vs independent OpenSSL3 API client $variant: authenticated HTTP and same-session exporter PASS"
done
