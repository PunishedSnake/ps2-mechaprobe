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
#include "secr_trace.h"
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
             "ROMVER: %s\nModel: %s\nModel raw: %s\n"
             "Model query: %d status=0x%08x\nMechaCon MV raw: %s\n"
             "MechaCon status: 0x%08x\nMechaCon version: %s\n"
             "MagicGate region: %s\nSystem type: %s\nRTC: %s",
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
    char body[2100];
    const char *icv = result->icvps2_valid ? result->icvps2_hex : "not present";
    int tone = result->code == 0 ? UI_TONE_SUCCESS : UI_TONE_DANGER;

    snprintf(body, sizeof(body),
             "Result: %s\nMode: %s\nStage: %s\nCode: %d\n"
             "Memory card: mc%d slot 0\nEvidence: %s\nInput bytes: %u\n"
             "SHA-256: %s\nFlags original/effective: 0x%04x / 0x%04x\n"
             "Header / BIT entries: %u / %u\nKELF Uses ICVPS2: %s\n"
             "ICV read attempted: %s%s\nICVPS2: %s\nReturned BIT blocks: %u\n"
             "Encrypted blocks sent: %u\nEvidence write result: %d\nSECR trace: %s",
             result->code == 0 ? "SUCCESS" : "FAILED",
             probe_mode_name(result->mode), result->stage, result->code,
             memory_card_port,
             result->run_dir[0] != '\0' ? result->run_dir : "not created",
             result->input_size,
             result->sha256[0] != '\0' ? result->sha256 : "n/a",
             result->original_flags, result->flags,
             result->header_size, result->bit_count,
             result->uses_icvps2 ? "yes" : "no",
             result->icvps2_read_attempted ? "yes" : "no",
             result->icvps2_read_unconditional ? " (unconditional)" : "",
             icv, result->returned_block_count,
             result->processed_encrypted_blocks, result->evidence_result,
             secr_trace_summary());

    ui_message(result->code == 0 ? "Probe complete" : "Probe failed",
               result->code == 0 ? "Evidence + passive F2/auth trace written to USB"
                                 : "Failure evidence was preserved when possible",
               body, "X Return", tone);
    ui_wait_cross();
}

static void run_probe(int memory_card_port, probe_mode_t mode)
{
    probe_result_t result;
    char body[980];
    const char *detail;

    if (mode == PROBE_MODE_NATIVE_CONTROL) {
        detail = "dev.11 preserves dev.10 exactly: no mcGetInfo/F3, no explicit SecrAuthCard and no replay. It passively records F2/50-53 and reports the last natural successful SecrAuthCard only if one occurred after instrumented SECRMAN loaded.";
    } else if (mode == PROBE_MODE_NATIVE_ICV_READ) {
        detail = "Legacy forced ICV comparison. Do not use for the controlled dev.11 session experiment.";
    } else {
        detail = "Forced-flag replay: historical RUN0001 reproduction only.";
    }

    snprintf(body, sizeof(body),
             "Input: %s\nMagicGate card: mc%d slot 0\nMode: %s\n\n%s\n\n"
             "Do not remove the memory card or USB device during the probe.",
             PROBE_INPUT_PATH, memory_card_port, probe_mode_name(mode), detail);
    ui_message("Running KELF probe", "Passive MechaCon / MagicGate observation",
               body, NULL, UI_TONE_WARNING);

    secr_trace_reset();
    probe_run(memory_card_port, mode, &result);
    if (result.run_dir[0] != '\0')
        secr_trace_save(result.run_dir);
    show_probe_result(memory_card_port, &result);
}

static void show_experiment_notes(void)
{
    ui_message(
        "Experiment protocol", "dev.11 | passive natural-auth observation",
        "The native KELF/F2 transaction order is unchanged from hardware-successful dev.10.\n\n"
        "Instrumented SECRMAN remembers a naturally occurring successful full\n"
        "SecrAuthCard only if the normal stack performs one after module load.\n"
        "The probe never requests or resets that auth.\n\n"
        "auth-trace-status.txt explicitly says whether such a transcript was\n"
        "observed. A clean 'false' is a useful result, not a probe failure.\n\n"
        "Return to Browser still restores the ROM IOP environment first.",
        "X Return", UI_TONE_INFO);
    ui_wait_cross();
}

int main(void)
{
    static const ui_menu_item_t menu[] = {
        {"Native F2+auth trace - mc0", "Normal KELF path; passive F2 and natural-auth observation", 1},
        {"Native F2+auth trace - mc1", "Same experiment through memory-card port 1", 1},
        {"Native + one ICV read - mc0", "Legacy forced-read comparison", 1},
        {"Native + one ICV read - mc1", "Legacy forced-read comparison on mc1", 1},
        {"Forced ICV flag replay - mc0", "Historical RUN0001 reproduction only", 1},
        {"Console / MechaCon info", "ROMVER, raw model response, sceCdMV and RTC evidence", 1},
        {"Experiment protocol", "dev.11 passive-auth and safe-exit notes", 1},
        {"Return to PS2 Browser", "Restore ROM IOP state, then leave through ExecOSD", 1}
    };
    unsigned int selection = 0;
    int result;

    ui_init();
    ui_message("Initializing", "Resetting IOP",
               "Building a known runtime instead of inheriting launcher modules.",
               NULL, UI_TONE_INFO);
    platform_reset_iop();

    ui_message("Initializing", "Loading embedded IOP modules",
               "Instrumented SECRMAN, paired SECRSIF, memory-card services and USB.",
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
               "Initializing libmc without probing/resetting card auth state.",
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
            "dev.11 | passive F2 + natural-auth trace",
            menu, sizeof(menu) / sizeof(menu[0]), &selection);

        if (choice < 0 || choice == 7) {
            SecrDeinit();
            platform_exit_browser();
        }
        if (choice == 0)
            run_probe(0, PROBE_MODE_NATIVE_CONTROL);
        else if (choice == 1)
            run_probe(1, PROBE_MODE_NATIVE_CONTROL);
        else if (choice == 2)
            run_probe(0, PROBE_MODE_NATIVE_ICV_READ);
        else if (choice == 3)
            run_probe(1, PROBE_MODE_NATIVE_ICV_READ);
        else if (choice == 4)
            run_probe(0, PROBE_MODE_FORCE_ICV_FLAG);
        else if (choice == 5)
            show_system_info();
        else if (choice == 6)
            show_experiment_notes();
    }
    return 0;
}
