#include <salts/process.h>
int main(){ salts_process_options_t opts{}; salts_process_options_init(&opts); return opts.max_output_bytes == 0 ? 1 : 0; }
