#include <salts/asn1.h>
#include <salts/asn1_value.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    static const uint8_t expected[] = {0x30u,0x03u,0x02u,0x01u,0x2au};
    salts_asn1_value *root = NULL, *v = NULL, *decoded = NULL;
    uint8_t output[16] = {0};
    size_t written = 0u, count = 0u;
    int64_t n = 0;
    int rc = 1;
    if (salts_asn1_value_create_sequence(&root) != SALTS_ASN1_OK) goto done;
    if (salts_asn1_value_create_integer(42, &v) != SALTS_ASN1_OK) goto done;
    if (salts_asn1_value_add_child(root,v) != SALTS_ASN1_OK) goto done;
    v = NULL; /* ownership transferred only after OK */
    if (salts_asn1_value_encode(root,SALTS_ASN1_DER,0u,
        output,sizeof(output),&written) != SALTS_ASN1_OK) goto done;
    if (written != sizeof(expected) || memcmp(output,expected,written)) goto done;
    if (salts_asn1_value_decode(output,written,SALTS_ASN1_DER,&decoded) != SALTS_ASN1_OK) goto done;
    count = salts_asn1_value_child_count(decoded);
    if (count != 1u) goto done;
    if (salts_asn1_value_get_integer_i64(
        salts_asn1_value_child_at(decoded,0u),&n) != SALTS_ASN1_OK || n != 42) goto done;
    rc = 0;
done:
    salts_asn1_value_free(v);
    salts_asn1_value_free(root);
    salts_asn1_value_free(decoded);
    if (rc) fprintf(stderr,"C11 standalone installed ASN.1 consumer failed\n");
    return rc;
}
