/* Installed C11 consumer: original native mmap API comes exclusively from
 * the canonical installed Salts::Core shared library, not CMeta or Salts.
 */
#include <cmeta_mmap.h>
#include <stddef.h>
#include <string.h>

int main(void)
{
    cmeta_mmap_t mapping;
    cmeta_mmap_group_t group;
    if (cmeta_mmap_page_size() == 0u ||
        cmeta_mmap_page_count(cmeta_mmap_page_size() + 1u) != 2u)
        return 1;
    cmeta_mmap_init(&mapping);
    cmeta_mmap_group_init(&group);
    if (cmeta_mmap_pages(&mapping) != 0u)
        return 2;
    if (cmeta_mmap_open(NULL, "", SALTS_MMAP_READ) != SALTS_MMAP_EINVAL)
        return 3;
    if (cmeta_mmap_from_fd(NULL, 0, 0, 0u, SALTS_MMAP_READ)
        != SALTS_MMAP_EINVAL)
        return 4;
    if (cmeta_mmap_advise(&mapping, SALTS_MMAP_NORMAL)
        != SALTS_MMAP_EINVAL)
        return 5;
    if (cmeta_mmap_lock(&mapping) != SALTS_MMAP_EINVAL)
        return 6;
    if (cmeta_mmap_unlock(&mapping) != SALTS_MMAP_EINVAL)
        return 7;
    if (cmeta_mmap_sync(&mapping, false) != SALTS_MMAP_EINVAL)
        return 8;
    if (strcmp(cmeta_mmap_strerror(SALTS_MMAP_EIO), "I/O error") != 0)
        return 9;
    cmeta_mmap_close(&mapping);
    cmeta_mmap_group_close(&group);
    return 0;
}
