#include <tlog.h>
#include <string.h>
static size_t count=0;
static void capture(const cmeta_log_entry_t *e,void *ctx) {
 (void)ctx;
 if(e && e->message.len==4 && !memcmp(e->message.data,"test",4))++count;
}
int main(void) {
 tlog_t *l=tlog_create(NULL);
 if(!l) return 1;
 cmeta_log_sink_t *sink=cmeta_sink_callback_create(capture,NULL);
 if(!sink) {tlog_destroy(l);return 2;}
 if(tlog_add_sink(l,sink)!=0) {cmeta_sink_destroy(sink);tlog_destroy(l);return 3;}
 tlog_set_level(l,SALTS_LOG_LEVEL_DEBUG);
 cmeta_log_str(l,SALTS_LOG_LEVEL_INFO,vstr_from_cstr("core"),
 vstr_from_buf(NULL,0),0,vstr_from_cstr("test"));
 tlog_flush(l);
 const int success=count==1 && tlog_get_written(l)==1 && tlog_get_queue_size(l)==0;
 tlog_destroy(l);
 return success?0:4;
}
