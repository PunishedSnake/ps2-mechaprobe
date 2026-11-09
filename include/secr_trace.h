#ifndef PS2_MECHAPROBE_SECR_TRACE_H
#define PS2_MECHAPROBE_SECR_TRACE_H

void secr_trace_reset(void);
int secr_trace_save(const char *run_dir);
const char *secr_trace_summary(void);

#endif
