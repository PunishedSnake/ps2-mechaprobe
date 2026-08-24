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
    char body[2000];
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
             "Evidence write result: %d\n"
             "SECR trace: %s",
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
             result->evidence_result,
             secr_trace_summary());

    ui_message(result->code == 0 ? "Probe complete" : "Probe failed",
               result->code == 0 ? "Evidence + explicit-auth SECR trace written to USB"
                                 : "Failure evidence was preserved when possible",
               body, "X Return", tone);
    ui_wait_cross();
}

static void run_probe(int memory_card_port, probe_mode_t mode)
{
    probe_result_t result;
    char body[840];
    const char *detail;

    if (mode == PROBE_MODE_NATIVE_CONTROL) {
        detail = "dev.8 establishes one fresh SecrAuthCard session immediately before the native KELF transaction, then captures the auth transcript plus 0x94..0x98 in-band.";
    } else if (mode == PROBE_MODE_NATIVE_ICV_READ) {
        detail = "Legacy forced ICV comparison. Do not use this for the dev.8 session experiment.";
    } else {
        detail = "Forced-flag replay: historical RUN0001 reproduction only.";
    }

    snprintf(body, sizeof(body),
             "Input: %s\n"
             "MagicGate card: mc%d slot 0\n"
             "Mode: %s\n\n"
             "%s\n\n"
             "Do not remove the memory card or USB device during the probe.",
             PROBE_INPUT_PATH, memory_card_port, probe_mode_name(mode), detail);
    ui_message("Running KELF probe", "Explicit MagicGate auth + MechaCon transaction",
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
        "Experiment protocol", "dev.8 | explicit MagicGate session capture",
        "Candidate A remains the validated ICVPS2 KELF.\n\n"
        "Before SecrDownloadHeader, the instrumented SECRMAN performs exactly\n"
        "one SecrAuthCard and captures CardIV, CardMaterial, CardNonce, all\n"
        "three MechaChallenges and all three CardResponses. The following\n"
        "normal KELF transaction captures pre-Kbit/pre-Kc, final keys and ICV.\n\n"
        "No command is replayed for tracing. Cold-boot between runs.",
        "X Return", UI_TONE_INFO);
    ui_wait_cross();
}

int main(void)
{
    static const ui_menu_item_t menu[] = {
        {"Auth + native trace - mc0", "Fresh SecrAuthCard session, then validated Candidate-A KELF path", 1},
        {"Auth + native trace - mc1", "Same experiment through physical SIO2 channel 3", 1},
        {"Native + one ICV read - mc0", "Legacy forced-read comparison", 1},
        {"Native + one ICV read - mc1", "Legacy forced-read comparison on mc1", 1},
        {"Forced ICV flag replay - mc0", "Historical RUN0001 reproduction only", 1},
        {"Console / MechaCon info", "ROMVER, raw model response, sceCdMV and RTC evidence", 1},
        {"Experiment protocol", "dev.8 auth/session capture notes", 1},
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
            "dev.8 | explicit auth + 0x94..0x98 trace",
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
