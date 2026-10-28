/* Private TLS 1.3 encrypted-handshake framing. Included by upstream tls13.c. */
static int tls13_recv_handshake_message(TLS_CONNECT *conn, int from_server)
{
	uint8_t *seq = from_server ? conn->server_seq_num : conn->client_seq_num;
	const BLOCK_CIPHER_KEY *key = from_server ? &conn->server_write_key : &conn->client_write_key;
	const uint8_t *iv = from_server ? conn->server_write_iv : conn->client_write_iv;
	size_t wanted;

	/* An absent optional CertificateRequest leaves one complete message. */
	if (conn->handshake_message_pending) {
		conn->handshake_message_pending = 0;
		return 1;
	}
	for (;;) {
		wanted = TLS_HANDSHAKE_HEADER_SIZE;
		if (conn->handshake_message_len >= TLS_HANDSHAKE_HEADER_SIZE) {
			const uint8_t *p = conn->plain_record + 5;
			size_t body = ((size_t)p[1] << 16) | ((size_t)p[2] << 8) | p[3];
			if (body > TLS_MAX_HANDSHAKE_DATA_SIZE) {
				tls13_send_alert(conn, TLS_alert_decode_error);
				return -1;
			}
			wanted += body;
			if (conn->handshake_message_len == wanted) {
				/* RFC 8446 5.1: key-changing messages end at a record boundary. */
				if ((p[0] == TLS_handshake_finished || p[0] == TLS_handshake_key_update)
					&& conn->handshake_fragment_offset != conn->handshake_fragment_len) {
					tls13_send_alert(conn, TLS_alert_unexpected_message);
					return -1;
				}
				conn->plain_record[0] = TLS_record_handshake;
				conn->plain_record[1] = 3;
				conn->plain_record[2] = 3;
				conn->plain_record[3] = (uint8_t)(wanted >> 8);
				conn->plain_record[4] = (uint8_t)wanted;
				conn->plain_recordlen = 5 + wanted;
				conn->handshake_message_len = 0;
				return 1;
			}
		}
		if (conn->handshake_fragment_offset == conn->handshake_fragment_len) {
			int ret = tls_recv_record(conn);
			if (ret != 1) return ret;
			if (tls_record_type(conn->record) != TLS_record_application_data
				|| tls_record_protocol(conn->record) != TLS_protocol_tls12) {
				tls13_send_alert(conn, TLS_alert_unexpected_message);
				return -1;
			}
			if (tls13_record_decrypt(conn->cipher_suite, key, iv, seq,
				conn->record, conn->recordlen, conn->handshake_fragment,
				&conn->handshake_fragment_len) != 1) {
				tls13_send_alert(conn, TLS_alert_bad_record_mac);
				return -1;
			}
			tls_seq_num_incr(seq);
			tls_clean_record(conn);
			if (conn->handshake_fragment_len <= 5
				|| conn->handshake_fragment_len > 5 + TLS_MAX_PLAINTEXT_SIZE
				|| tls_record_type(conn->handshake_fragment) != TLS_record_handshake) {
				tls13_send_alert(conn, TLS_alert_unexpected_message);
				return -1;
			}
			conn->handshake_fragment_offset = 5;
		}
		{
			size_t count = conn->handshake_fragment_len - conn->handshake_fragment_offset;
			if (count > wanted - conn->handshake_message_len)
				count = wanted - conn->handshake_message_len;
			memcpy(conn->plain_record + 5 + conn->handshake_message_len,
				conn->handshake_fragment + conn->handshake_fragment_offset, count);
			conn->handshake_message_len += count;
			conn->handshake_fragment_offset += count;
		}
	}
}
