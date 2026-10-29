/* SPDX-License-Identifier: Apache-2.0
 * Original salts/utils mmap regression: real Salts::FS C11 owner.
 * Source Git blobs: cmeta_mmap.h ba7aecd588492814ebca196d66023dfc9f24801c,
 * cmeta_mmap.c 04e87de7bc565d77261e2de66c84380b85a0ac15.
 */
#include <cmeta_mmap.h>
#include <salts/fs.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#endif

#define CHECK(expr) do { if (!(expr)) { \
  fprintf(stderr, "Original mmap FAIL %s:%d: %s\n", __FILE__,__LINE__,#expr); \
  return 1; \
} } while(0)

int main(void) {
  static const char content[] = "C11 native mmap persisted bytes";
  char tmp[1024]={0},path[1400]={0},filename[120]={0};
  salts_fs_buf_t input=salts_fs_buf_init((char *)content,sizeof(content)-1u);
  salts_fs_buf_t output={0};
  cmeta_mmap_t mapping;
  cmeta_mmap_group_t group;
  size_t page=cmeta_mmap_page_size();
#ifdef _WIN32
  unsigned long pid=GetCurrentProcessId();
#else
  unsigned long pid=(unsigned long)getpid();
#endif
  CHECK(page>0 && cmeta_mmap_page_count(1u)==1u &&
        cmeta_mmap_page_count(page)==1u);
  CHECK(salts_fs_get_tmpdir(tmp,sizeof(tmp))==0);
  CHECK(snprintf(filename,sizeof(filename),"saltscore-original-mmap-%lu",pid)>0);
  CHECK(salts_fs_path_join(path,sizeof(path),tmp,filename)==0);
  CHECK(salts_fs_write_file(path,&input)==0);

  cmeta_mmap_init(&mapping);
  CHECK(!cmeta_mmap_is_open(&mapping));
  CHECK(cmeta_mmap_sync(&mapping,false)==SALTS_MMAP_EINVAL);
  CHECK(cmeta_mmap_sync_range(&mapping,0u,1u,false)==SALTS_MMAP_EINVAL);
  CHECK(cmeta_mmap_advise(&mapping,SALTS_MMAP_NORMAL)==SALTS_MMAP_EINVAL);
  CHECK(cmeta_mmap_lock(&mapping)==SALTS_MMAP_EINVAL);
  CHECK(cmeta_mmap_unlock(&mapping)==SALTS_MMAP_EINVAL);
  CHECK(cmeta_mmap_from_fd(NULL,0,0,0,SALTS_MMAP_READ)==SALTS_MMAP_EINVAL);
  CHECK(cmeta_mmap_open(&mapping,path,SALTS_MMAP_READ)==SALTS_MMAP_OK);
  CHECK(cmeta_mmap_is_open(&mapping));
  CHECK(cmeta_mmap_data(&mapping)!=NULL);
  CHECK(cmeta_mmap_size(&mapping)==sizeof(content)-1u);
  CHECK(memcmp(cmeta_mmap_data(&mapping),content,sizeof(content)-1u)==0);
  CHECK(cmeta_mmap_pages(&mapping)>=1u);
  cmeta_mmap_close(&mapping);
  CHECK(!cmeta_mmap_is_open(&mapping));
  cmeta_mmap_close(&mapping);

  /* Original from_fd must preserve borrowed native descriptor/handle. */
#ifdef _WIN32
  {
    HANDLE borrowed=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,NULL,
                                OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    CHECK(borrowed!=INVALID_HANDLE_VALUE);
    cmeta_mmap_init(&mapping);
    CHECK(cmeta_mmap_from_fd(&mapping,(intptr_t)borrowed,0,0,SALTS_MMAP_READ)
          ==SALTS_MMAP_OK);
    CHECK(memcmp(cmeta_mmap_data(&mapping),content,sizeof(content)-1u)==0);
    cmeta_mmap_close(&mapping);
    CHECK(GetFileType(borrowed)==FILE_TYPE_DISK);
    CloseHandle(borrowed);
  }
#else
  {
    int fd=open(path,O_RDONLY);
    CHECK(fd>=0);
    cmeta_mmap_init(&mapping);
    CHECK(cmeta_mmap_from_fd(&mapping,(intptr_t)fd,0,0,SALTS_MMAP_READ)
          ==SALTS_MMAP_OK);
    CHECK(memcmp(cmeta_mmap_data(&mapping),content,sizeof(content)-1u)==0);
    cmeta_mmap_close(&mapping);
    CHECK(fcntl(fd,F_GETFD)!=-1);
    CHECK(close(fd)==0);
  }
#endif

  CHECK(cmeta_mmap_open_range(&mapping,path,3,7,SALTS_MMAP_READ)==SALTS_MMAP_OK);
  CHECK(cmeta_mmap_size(&mapping)==7u);
  CHECK(memcmp(cmeta_mmap_data(&mapping),content+3,7u)==0);
  cmeta_mmap_close(&mapping);

  CHECK(cmeta_mmap_open(&mapping,path,SALTS_MMAP_WRITE)==SALTS_MMAP_OK);
  CHECK(cmeta_mmap_size(&mapping)==sizeof(content)-1u);
  CHECK(cmeta_mmap_sync_range(&mapping,0u,1u,false)==SALTS_MMAP_OK);
  CHECK(cmeta_mmap_get(&mapping,0u)==(uint8_t)content[0]);
  cmeta_mmap_set(&mapping,0u,(uint8_t)'X');
  CHECK(cmeta_mmap_sync(&mapping,false)==SALTS_MMAP_OK);
  cmeta_mmap_close(&mapping);
  CHECK(salts_fs_read_file(path,&output)==0);
  CHECK(output.len==sizeof(content)-1u);
  CHECK(output.base[0]=='X');
  CHECK(memcmp(output.base+1,content+1,sizeof(content)-2u)==0);
  salts_fs_buf_free(&output);

  cmeta_mmap_group_init(&group);
  CHECK(cmeta_mmap_group_open(&group,NULL,1,SALTS_MMAP_READ)
        ==SALTS_MMAP_EINVAL);
  cmeta_mmap_group_close(&group);
  CHECK(cmeta_mmap_open(&mapping,"__no_such_saltscore_mmap_file__",
                         SALTS_MMAP_READ)==SALTS_MMAP_ENOENT);
  CHECK(!cmeta_mmap_is_open(&mapping));
  CHECK(cmeta_mmap_strerror(SALTS_MMAP_EINVAL)!=NULL);
  CHECK(salts_fs_unlink(path)==0);
  puts("PASS: original mmap C11 real map/range/write/sync/close/error owner");
  return 0;
}
