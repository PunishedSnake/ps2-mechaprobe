#include <tamtypes.h>
#include <kernel.h>
#include <delaythread.h>
#include <sifrpc.h>
#include <iopcontrol.h>
#include <loadfile.h>
#include <sbv_patches.h>
#include <libpad.h>

#include "platform.h"

static unsigned char pad_buffer[256] __attribute__((aligned(64)));
static u32 previous_pad_buttons;

extern unsigned char iomanX_irx[];
extern unsigned int size_iomanX_irx;
extern unsigned char fileXio_irx[];
extern unsigned int size_fileXio_irx;
extern unsigned char secrman_irx[];
extern unsigned int size_secrman_irx;
extern unsigned char freesio2_irx[];
extern unsigned int size_freesio2_irx;
extern unsigned char freepad_irx[];
extern unsigned int size_freepad_irx;
extern unsigned char mcman_irx[];
extern unsigned int size_mcman_irx;
extern unsigned char mcserv_irx[];
extern unsigned int size_mcserv_irx;
extern unsigned char secrsif_irx[];
extern unsigned int size_secrsif_irx;
extern unsigned char bdm_irx[];
extern unsigned int size_bdm_irx;
extern unsigned char bdmfs_fatfs_irx[];
extern unsigned int size_bdmfs_fatfs_irx;
extern unsigned char usbd_irx[];
extern unsigned int size_usbd_irx;
extern unsigned char usbmass_bd_irx[];
extern unsigned int size_usbmass_bd_irx;

void platform_reset_iop(void)
{
    sceSifInitRpc(0);
    while (!SifIopReset(NULL, 0)) {}
    while (!SifIopSync()) {}
    sceSifInitRpc(0);
    sbv_patch_enable_lmb();
    sbv_patch_disable_prefix_check();
}

static int exec_irx(void *buffer, unsigned int size)
{
    int module_result = 0;
    int module_id = SifExecModuleBuffer(buffer, size, 0, NULL, &module_result);

    if (module_id < 0)
        return module_id;
    return module_result < 0 ? module_result : 0;
}

int platform_load_modules(void)
{
    if (exec_irx(iomanX_irx, size_iomanX_irx) < 0) return -1;
    if (exec_irx(fileXio_irx, size_fileXio_irx) < 0) return -2;
    if (exec_irx(secrman_irx, size_secrman_irx) < 0) return -3;
    if (exec_irx(freesio2_irx, size_freesio2_irx) < 0) return -4;
    if (exec_irx(freepad_irx, size_freepad_irx) < 0) return -5;
    if (exec_irx(mcman_irx, size_mcman_irx) < 0) return -6;
    if (exec_irx(mcserv_irx, size_mcserv_irx) < 0) return -7;
    if (exec_irx(secrsif_irx, size_secrsif_irx) < 0) return -8;
    if (exec_irx(bdm_irx, size_bdm_irx) < 0) return -9;
    if (exec_irx(bdmfs_fatfs_irx, size_bdmfs_fatfs_irx) < 0) return -10;
    if (exec_irx(usbd_irx, size_usbd_irx) < 0) return -11;
    if (exec_irx(usbmass_bd_irx, size_usbmass_bd_irx) < 0) return -12;
    return 0;
}

static int wait_pad_ready(void)
{
    int timeout = 50000;

    while (timeout-- > 0) {
        int state = padGetState(0, 0);
        if (state == PAD_STATE_STABLE || state == PAD_STATE_FINDCTP1)
            return 0;
        DelayThread(100);
    }
    return -1;
}

int platform_init_pad(void)
{
    previous_pad_buttons = 0;
    padInit(0);
    if (!padPortOpen(0, 0, pad_buffer))
        return -1;
    return wait_pad_ready();
}

static int poll_pad_press(u32 *pressed)
{
    struct padButtonStatus buttons;
    int state;
    u32 current;

    if (pressed == NULL)
        return 0;
    *pressed = 0;
    state = padGetState(0, 0);
    if (state != PAD_STATE_STABLE && state != PAD_STATE_FINDCTP1)
        return 0;
    if (padRead(0, 0, &buttons) == 0)
        return 0;

    current = 0xffffu ^ buttons.btns;
    *pressed = current & ~previous_pad_buttons;
    previous_pad_buttons = current;
    return *pressed != 0;
}

u32 platform_wait_press(void)
{
    u32 pressed;

    for (;;) {
        if (poll_pad_press(&pressed))
            return pressed;
        DelayThread(16000);
    }
}

void platform_exit_browser(void)
{
    static char *browser_args[] = {"BootBrowser", NULL};

    padPortClose(0, 0);
    ExecOSD(1, browser_args);
    SleepThread();
}
