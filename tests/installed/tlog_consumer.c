#include <tlog.h>
#include <string.h>
static size_t hits;
static void cb(const cmeta_log_entry_t *e,void *ctx) {
 (void)ctx;
 if(e && e->message.len==2 && !memcmp(e->message.data,"ok",2))++hits;
}
int main(void) {
 tlog_t *l=tlog_create(NULL);
 if(!l)return 1;
 cmeta_log_sink_t *s=cmeta_sink_callback_create(cb,NULL);
 if(!s || tlog_add_sink(l,s)) {
  if(s)cmeta_sink_destroy(s);tlog_destroy(l);return 2;
 }
 cmeta_log_str(l,SALTS_LOG_LEVEL_INFO,vstr_from_buf(NULL,0),
   vstr_from_buf(NULL,0),0,vstr_from_cstr("ok"));
 tlog_flush(l);
 const int success=hits==1 && tlog_get_written(l)==1;
 tlog_destroy(l);
 return success?0:3;
}
