#ifndef PS2_MECHAPROBE_PLATFORM_H
#define PS2_MECHAPROBE_PLATFORM_H

#include <tamtypes.h>

void platform_reset_iop(void);
int platform_load_modules(void);
int platform_init_pad(void);
u32 platform_wait_press(void);
void platform_exit_browser(void);

#endif
