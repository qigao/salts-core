#!/usr/bin/env bash
# Security-negative real TLS 1.3 HRR gate: duplicate and malformed dummy CCS
# MUST be refused by the installed Crypto::TLS server before ClientHello2.
set -euo pipefail

crypto_server="${1:?opaque installed Crypto C11 server required}"
openssl_client="${2:?independent OpenSSL3-only API client required}"
work="${3:?private fixture directory required}"
base="${4:-9970}"

test -x "$crypto_server"
test -x "$openssl_client"
test -s "$work/leaf.pem"
test -s "$work/leaf-key.pem"
test -s "$work/ca.pem"
openssl version | grep -q '^OpenSSL 3[.]'

server_pid=""
proxy_pid=""
cleanup() {
  for pid in "$proxy_pid" "$server_pid"; do
    if [[ -n "$pid" ]]; then
      kill "$pid" 2>/dev/null || true
      wait "$pid" 2>/dev/null || true
    fi
  done
  rm -f "$work"/hrr-negative-*.secret \
        "$work"/hrr-negative-*.private
}
trap cleanup EXIT

index=0
for mode in duplicate malformed; do
  native_port="$((base + 2*index))"
  proxy_port="$((native_port + 1))"
  index="$((index + 1))"
  server_log="$work/hrr-negative-$mode-server.private"
  proxy_log="$work/hrr-negative-$mode-proxy.private"
  client_log="$work/hrr-negative-$mode-client.private"
  server_secret="$work/hrr-negative-$mode-server.secret"
  client_secret="$work/hrr-negative-$mode-client.secret"

  "$crypto_server" 13 "$native_port" "$work/leaf.pem" "$work/leaf-key.pem" \
    "$server_secret" >"$server_log" 2>&1 &
  server_pid=$!
  python3 tools/tls13_hrr_ccs_mutation_proxy.py \
    --listen "$proxy_port" --upstream "$native_port" --mode "$mode" \
    >"$proxy_log" 2>&1 &
  proxy_pid=$!
  sleep 1
  kill -0 "$server_pid" 2>/dev/null || {
    echo "Negative $mode: native TLS server unavailable" >&2
    exit 1
  }
  kill -0 "$proxy_pid" 2>/dev/null || {
    echo "Negative $mode: test mutation proxy unavailable" >&2
    exit 1
  }

  if "$openssl_client" 13 "$proxy_port" "$work/ca.pem" localhost hrr \
       "$client_secret" >"$client_log" 2>&1; then
    echo "SECURITY FAIL: Crypto accepted $mode CCS and authenticated TLS" >&2
    exit 1
  fi

  if ! wait "$proxy_pid"; then
    proxy_pid=""
    echo "Negative $mode: proxy did not inject the required HRR CCS" >&2
    exit 1
  fi
  proxy_pid=""
  if ! grep -q "^CRYPTO_HRR_MUTATION_PROXY injected=$mode$" "$proxy_log"; then
    echo "Negative $mode: mutation admission was not observed" >&2
    exit 1
  fi
  if wait "$server_pid"; then
    server_pid=""
    echo "SECURITY FAIL: native TLS server completed after $mode CCS" >&2
    exit 1
  fi
  server_pid=""
  if [[ -e "$client_secret" || -e "$server_secret" ]]; then
    echo "SECURITY FAIL: key material published despite $mode CCS" >&2
    exit 1
  fi
  rm -f "$server_log" "$client_log" "$proxy_log"
  echo "Crypto TLS1.3 HRR strictly rejects $mode CCS before authentication: PASS"
done
