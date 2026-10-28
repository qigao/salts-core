#!/usr/bin/env bash
# Independent OpenSSL3 s_client against an actual installed Salts::Crypto
# SERVER. Keep the reverse-role gate separate from the already-qualified
# Crypto-client/OpenSSL-server exporter oracle.
set -euo pipefail

crypto_server="${1:?installed opaque C11 Crypto TLS server required}"
work="${2:?existing private OpenSSL fixture scratch dir required}"
base_port="${3:-9900}"

test -x "$crypto_server"
test -s "$work/ca.pem"
test -s "$work/leaf.pem"
test -s "$work/leaf-key.pem"
openssl version | grep -q '^OpenSSL 3[.]'

server_pid=""
cleanup() {
  if [[ -n "$server_pid" ]]; then
    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
  fi
  # Unlink transient material on EVERY exit including failed handshake.
  rm -f "$work"/reverse-crypto-*.secret \
        "$work"/reverse-openssl-*.private \
        "$work"/reverse-server-*.private
}
trap cleanup EXIT

if [[ "${CRYPTO_TLS_TEST_HRR_ONLY:-0}" == 1 ]]; then
  protocols=(13)
  peer_mode="default-hrr"
else
  protocols=(12 13)
  peer_mode="single-p256"
fi

for proto in "${protocols[@]}"; do
  port="$((base_port + proto - 12))"
  server_log="$work/reverse-server-$proto.private"
  openssl_log="$work/reverse-openssl-$proto.private"
  secret="$work/reverse-crypto-$proto.secret"

  "$crypto_server" "$proto" "$port" "$work/leaf.pem" \
    "$work/leaf-key.pem" "$secret" >"$server_log" 2>&1 &
  server_pid=$!
  sleep 1
  kill -0 "$server_pid" 2>/dev/null || {
    echo "Crypto TLS $proto test server did not enter accept" >&2; exit 1;
  }

  if ! python3 - "$proto" "$port" "$work/ca.pem" "$openssl_log" "$peer_mode" <<'PY'
from pathlib import Path
import os
import re
import signal
import subprocess
import sys

protocol, port, ca_path, logfile, peer_mode = sys.argv[1:]
args = [
    "openssl", "s_client", "-connect", "127.0.0.1:" + port,
    "-servername", "localhost",
    "-CAfile", ca_path, "-verify_return_error",
    "-verify_hostname", "localhost",
    "-keymatexport", "EXPORTER-Channel-Binding", "-keymatexportlen", "32",
    "-ign_eof", "-ignore_unexpected_eof", "-state",
]
if protocol == "12":
    args += ["-tls1_2", "-cipher", "ECDHE-RSA-AES128-GCM-SHA256",
             "-sigalgs", "rsa_pkcs1_sha256"]
else:
    args += ["-tls1_3", "-ciphersuites", "TLS_AES_128_GCM_SHA256",
             "-sigalgs", "rsa_pss_rsae_sha256"]
    if peer_mode == "single-p256":
        args += ["-groups", "P-256"]  # normal single-keyshare qualification
    elif peer_mode != "default-hrr":
        raise SystemExit("invalid peer_mode")
with Path(logfile).open("wb") as output:
    proc = subprocess.Popen(
        args, stdin=subprocess.PIPE, stdout=output, stderr=subprocess.STDOUT,
        start_new_session=True,
    )
    try:
        proc.communicate(
            input=b"GET / HTTP/1.0\r\nHost: localhost\r\n\r\n", timeout=22
        )
    except subprocess.TimeoutExpired:
        os.killpg(proc.pid, signal.SIGKILL)
        proc.communicate()
        print("Reverse OpenSSL peer timed out, TLS " + protocol,
              file=sys.stderr)
        sys.exit(1)
    if proc.returncode:
        log = Path(logfile).read_text(errors="replace")
        # This output contains exporter material. Do not print it!
        state_lines = [
            line for line in log.splitlines()
            if line.startswith("SSL_connect:")
        ]
        # Fixed/whitelisted phase labels only, NO TLS transcript or keys.
        phase_class = {
            "server_hello": any("read server hello" in l.lower() for l in state_lines),
            "server_certificate": any("read server certificate" in l.lower() for l in state_lines),
            "server_key_exchange": any("read server key exchange" in l.lower() for l in state_lines),
            "client_finished": any("write finished" in l.lower() for l in state_lines),
            "server_finished": any("read finished" in l.lower() for l in state_lines),
            "application_data": "HTTP/1.0 200" in log,
            "exporter_marker": "Keying material:" in log,
        }
        safe_errors = [
            "bad record mac", "decryption failed or bad record mac",
            "decrypt error", "certificate verify failed",
            "wrong signature type", "invalid signature",
            "no suitable signature algorithm", "unexpected message",
            "handshake failure", "no shared cipher",
            "unsupported protocol", "illegal parameter",
            "unsupported extension", "wrong version number",
            "record overflow", "decode error", "internal error",
            "tlsv1 alert decrypt error", "bad key share",
        ]
        # These are *literal fixed reason categories*, never raw OpenSSL
        # output (which contains ephemeral exported secrets).
        reason_flags = [reason for reason in safe_errors if reason in log.lower()]
        # Extract ONLY cryptographic diagnostic reason identifiers, not
        # OpenSSL's raw log which contains private keying material.
        detail = []
        for line in log.splitlines():
            m = re.search(
                r"error:[0-9a-fA-F]+:SSL routines:(?:[A-Za-z0-9_-]*:)?"
                r"([A-Za-z][A-Za-z0-9 _-]{1,79})", line
            )
            if m:
                detail.append(m.group(1).strip().lower())
        print(
            "Reverse OpenSSL TLS {} mode={} failed rc={} verified={} stages={} reasons={} details={}".format(
                protocol, peer_mode, proc.returncode,
                ("verify return code: 0" in log.lower()
                 or "verification: ok" in log.lower()),
                phase_class, reason_flags, detail[-3:]
            ), file=sys.stderr
        )
        sys.exit(1)
PY
  then
    # The server might still be unwinding the failed handshake. Show only
    # bounded state/length diagnostics, never the output containing secrets.
    sleep 0.2
    grep -E '^CRYPTO_TLS_PRIVATE_(HANDSHAKE_DIAG|STATE|CKE_DIAG|CKE_RECV|CKE_PARSED|CKE_DERIVE) ' "$server_log" >&2 | tail -40 || true
    grep '^CRYPTO_TEST_REVERSE_PHASE ' "$server_log" >&2 || true
    grep '^Crypto test-only TLS server/exporter failed ' "$server_log" >&2 || true
    echo "Reverse OpenSSL TLS $proto peer did not qualify" >&2
    exit 1
  fi

  if ! wait "$server_pid"; then
    server_pid=""
    # Print ONLY the approved private state/type/length diagnostic
    # without accidentally publishing a TLS transcript or export material.
    grep -E '^CRYPTO_TLS_PRIVATE_(HANDSHAKE_DIAG|STATE|CKE_DIAG|CKE_RECV|CKE_PARSED|CKE_DERIVE) ' "$server_log" >&2 | tail -40 || true
    grep '^CRYPTO_TEST_REVERSE_PHASE ' "$server_log" >&2 || true
    grep '^Crypto test-only TLS server/exporter failed ' "$server_log" >&2 || true
    echo "Crypto TLS $proto server failed authenticated TLS/HTTP" >&2
    exit 1
  fi
  server_pid=""
  test -s "$secret"
  if ! grep -q 'HTTP/1.0 200' "$openssl_log"; then
    echo "OpenSSL TLS $proto did not receive Crypto authenticated HTTP" >&2
    exit 1
  fi
  # Do NOT write the TLS export secret to CI logs; later add a separate
  # independently verified peer exporter comparison after handshake passes.
  rm -f "$openssl_log" "$server_log" "$secret"
  echo "Crypto shared TLS server vs OpenSSL3 s_client TLS $proto ($peer_mode) handshake and HTTP PASS"
done
