#include <cnet/cnet.h>
#include <cnet/destination_policy.h>
#include <salts/error_codes.h>

int main() {
  cnet_client client{};
  cnet_destination_hint hint{1u, 1u, 0u, true};
  if (cnet_destination_validate(&hint, 1u) != SALTS_OK) return 1;
  if (cnet_client_destroy(&client) != SALTS_OK) return 2;
  return 0;
}
