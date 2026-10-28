#include <salts/asn1.h>
#include <salts/asn1_value.h>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <type_traits>
static_assert(std::is_standard_layout<salts_asn1_reader>::value,"C11 ABI");
int main() {
    salts_asn1_value *root=nullptr,*child=nullptr,*parsed=nullptr;
    std::uint8_t encoded[32]{};
    std::size_t size=0;
    std::int64_t n=0;
    int rc=1;
    if (salts_asn1_value_create_sequence(&root)!=SALTS_ASN1_OK) goto done;
    if (salts_asn1_value_create_integer(-128,&child)!=SALTS_ASN1_OK) goto done;
    if (salts_asn1_value_add_child(root,child)!=SALTS_ASN1_OK) goto done;
    child=nullptr;
    if (salts_asn1_value_encode(root,SALTS_ASN1_DER,0,encoded,sizeof encoded,&size)!=SALTS_ASN1_OK) goto done;
    if (salts_asn1_validate(encoded,size,SALTS_ASN1_DER)!=SALTS_ASN1_OK) goto done;
    if (salts_asn1_value_decode(encoded,size,SALTS_ASN1_DER,&parsed)!=SALTS_ASN1_OK) goto done;
    if (salts_asn1_value_child_count(parsed)!=1) goto done;
    if (salts_asn1_value_get_integer_i64(salts_asn1_value_child_at(parsed,0),&n)!=SALTS_ASN1_OK) goto done;
    if (n != -128) goto done;
    rc=0;
done:
    salts_asn1_value_free(child);
    salts_asn1_value_free(root);
    salts_asn1_value_free(parsed);
    if (rc) std::fprintf(stderr,"C++17 standalone installed ASN.1 consumer failed\n");
    return rc;
}
