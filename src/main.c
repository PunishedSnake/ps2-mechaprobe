#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <kernel.h>
#include <libcdvd.h>
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
    char body[1024];

    probe_collect_system_info(&info);
    snprintf(body, sizeof(body),
             "ROMVER: %s\n"
             "Model: %s (query=%d status=0x%08x)\n"
             "MechaCon MV raw: %s\n"
             "MechaCon status: 0x%08x\n"
             "MechaCon version: %s\n"
             "MagicGate region: %s\n"
             "System type: %s\n"
             "RTC: %s",
             info.romver, info.model, info.model_result, info.model_status,
             info.mv_raw_hex, info.mv_status, info.mechacon_version,
             info.mg_region, info.system_type, info.rtc);
    ui_message("Console / MechaCon", "Read-only hardware evidence",
               body, "X Return", UI_TONE_INFO);
    ui_wait_cross();
}

static void show_probe_result(int memory_card_port, const probe_result_t *result)
{
    char body[1400];
    const char *icv = result->icvps2_valid ? result->icvps2_hex : "not present";
    int tone = result->code == 0 ? UI_TONE_SUCCESS : UI_TONE_DANGER;

    snprintf(body, sizeof(body),
             "Result: %s\n"
             "Stage: %s\n"
             "Code: %d\n"
             "Memory card: mc%d slot 0\n"
             "Evidence: %s\n"
             "Input bytes: %u\n"
             "SHA-256: %s\n"
             "KELF flags: 0x%04x\n"
             "Header / BIT entries: %u / %u\n"
             "Uses ICVPS2: %s\n"
             "ICVPS2: %s\n"
             "Returned BIT blocks: %u\n"
             "Encrypted blocks sent: %u\n"
             "Evidence write result: %d",
             result->code == 0 ? "SUCCESS" : "FAILED",
             result->stage, result->code, memory_card_port,
             result->run_dir[0] != '\0' ? result->run_dir : "not created",
             result->input_size, result->sha256[0] != '\0' ? result->sha256 : "n/a",
             result->flags, result->header_size, result->bit_count,
             result->uses_icvps2 ? "yes" : "no", icv,
             result->returned_block_count, result->processed_encrypted_blocks,
             result->evidence_result);

    ui_message(result->code == 0 ? "Probe complete" : "Probe failed",
               result->code == 0 ? "Evidence bundle written to USB"
                                 : "Failure evidence was preserved when possible",
               body, "X Return", tone);
    ui_wait_cross();
}

static void run_probe(int memory_card_port)
{
    probe_result_t result;
    char body[512];

    snprintf(body, sizeof(body),
             "Input: %s\n"
             "MagicGate card: mc%d slot 0\n\n"
             "The transaction will process this KELF through SECRMAN.\n"
             "Do not remove the memory card or USB device during the probe.",
             PROBE_INPUT_PATH, memory_card_port);
    ui_message("Running KELF probe", "MechaCon / MagicGate transaction active",
               body, NULL, UI_TONE_WARNING);

    probe_run(memory_card_port, &result);
    show_probe_result(memory_card_port, &result);
}

static void show_experiment_notes(void)
{
    ui_message(
        "Experiment protocol", "v0.1 keeps the transaction scientifically boring",
        "1. Put one KELF at mass:/PS2DF-MECHA/input.kelf\n"
        "2. Probe it with the matching MagicGate card.\n"
        "3. Reboot the PS2 and repeat the same KELF several times.\n"
        "4. Then repeat with different KELFs that use ICVPS2.\n\n"
        "The normal probe issues SCMD 0x98 exactly once, inside the instrumented\n"
        "SecrDownloadFile-equivalent flow. A second raw 0x98 probe is deliberately\n"
        "not mixed into this transaction because it would measure a later state.",
        "X Return", UI_TONE_INFO);
    ui_wait_cross();
}

int main(void)
{
    static const ui_menu_item_t menu[] = {
        {"Probe input.kelf with mc0", "Use the MagicGate card in memory-card port 1", 1},
        {"Probe input.kelf with mc1", "Use the MagicGate card in memory-card port 2", 1},
        {"Console / MechaCon info", "ROMVER, model query, sceCdMV and RTC evidence", 1},
        {"Experiment protocol", "How to compare repeated and cross-KELF ICVPS2 results", 1},
        {"Return to PS2 Browser", "Leave Mecha Probe through ExecOSD", 1}
    };
    unsigned int selection = 0;
    int result;

    /* Keep the hardware-proven libdebug GS bootstrap before IOP reset. */
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
            "Input: mass:/PS2DF-MECHA/input.kelf",
            menu, sizeof(menu) / sizeof(menu[0]), &selection);

        if (choice < 0 || choice == 4) {
            SecrDeinit();
            platform_exit_browser();
        }
        if (choice == 0)
            run_probe(0);
        else if (choice == 1)
            run_probe(1);
        else if (choice == 2)
            show_system_info();
        else if (choice == 3)
            show_experiment_notes();
    }

    return 0;
}
