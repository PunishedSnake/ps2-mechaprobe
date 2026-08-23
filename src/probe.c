#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <io_common.h>
#include <libcdvd.h>
#include <libsecr.h>
#include <delaythread.h>
#include <tamtypes.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "probe.h"
#include "sha256.h"

#define PROBE_MAX_KELF_SIZE (16u * 1024u * 1024u)
#define PROBE_RPC_READ_PADDING 0x1000u
#define PROBE_PATH_SIZE 192u

static const char *region_names[8] = {
    "Japan", "USA", "Europe", "Oceania",
    "Asia", "Russia", "China", "Mexico"
};

static int write_whole_file(const char *path, const void *data, unsigned int size)
{
    const unsigned char *source = (const unsigned char *)data;
    int fd = fileXioOpen(path, FIO_O_WRONLY | FIO_O_CREAT | FIO_O_TRUNC, 0666);
    unsigned int total = 0;

    if (fd < 0)
        return fd;
    while (total < size) {
        int written = fileXioWrite(fd, source + total, size - total);
        if (written <= 0) {
            fileXioClose(fd);
            return written < 0 ? written : -1;
        }
        total += (unsigned int)written;
    }
    fileXioClose(fd);
    return 0;
}

static int read_bounded_file(const char *path, unsigned char **data_out,
                             unsigned int *size_out)
{
    int fd = fileXioOpen(path, FIO_O_RDONLY, 0);
    int size;
    int total = 0;
    unsigned char *data;

    if (fd < 0)
        return fd;
    size = fileXioLseek(fd, 0, FIO_SEEK_END);
    if (size <= 0 || (unsigned int)size > PROBE_MAX_KELF_SIZE ||
        fileXioLseek(fd, 0, FIO_SEEK_SET) < 0) {
        fileXioClose(fd);
        return -101;
    }

    data = malloc((unsigned int)size + PROBE_RPC_READ_PADDING);
    if (data == NULL) {
        fileXioClose(fd);
        return -102;
    }
    memset(data + size, 0, PROBE_RPC_READ_PADDING);

    while (total < size) {
        int received = fileXioRead(fd, data + total, size - total);
        if (received <= 0) {
            free(data);
            fileXioClose(fd);
            return received < 0 ? received : -103;
        }
        total += received;
    }
    fileXioClose(fd);
    *data_out = data;
    *size_out = (unsigned int)size;
    return 0;
}

static int path_exists(const char *path)
{
    iox_stat_t status;

    memset(&status, 0, sizeof(status));
    return fileXioGetStat(path, &status) >= 0;
}

int probe_wait_for_mass(unsigned int timeout_ms)
{
    unsigned int elapsed = 0;

    while (elapsed <= timeout_ms) {
        if (path_exists("mass:/"))
            return 0;
        DelayThread(100000);
        elapsed += 100u;
    }
    return -1;
}

static int ensure_output_root(void)
{
    if (path_exists(PROBE_ROOT_PATH))
        return 0;
    return fileXioMkdir(PROBE_ROOT_PATH, 0777);
}

static int create_run_directory(char destination[96])
{
    unsigned int i;
    int result;

    result = ensure_output_root();
    if (result < 0 && !path_exists(PROBE_ROOT_PATH))
        return result;

    for (i = 1; i <= 9999; i++) {
        snprintf(destination, 96, PROBE_ROOT_PATH "/RUN%04u", i);
        if (!path_exists(destination)) {
            result = fileXioMkdir(destination, 0777);
            if (result >= 0)
                return 0;
        }
    }
    return -104;
}

static void output_path(char destination[PROBE_PATH_SIZE], const char *directory,
                        const char *filename)
{
    snprintf(destination, PROBE_PATH_SIZE, "%s/%s", directory, filename);
}

static int read_romver(char destination[32])
{
    int fd = fileXioOpen("rom0:ROMVER", FIO_O_RDONLY, 0);
    int received;

    memset(destination, 0, 32);
    if (fd < 0)
        return fd;
    received = fileXioRead(fd, destination, 31);
    fileXioClose(fd);
    if (received <= 0)
        return received < 0 ? received : -1;
    destination[received < 31 ? received : 31] = '\0';
    return received;
}

static int bcd_to_int(unsigned char value)
{
    return ((value >> 4) * 10) + (value & 0x0f);
}

static void bytes_to_hex(const unsigned char *data, unsigned int size,
                         char *output)
{
    static const char digits[] = "0123456789abcdef";
    unsigned int i;

    for (i = 0; i < size; i++) {
        output[i * 2] = digits[data[i] >> 4];
        output[i * 2 + 1] = digits[data[i] & 0x0f];
    }
    output[size * 2] = '\0';
}

void probe_collect_system_info(probe_system_info_t *info)
{
    sceCdCLOCK clock;
    unsigned int region_code;
    int looks_like_rr_mm_tt;

    memset(info, 0, sizeof(*info));
    if (read_romver(info->romver) < 0)
        snprintf(info->romver, sizeof(info->romver), "unknown");

    snprintf(info->model, sizeof(info->model), "unknown");
    info->model_status = 0;
    info->model_result = sceCdRM(info->model, &info->model_status);
    if (!info->model_result)
        snprintf(info->model, sizeof(info->model), "unknown");

    info->mv_status = 0;
    memset(info->mv_raw, 0xff, sizeof(info->mv_raw));
    info->mv_result = sceCdMV(info->mv_raw, &info->mv_status);
    if (info->mv_result) {
        bytes_to_hex(info->mv_raw, 4, info->mv_raw_hex);
        looks_like_rr_mm_tt = info->mv_raw[0] <= 7u && info->mv_raw[3] <= 1u;
        if (looks_like_rr_mm_tt) {
            region_code = info->mv_raw[0];
            snprintf(info->mechacon_version, sizeof(info->mechacon_version),
                     "%02X.%02X", info->mv_raw[1], info->mv_raw[2]);
            snprintf(info->system_type, sizeof(info->system_type), "%s",
                     info->mv_raw[3] == 1u ? "PSX" : "PS2");
        } else {
            region_code = info->mv_status & 0x7fu;
            snprintf(info->mechacon_version, sizeof(info->mechacon_version),
                     "%02X.%02X", info->mv_raw[0], info->mv_raw[1]);
            snprintf(info->system_type, sizeof(info->system_type), "%s",
                     info->mv_raw[2] == 1u ? "PSX" : "PS2");
        }
        snprintf(info->mg_region, sizeof(info->mg_region), "%s",
                 region_code < 8u ? region_names[region_code] : "unknown");
    } else {
        snprintf(info->mv_raw_hex, sizeof(info->mv_raw_hex), "unknown");
        snprintf(info->mg_region, sizeof(info->mg_region), "unknown");
        snprintf(info->mechacon_version, sizeof(info->mechacon_version), "unknown");
        snprintf(info->system_type, sizeof(info->system_type), "unknown");
    }

    memset(&clock, 0, sizeof(clock));
    if (sceCdReadClock(&clock)) {
        snprintf(info->rtc, sizeof(info->rtc), "20%02d-%02d-%02dT%02d:%02d:%02d",
                 bcd_to_int(clock.year), bcd_to_int(clock.month & 0x7f),
                 bcd_to_int(clock.day), bcd_to_int(clock.hour),
                 bcd_to_int(clock.minute), bcd_to_int(clock.second));
    } else {
        snprintf(info->rtc, sizeof(info->rtc), "unknown");
    }
}

static int kelf_preflight(unsigned char *buffer, unsigned int size,
                          probe_result_t *result, unsigned int *key_offset)
{
    SecrKELFHeader_t *header;
    unsigned int offset;

    if (size < sizeof(SecrKELFHeader_t))
        return -110;
    header = (SecrKELFHeader_t *)buffer;
    result->header_size = header->KELF_header_size;
    result->flags = header->flags;
    result->bit_count = header->BIT_count;
    result->uses_icvps2 = (header->flags >> 1) & 1;

    if (result->header_size < sizeof(SecrKELFHeader_t) ||
        result->header_size > size || result->bit_count > 63u)
        return -111;

    offset = sizeof(SecrKELFHeader_t) +
             result->bit_count * sizeof(SecrBitBlockData_t);
    if (offset > result->header_size)
        return -112;
    if ((result->flags & 1u) != 0u) {
        unsigned int extension_size;
        if (offset >= result->header_size)
            return -113;
        extension_size = (unsigned int)buffer[offset] + 1u;
        if (extension_size > result->header_size - offset)
            return -114;
        offset += extension_size;
    }
    if ((result->flags & 0xf000u) == 0u) {
        if (result->header_size - offset < 8u)
            return -115;
        offset += 8u;
    }
    if (offset > result->header_size || result->header_size - offset < 32u)
        return -116;
    if (result->uses_icvps2 && result->header_size - offset < 40u)
        return -117;

    *key_offset = offset;
    return 0;
}

static int save_initial_evidence(const probe_result_t *result,
                                 const unsigned char *buffer,
                                 unsigned int size)
{
    char path[PROBE_PATH_SIZE];
    char hash_line[67];
    int first_error = 0;
    int current;

    output_path(path, result->run_dir, "input.kelf");
    current = write_whole_file(path, buffer, size);
    if (current < 0 && first_error == 0)
        first_error = current;

    snprintf(hash_line, sizeof(hash_line), "%s\n", result->sha256);
    output_path(path, result->run_dir, "input.sha256");
    current = write_whole_file(path, hash_line, (unsigned int)strlen(hash_line));
    if (current < 0 && first_error == 0)
        first_error = current;
    return first_error;
}

static int save_final_evidence(const probe_result_t *result,
                               const unsigned char *buffer,
                               const SecrBitTable_t *bit_table,
                               unsigned int bit_table_size)
{
    char path[PROBE_PATH_SIZE];
    char json[4096];
    char icv_line[19];
    char icv_json[24];
    const char *status = result->code == 0 ? "success" : "error";
    int first_error = result->evidence_result;
    int current;
    int written;

    if (result->header_size > 0 && result->header_size <= result->input_size) {
        output_path(path, result->run_dir, "processed-header.bin");
        current = write_whole_file(path, buffer, result->header_size);
        if (current < 0 && first_error == 0)
            first_error = current;
    }
    if (bit_table != NULL && bit_table_size > 0) {
        output_path(path, result->run_dir, "bit-table.bin");
        current = write_whole_file(path, bit_table, bit_table_size);
        if (current < 0 && first_error == 0)
            first_error = current;
    }
    if (result->icvps2_valid) {
        output_path(path, result->run_dir, "icvps2.bin");
        current = write_whole_file(path, result->icvps2, sizeof(result->icvps2));
        if (current < 0 && first_error == 0)
            first_error = current;
        snprintf(icv_line, sizeof(icv_line), "%s\n", result->icvps2_hex);
        output_path(path, result->run_dir, "icvps2.txt");
        current = write_whole_file(path, icv_line, (unsigned int)strlen(icv_line));
        if (current < 0 && first_error == 0)
            first_error = current;
        snprintf(icv_json, sizeof(icv_json), "\"%s\"", result->icvps2_hex);
    } else {
        snprintf(icv_json, sizeof(icv_json), "null");
    }

    written = snprintf(
        json, sizeof(json),
        "{\n"
        "  \"schema\": \"ps2-mechaprobe/v1\",\n"
        "  \"probe_version\": \"%s\",\n"
        "  \"status\": \"%s\",\n"
        "  \"stage\": \"%s\",\n"
        "  \"code\": %d,\n"
        "  \"input\": {\"path\": \"%s\", \"size\": %u, \"sha256\": \"%s\"},\n"
        "  \"memory_card\": {\"port\": %d, \"slot\": 0},\n"
        "  \"console\": {\"romver\": \"%s\", \"model\": \"%s\", \"model_query_result\": %d, \"model_status\": \"0x%08x\"},\n"
        "  \"mechacon\": {\"mv_result\": %d, \"mv_status\": \"0x%08x\", \"raw\": \"%s\", \"region\": \"%s\", \"version\": \"%s\", \"system_type\": \"%s\"},\n"
        "  \"rtc\": \"%s\",\n"
        "  \"kelf\": {\"flags\": \"0x%04x\", \"header_size\": %u, \"bit_count\": %u, \"uses_icvps2\": %s},\n"
        "  \"transaction\": {\"returned_header_size\": %u, \"returned_block_count\": %u, \"processed_encrypted_blocks\": %u},\n"
        "  \"icvps2\": %s,\n"
        "  \"evidence_write_result\": %d\n"
        "}\n",
        PROBE_VERSION, status, result->stage, result->code,
        PROBE_INPUT_PATH, result->input_size, result->sha256,
        result->memory_card_port,
        result->system.romver, result->system.model,
        result->system.model_result, result->system.model_status,
        result->system.mv_result, result->system.mv_status,
        result->system.mv_raw_hex, result->system.mg_region,
        result->system.mechacon_version, result->system.system_type,
        result->system.rtc, result->flags, result->header_size,
        result->bit_count, result->uses_icvps2 ? "true" : "false",
        result->returned_header_size, result->returned_block_count,
        result->processed_encrypted_blocks, icv_json, first_error);

    if (written < 0 || (unsigned int)written >= sizeof(json))
        return first_error != 0 ? first_error : -130;

    output_path(path, result->run_dir, "probe.json");
    current = write_whole_file(path, json, (unsigned int)written);
    if (current < 0 && first_error == 0)
        first_error = current;
    return first_error;
}

static int perform_download_transaction(int memory_card_port,
                                        unsigned char *buffer,
                                        unsigned int size,
                                        unsigned int key_offset,
                                        probe_result_t *result,
                                        SecrBitTable_t *bit_table,
                                        unsigned int *bit_table_size)
{
    s32 returned_size = 0;
    unsigned int offset;
    unsigned int i;
    unsigned char kbit[16];
    unsigned char kc[16];

    memset(bit_table, 0, sizeof(*bit_table));
    if (!SecrDownloadHeader(memory_card_port, 0, buffer, bit_table, &returned_size)) {
        snprintf(result->stage, sizeof(result->stage), "SecrDownloadHeader");
        return -120;
    }

    result->returned_header_size = bit_table->header.headersize;
    result->returned_block_count = bit_table->header.block_count;
    if (returned_size > 0 && (unsigned int)returned_size <= sizeof(*bit_table))
        *bit_table_size = (unsigned int)returned_size;
    else
        *bit_table_size = sizeof(SecrBitTableHeader_t) +
                          result->returned_block_count * sizeof(SecrBitBlockData_t);

    if (result->returned_block_count > 63u ||
        result->returned_header_size > size) {
        snprintf(result->stage, sizeof(result->stage), "validate returned BIT");
        return -121;
    }

    offset = result->returned_header_size;
    for (i = 0; i < result->returned_block_count; i++) {
        unsigned int block_size = bit_table->blocks[i].size;
        if (offset > size || block_size > size - offset) {
            snprintf(result->stage, sizeof(result->stage), "validate block bounds");
            return -122;
        }
        if ((bit_table->blocks[i].flags & 2u) != 0u) {
            if (!SecrDownloadBlock(buffer + offset, block_size)) {
                snprintf(result->stage, sizeof(result->stage), "SecrDownloadBlock");
                return -123;
            }
            result->processed_encrypted_blocks++;
        }
        offset += block_size;
    }

    if (!SecrDownloadGetKbit(memory_card_port, 0, kbit)) {
        snprintf(result->stage, sizeof(result->stage), "SecrDownloadGetKbit");
        return -124;
    }
    if (!SecrDownloadGetKc(memory_card_port, 0, kc)) {
        snprintf(result->stage, sizeof(result->stage), "SecrDownloadGetKc");
        return -125;
    }
    memcpy(buffer + key_offset, kbit, sizeof(kbit));
    memcpy(buffer + key_offset + sizeof(kbit), kc, sizeof(kc));

    if (result->uses_icvps2) {
        if (!SecrDownloadGetICVPS2(result->icvps2)) {
            snprintf(result->stage, sizeof(result->stage), "SecrDownloadGetICVPS2");
            return -126;
        }
        result->icvps2_valid = 1;
        bytes_to_hex(result->icvps2, sizeof(result->icvps2), result->icvps2_hex);
        memcpy(buffer + result->header_size - sizeof(result->icvps2),
               result->icvps2, sizeof(result->icvps2));
    }

    snprintf(result->stage, sizeof(result->stage), "complete");
    return 0;
}

int probe_run(int memory_card_port, probe_result_t *result)
{
    unsigned char *buffer = NULL;
    unsigned char digest[32];
    unsigned int size = 0;
    unsigned int key_offset = 0;
    unsigned int bit_table_size = 0;
    SecrBitTable_t bit_table;
    int code;

    if (result == NULL || (memory_card_port != 0 && memory_card_port != 1))
        return -1;
    memset(result, 0, sizeof(*result));
    result->memory_card_port = memory_card_port;
    snprintf(result->stage, sizeof(result->stage), "initializing");
    probe_collect_system_info(&result->system);

    if (probe_wait_for_mass(10000) < 0) {
        result->code = -100;
        snprintf(result->stage, sizeof(result->stage), "wait for USB mass storage");
        return result->code;
    }

    code = read_bounded_file(PROBE_INPUT_PATH, &buffer, &size);
    if (code < 0) {
        result->code = code;
        snprintf(result->stage, sizeof(result->stage), "read input.kelf");
        return result->code;
    }
    result->input_size = size;
    sha256_buffer(buffer, size, digest);
    sha256_hex(digest, result->sha256);

    code = create_run_directory(result->run_dir);
    if (code < 0) {
        free(buffer);
        result->code = code;
        snprintf(result->stage, sizeof(result->stage), "create evidence directory");
        return result->code;
    }
    result->evidence_result = save_initial_evidence(result, buffer, size);

    code = kelf_preflight(buffer, size, result, &key_offset);
    if (code < 0) {
        result->code = code;
        snprintf(result->stage, sizeof(result->stage), "KELF preflight");
        result->evidence_result = save_final_evidence(result, buffer, NULL, 0);
        free(buffer);
        return result->code;
    }

    memset(&bit_table, 0, sizeof(bit_table));
    code = perform_download_transaction(memory_card_port, buffer, size,
                                        key_offset, result, &bit_table,
                                        &bit_table_size);
    result->code = code;
    result->evidence_result = save_final_evidence(
        result, buffer, bit_table_size != 0 ? &bit_table : NULL, bit_table_size);
    free(buffer);
    return result->code;
}
