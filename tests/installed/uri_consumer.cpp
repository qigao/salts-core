#include <uri_parser.h>
#include <cstring>
int main() {
  uri_t uri{};
  if (!uri_parse("https://[::1]:443/hello",&uri) ||
      uri.port!=443 || uri.host_type!=URI_HOST_IPV6ADDR ||
      std::strcmp(uri.host,"::1")!=0) return 1;
  return 0;
}
