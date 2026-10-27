#include <salts/fs.h>
int main() { salts_fs_stat_t info{}; (void)info; return salts_fs_path_is_absolute("") ? 1 : 0; }
