#include <tlog.h>
#include <tlog_helper.h>
#include <cstring>
#include <string>
#include <vector>
static int count=0;
static void capture(const cmeta_log_entry_t *e,void *) {
 if(e && e->message.len==3u && !std::memcmp(e->message.data,"cpp",3))++count;
}
int main(){
 tlog_t *l=tlog_create(nullptr);
 if(!l)return 1;
 auto *s=cmeta_sink_callback_create(capture,nullptr);
 if(!s || tlog_add_sink(l,s)!=0) {
  if(s)cmeta_sink_destroy(s);tlog_destroy(l);return 2;
 }
 cmeta_log_str(l,SALTS_LOG_LEVEL_INFO,vstr_from_cstr("cpp"),
   vstr_from_buf(nullptr,0),0,vstr_from_cstr("cpp"));
 tlog_flush(l);
 const bool ok=count==1 && tlog_get_written(l)==1;
 tlog_destroy(l);return ok?0:3;
}
