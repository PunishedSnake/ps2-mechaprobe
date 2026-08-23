#ifndef PS2_MECHAPROBE_PROBE_H
#define PS2_MECHAPROBE_PROBE_H

#define PROBE_INPUT_PATH "mass:/PS2DF-MECHA/input.kelf"
#define PROBE_ROOT_PATH "mass:/PS2DF-MECHA"
#define PROBE_VERSION "0.1.0-dev"

typedef struct {
    char romver[32];
    char model[32];
    int model_result;
    unsigned int model_status;
    int mv_result;
    unsigned int mv_status;
    unsigned char mv_raw[4];
    char mv_raw_hex[9];
    char mg_region[16];
    char mechacon_version[16];
    char system_type[16];
    char rtc[32];
} probe_system_info_t;

typedef struct {
    int code;
    int memory_card_port;
    char stage[48];
    char run_dir[96];
    char sha256[65];
    unsigned int input_size;
    unsigned int header_size;
    unsigned int flags;
    unsigned int bit_count;
    int uses_icvps2;
    unsigned int returned_header_size;
    unsigned int returned_block_count;
    unsigned int processed_encrypted_blocks;
    unsigned char icvps2[8];
    char icvps2_hex[17];
    int icvps2_valid;
    int evidence_result;
    probe_system_info_t system;
} probe_result_t;

void probe_collect_system_info(probe_system_info_t *info);
int probe_wait_for_mass(unsigned int timeout_ms);
int probe_run(int memory_card_port, probe_result_t *result);

#endif
