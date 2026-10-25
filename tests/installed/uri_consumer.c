#include <uri_parser.h>
#include <string.h>
int main(void) {
  uri_t uri = {0};
  if (!uri_parse("tcp://127.0.0.1:7999",&uri) ||
      uri.port!=7999 || uri.host_type!=URI_HOST_IPV4ADDR ||
      strcmp(uri.host,"127.0.0.1")!=0) return 1;
  return 0;
}
