#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <kernel.h>
#include <libcdvd.h>
#include <libmc.h>
#include <libsecr.h>

#include <stdio.h>
#include <string.h>

#include "platform.h"
#include "probe.h"
#include "ui.h"

static void fatal_startup(const char *message, int code)
{
    char body[512];

    snprintf(body, sizeof(body),
             "%s\n\nStartup code: %d\n\nPower-cycle the console after noting the code.",
             message, code);
    ui_message("Startup failure", "Mecha Probe cannot continue",
               body, NULL, UI_TONE_DANGER);
    SleepThread();
}

static void show_system_info(void)
{
    probe_system_info_t info;
    char body[1200];

    probe_collect_system_info(&info);
    snprintf(body, sizeof(body),
             "ROMVER: %s\n"
             "Model: %s\n"
             "Model raw: %s\n"
             "Model query: %d status=0x%08x\n"
             "MechaCon MV raw: %s\n"
             "MechaCon status: 0x%08x\n"
             "MechaCon version: %s\n"
             "MagicGate region: %s\n"
             "System type: %s\n"
             "RTC: %s",
             info.romver, info.model, info.model_raw_hex,
             info.model_result, info.model_status,
             info.mv_raw_hex, info.mv_status, info.mechacon_version,
             info.mg_region, info.system_type, info.rtc);
    ui_message("Console / MechaCon", "Read-only hardware evidence",
               body, "X Return", UI_TONE_INFO);
    ui_wait_cross();
}

static void show_probe_result(int memory_card_port, const probe_result_t *result)
{
    char body[1800];
    const char *icv = result->icvps2_valid ? result->icvps2_hex : "not present";
    int tone = result->code == 0 ? UI_TONE_SUCCESS : UI_TONE_DANGER;

    snprintf(body, sizeof(body),
             "Result: %s\n"
             "Mode: %s\n"
             "Stage: %s\n"
             "Code: %d\n"
             "Memory card: mc%d slot 0\n"
             "Evidence: %s\n"
             "Input bytes: %u\n"
             "SHA-256: %s\n"
             "Flags original/effective: 0x%04x / 0x%04x\n"
             "Header / BIT entries: %u / %u\n"
             "KELF Uses ICVPS2: %s\n"
             "ICV read attempted: %s%s\n"
             "ICVPS2: %s\n"
             "Returned BIT blocks: %u\n"
             "Encrypted blocks sent: %u\n"
             "Evidence write result: %d",
             result->code == 0 ? "SUCCESS" : "FAILED",
             probe_mode_name(result->mode),
             result->stage, result->code, memory_card_port,
             result->run_dir[0] != '\0' ? result->run_dir : "not created",
             result->input_size, result->sha256[0] != '\0' ? result->sha256 : "n/a",
             result->original_flags, result->flags,
             result->header_size, result->bit_count,
             result->uses_icvps2 ? "yes" : "no",
             result->icvps2_read_attempted ? "yes" : "no",
             result->icvps2_read_unconditional ? " (unconditional)" : "",
             icv, result->returned_block_count,
             result->processed_encrypted_blocks,
             result->evidence_result);

    ui_message(result->code == 0 ? "Probe complete" : "Probe failed",
               result->code == 0 ? "Evidence bundle written to USB"
                                 : "Failure evidence was preserved when possible",
               body, "X Return", tone);
    ui_wait_cross();
}

static void run_probe(int memory_card_port, probe_mode_t mode)
{
    probe_result_t result;
    char body[760];
    const char *detail;

    if (mode == PROBE_MODE_NATIVE_CONTROL) {
        detail = "Native control: the KELF header is not modified. SCMD 0x98 is used only if the KELF itself requests ICVPS2.";
    } else if (mode == PROBE_MODE_NATIVE_ICV_READ) {
        detail = "Native header + one ICV read: the KELF header stays untouched, then one SCMD 0x98 is attempted after Kbit/Kc.";
    } else {
        detail = "Forced-flag replay: enables Uses_ICVPS2 in the private RAM copy. Kept only to reproduce RUN0001.";
    }

    snprintf(body, sizeof(body),
             "Input: %s\n"
             "MagicGate card: mc%d slot 0\n"
             "Mode: %s\n\n"
             "%s\n\n"
             "Do not remove the memory card or USB device during the probe.",
             PROBE_INPUT_PATH, memory_card_port, probe_mode_name(mode), detail);
    ui_message("Running KELF probe", "MechaCon / MagicGate transaction active",
               body, NULL, UI_TONE_WARNING);

    probe_run(memory_card_port, mode, &result);
    show_probe_result(memory_card_port, &result);
}

static void show_experiment_notes(void)
{
    ui_message(
        "Experiment protocol", "dev.5 uses the hardware-validated SECR port bridge",
        "RUN0004/RUN0005 proved mcInit alone does not fix GET_KBIT.\n"
        "MagicGate Inspector already established that SECRMAN CardAuth expects\n"
        "physical SIO2 channels 2/3 while the EE UI uses logical mc0/mc1.\n\n"
        "dev.5 translates only SECR Header/Kbit/Kc RPC ports: 0->2 and 1->3.\n"
        "The KELF, MechaCon sequence and ICV experiment are otherwise unchanged.\n\n"
        "Cold-boot between native control and native + one ICV read.",
        "X Return", UI_TONE_INFO);
    ui_wait_cross();
}

int main(void)
{
    static const ui_menu_item_t menu[] = {
        {"Native control - mc0", "Untouched KELF; SECR CardAuth uses physical SIO2 channel 2", 1},
        {"Native + one ICV read - mc0", "Untouched header; attempt one SCMD 0x98 after Kbit/Kc", 1},
        {"Native + one ICV read - mc1", "Same experiment; SECR CardAuth uses physical SIO2 channel 3", 1},
        {"Forced ICV flag replay - mc0", "Reproduce RUN0001: private RAM header 0x022c -> 0x022e", 1},
        {"Console / MechaCon info", "ROMVER, raw model response, sceCdMV and RTC evidence", 1},
        {"Experiment protocol", "RUN0004/5 finding and physical SECR port mapping", 1},
        {"Return to PS2 Browser", "Leave Mecha Probe through ExecOSD", 1}
    };
    unsigned int selection = 0;
    int result;

    ui_init();
    ui_message("Initializing", "Resetting IOP",
               "Building a known runtime instead of inheriting launcher modules.",
               NULL, UI_TONE_INFO);
    platform_reset_iop();

    ui_message("Initializing", "Loading embedded IOP modules",
               "SECRMAN, SECRSIF, memory-card services and USB mass storage.",
               NULL, UI_TONE_INFO);
    result = platform_load_modules();
    if (result < 0)
        fatal_startup("Could not load the required embedded IOP modules.", result);

    result = fileXioInit();
    if (result < 0)
        fatal_startup("fileXio RPC initialization failed.", result);

    if (!sceCdInit(SCECdINIT))
        fatal_startup("libcdvd could not initialize the CDVD/MechaCon RPC path.", -20);

    ui_message("Initializing", "Binding memory-card RPC",
               "Initializing the EE libmc client exactly as the FreeMcBoot signing path does.",
               NULL, UI_TONE_INFO);
    result = mcInit(MC_TYPE_XMC);
    if (result < 0)
        fatal_startup("libmc could not initialize MCMAN/MCSERV.", -23);

    ui_message("Initializing", "Binding SECR RPC",
               "Waiting for the seven SECRSIF services used by libsecr.",
               NULL, UI_TONE_INFO);
    if (!SecrInit())
        fatal_startup("libsecr initialization failed.", -21);

    result = platform_init_pad();
    if (result < 0)
        fatal_startup("Controller 1 is not available.", -22);

    for (;;) {
        int choice = ui_menu_select(
            "DriveForge Mecha Probe",
            "dev.5 | input: mass:/PS2DF-MECHA/input.kelf",
            menu, sizeof(menu) / sizeof(menu[0]), &selection);

        if (choice < 0 || choice == 6) {
            SecrDeinit();
            platform_exit_browser();
        }
        if (choice == 0)
            run_probe(0, PROBE_MODE_NATIVE_CONTROL);
        else if (choice == 1)
            run_probe(0, PROBE_MODE_NATIVE_ICV_READ);
        else if (choice == 2)
            run_probe(1, PROBE_MODE_NATIVE_ICV_READ);
        else if (choice == 3)
            run_probe(0, PROBE_MODE_FORCE_ICV_FLAG);
        else if (choice == 4)
            show_system_info();
        else if (choice == 5)
            show_experiment_notes();
    }

    return 0;
}
