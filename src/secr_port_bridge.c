/* SPDX-License-Identifier: MIT */
/*
 * Translate libmc-style logical memory-card ports (0/1) to the physical SIO2
 * memory-card channels (2/3) expected by SECRMAN CardAuth.
 *
 * dev.10 restores the hardware-successful dev.7 transaction semantics and adds
 * passive observability only around the stock F2/50-53 card_encrypt operations.
 * No mcGetInfo/F3 reset, SecrAuthCard replay, MechaCon replay or extra CardAuth
 * command is issued by this EE shim.
 */

#define NEWLIB_PORT_AWARE
#include <sifrpc.h>
#include <libsecr-common.h>
#include <secrsif.h>
#include <fileXio_rpc.h>
#include <io_common.h>

#include <stdio.h>
#include <string.h>

#include "secr_trace.h"

#define SECR_PREKEY_RPC_OFFSET 0x100u
#define SECR_F2_TRACE_RPC_OFFSET 0x120u
#define SECR_TRACE_PATH_SIZE 192u
#define F2_TRACE_SIZE 56u
#define F2_RECORD_SIZE 24u

static SifRpcClientData_t *HeaderClient;
static SifRpcClientData_t *KbitClient;
static SifRpcClientData_t *KcClient;
static SifRpcClientData_t *IcvClient;

static unsigned char PreKbit[16];
static unsigned char PreKc[16];
static unsigned char FinalKbit[16];
static unsigned char FinalKc[16];
static unsigned char Icvps2[8];
static unsigned char F2KbitTrace[F2_TRACE_SIZE];
static unsigned char F2KcTrace[F2_TRACE_SIZE];
static int HavePreKbit;
static int HavePreKc;
static int HaveFinalKbit;
static int HaveFinalKc;
static int HaveIcvps2;
static int HaveF2KbitTrace;
static int HaveF2KcTrace;
static char Summary[448];

int __real_sceSifBindRpc(SifRpcClientData_t *cd, int sid, int mode);
int __real_sceSifCallRpc(SifRpcClientData_t *cd, int fno, int mode,
                         void *send, int ssize, void *receive, int rsize,
                         SifRpcEndFunc_t endfunc, void *efarg);

static int physical_secr_port(int port)
{
    if (port >= 0 && port <= 1)
        return port + 2;
    return port;
}

static void bytes_to_hex(const unsigned char *data, unsigned int size, char *out)
{
    static const char digits[] = "0123456789abcdef";
    unsigned int i;

    for (i = 0; i < size; i++) {
        out[i * 2] = digits[data[i] >> 4];
        out[i * 2 + 1] = digits[data[i] & 0x0f];
    }
    out[size * 2] = '\0';
}

static int f2_trace_valid(const unsigned char *trace, unsigned char kind)
{
    return trace[0] == 'M' && trace[1] == 'G' &&
           trace[2] == 'F' && trace[3] == '2' &&
           trace[4] == 1 && trace[5] <= 2 && trace[6] == kind;
}

static const unsigned char *f2_record(const unsigned char *trace, unsigned int index)
{
    return &trace[8 + index * F2_RECORD_SIZE];
}

static int write_whole_file(const char *path, const void *data, unsigned int size)
{
    const unsigned char *p = (const unsigned char *)data;
    unsigned int done = 0;
    int fd = fileXioOpen(path, FIO_O_WRONLY | FIO_O_CREAT | FIO_O_TRUNC, 0666);

    if (fd < 0)
        return fd;
    while (done < size) {
        int rc = fileXioWrite(fd, p + done, size - done);
        if (rc <= 0) {
            fileXioClose(fd);
            return rc < 0 ? rc : -1;
        }
        done += (unsigned int)rc;
    }
    fileXioClose(fd);
    return 0;
}

void secr_trace_reset(void)
{
    memset(PreKbit, 0, sizeof(PreKbit));
    memset(PreKc, 0, sizeof(PreKc));
    memset(FinalKbit, 0, sizeof(FinalKbit));
    memset(FinalKc, 0, sizeof(FinalKc));
    memset(Icvps2, 0, sizeof(Icvps2));
    memset(F2KbitTrace, 0, sizeof(F2KbitTrace));
    memset(F2KcTrace, 0, sizeof(F2KcTrace));
    HavePreKbit = HavePreKc = 0;
    HaveFinalKbit = HaveFinalKc = HaveIcvps2 = 0;
    HaveF2KbitTrace = HaveF2KcTrace = 0;
    Summary[0] = '\0';
}

const char *secr_trace_summary(void)
{
    char a[33], b[33], c[33], d[33], e[17];

    bytes_to_hex(PreKbit, 16, a);
    bytes_to_hex(PreKc, 16, b);
    bytes_to_hex(FinalKbit, 16, c);
    bytes_to_hex(FinalKc, 16, d);
    bytes_to_hex(Icvps2, 8, e);
    snprintf(Summary, sizeof(Summary),
             "f2K=%u f2C=%u preKbit=%s preKc=%s finalKbit=%s finalKc=%s icv=%s",
             HaveF2KbitTrace ? (unsigned int)F2KbitTrace[5] : 0u,
             HaveF2KcTrace ? (unsigned int)F2KcTrace[5] : 0u,
             HavePreKbit ? a : "n/a", HavePreKc ? b : "n/a",
             HaveFinalKbit ? c : "n/a", HaveFinalKc ? d : "n/a",
             HaveIcvps2 ? e : "n/a");
    return Summary;
}

static int save_named(const char *run_dir, const char *name,
                      const void *data, unsigned int size)
{
    char path[SECR_TRACE_PATH_SIZE];

    snprintf(path, sizeof(path), "%s/%s", run_dir, name);
    return write_whole_file(path, data, size);
}

int secr_trace_save(const char *run_dir)
{
    char path[SECR_TRACE_PATH_SIZE];
    char text[2400];
    char pre_kbit_hex[33], pre_kc_hex[33];
    char final_kbit_hex[33], final_kc_hex[33], icv_hex[17];
    char k0in[17], k0out[17], k1in[17], k1out[17];
    char c0in[17], c0out[17], c1in[17], c1out[17];
    const unsigned char *kr0 = f2_record(F2KbitTrace, 0);
    const unsigned char *kr1 = f2_record(F2KbitTrace, 1);
    const unsigned char *cr0 = f2_record(F2KcTrace, 0);
    const unsigned char *cr1 = f2_record(F2KcTrace, 1);
    int first_error = 0;
    int rc;

    if (run_dir == NULL || run_dir[0] == '\0')
        return -1;

#define SAVE(name, data, have, size) \
    do { \
        if (have) { \
            rc = save_named(run_dir, name, data, size); \
            if (rc < 0 && first_error == 0) first_error = rc; \
        } \
    } while (0)

    SAVE("mecha-pre-kbit.bin", PreKbit, HavePreKbit, 16);
    SAVE("mecha-pre-kc.bin", PreKc, HavePreKc, 16);
    SAVE("final-kbit.bin", FinalKbit, HaveFinalKbit, 16);
    SAVE("final-kc.bin", FinalKc, HaveFinalKc, 16);
    SAVE("icvps2-trace.bin", Icvps2, HaveIcvps2, 8);
    SAVE("f2-kbit-trace.bin", F2KbitTrace, HaveF2KbitTrace, F2_TRACE_SIZE);
    SAVE("f2-kc-trace.bin", F2KcTrace, HaveF2KcTrace, F2_TRACE_SIZE);

    SAVE("f2-kbit-half0-input.bin", &kr0[4], HaveF2KbitTrace && F2KbitTrace[5] > 0, 8);
    SAVE("f2-kbit-half0-output.bin", &kr0[12], HaveF2KbitTrace && F2KbitTrace[5] > 0, 8);
    SAVE("f2-kbit-half1-input.bin", &kr1[4], HaveF2KbitTrace && F2KbitTrace[5] > 1, 8);
    SAVE("f2-kbit-half1-output.bin", &kr1[12], HaveF2KbitTrace && F2KbitTrace[5] > 1, 8);
    SAVE("f2-kc-half0-input.bin", &cr0[4], HaveF2KcTrace && F2KcTrace[5] > 0, 8);
    SAVE("f2-kc-half0-output.bin", &cr0[12], HaveF2KcTrace && F2KcTrace[5] > 0, 8);
    SAVE("f2-kc-half1-input.bin", &cr1[4], HaveF2KcTrace && F2KcTrace[5] > 1, 8);
    SAVE("f2-kc-half1-output.bin", &cr1[12], HaveF2KcTrace && F2KcTrace[5] > 1, 8);
#undef SAVE

    bytes_to_hex(PreKbit, 16, pre_kbit_hex);
    bytes_to_hex(PreKc, 16, pre_kc_hex);
    bytes_to_hex(FinalKbit, 16, final_kbit_hex);
    bytes_to_hex(FinalKc, 16, final_kc_hex);
    bytes_to_hex(Icvps2, 8, icv_hex);
    bytes_to_hex(&kr0[4], 8, k0in);
    bytes_to_hex(&kr0[12], 8, k0out);
    bytes_to_hex(&kr1[4], 8, k1in);
    bytes_to_hex(&kr1[12], 8, k1out);
    bytes_to_hex(&cr0[4], 8, c0in);
    bytes_to_hex(&cr0[12], 8, c0out);
    bytes_to_hex(&cr1[4], 8, c1in);
    bytes_to_hex(&cr1[12], 8, c1out);

    snprintf(text, sizeof(text),
             "PS2 Mecha Probe dev.10 native F2/50-53 trace\n"
             "No mcGetInfo/F3 reset, no explicit SecrAuthCard, no replay.\n\n"
             "0x94+0x95 pre-Kbit: %s\n"
             "Kbit half0: port=%u slot=%u mask=0x%02x failed=0x%02x in=%s out=%s\n"
             "Kbit half1: port=%u slot=%u mask=0x%02x failed=0x%02x in=%s out=%s\n"
             "final Kbit: %s\n\n"
             "0x96+0x97 pre-Kc:   %s\n"
             "Kc half0:   port=%u slot=%u mask=0x%02x failed=0x%02x in=%s out=%s\n"
             "Kc half1:   port=%u slot=%u mask=0x%02x failed=0x%02x in=%s out=%s\n"
             "final Kc:   %s\n\n"
             "0x98 ICVPS2: %s\n\n"
             "mask bits: bit0=F2/50 bit1=F2/51 bit2=F2/52 bit3=F2/53; 0x0f means the stock four-command transform completed.\n",
             HavePreKbit ? pre_kbit_hex : "not captured",
             HaveF2KbitTrace && F2KbitTrace[5] > 0 ? (unsigned int)kr0[0] : 0u,
             HaveF2KbitTrace && F2KbitTrace[5] > 0 ? (unsigned int)kr0[1] : 0u,
             HaveF2KbitTrace && F2KbitTrace[5] > 0 ? (unsigned int)kr0[2] : 0u,
             HaveF2KbitTrace && F2KbitTrace[5] > 0 ? (unsigned int)kr0[3] : 0xffu,
             HaveF2KbitTrace && F2KbitTrace[5] > 0 ? k0in : "n/a",
             HaveF2KbitTrace && F2KbitTrace[5] > 0 ? k0out : "n/a",
             HaveF2KbitTrace && F2KbitTrace[5] > 1 ? (unsigned int)kr1[0] : 0u,
             HaveF2KbitTrace && F2KbitTrace[5] > 1 ? (unsigned int)kr1[1] : 0u,
             HaveF2KbitTrace && F2KbitTrace[5] > 1 ? (unsigned int)kr1[2] : 0u,
             HaveF2KbitTrace && F2KbitTrace[5] > 1 ? (unsigned int)kr1[3] : 0xffu,
             HaveF2KbitTrace && F2KbitTrace[5] > 1 ? k1in : "n/a",
             HaveF2KbitTrace && F2KbitTrace[5] > 1 ? k1out : "n/a",
             HaveFinalKbit ? final_kbit_hex : "not captured",
             HavePreKc ? pre_kc_hex : "not captured",
             HaveF2KcTrace && F2KcTrace[5] > 0 ? (unsigned int)cr0[0] : 0u,
             HaveF2KcTrace && F2KcTrace[5] > 0 ? (unsigned int)cr0[1] : 0u,
             HaveF2KcTrace && F2KcTrace[5] > 0 ? (unsigned int)cr0[2] : 0u,
             HaveF2KcTrace && F2KcTrace[5] > 0 ? (unsigned int)cr0[3] : 0xffu,
             HaveF2KcTrace && F2KcTrace[5] > 0 ? c0in : "n/a",
             HaveF2KcTrace && F2KcTrace[5] > 0 ? c0out : "n/a",
             HaveF2KcTrace && F2KcTrace[5] > 1 ? (unsigned int)cr1[0] : 0u,
             HaveF2KcTrace && F2KcTrace[5] > 1 ? (unsigned int)cr1[1] : 0u,
             HaveF2KcTrace && F2KcTrace[5] > 1 ? (unsigned int)cr1[2] : 0u,
             HaveF2KcTrace && F2KcTrace[5] > 1 ? (unsigned int)cr1[3] : 0xffu,
             HaveF2KcTrace && F2KcTrace[5] > 1 ? c1in : "n/a",
             HaveF2KcTrace && F2KcTrace[5] > 1 ? c1out : "n/a",
             HaveFinalKc ? final_kc_hex : "not captured",
             HaveIcvps2 ? icv_hex : "not captured");

    snprintf(path, sizeof(path), "%s/secr-trace.txt", run_dir);
    rc = write_whole_file(path, text, (unsigned int)strlen(text));
    if (rc < 0 && first_error == 0)
        first_error = rc;

    return first_error;
}

int __wrap_sceSifBindRpc(SifRpcClientData_t *cd, int sid, int mode)
{
    int rc = __real_sceSifBindRpc(cd, sid, mode);

    if (rc >= 0 && cd->server != NULL) {
        if ((unsigned int)sid == SECRSIF_DOWNLOAD_HEADER)
            HeaderClient = cd;
        else if ((unsigned int)sid == SECRSIF_DOWNLOAD_GET_KBIT)
            KbitClient = cd;
        else if ((unsigned int)sid == SECRSIF_DOWNLOAD_GET_KC)
            KcClient = cd;
        else if ((unsigned int)sid == SECRSIF_DOWNLOAD_GET_ICVPS2)
            IcvClient = cd;
    }

    return rc;
}

int __wrap_sceSifCallRpc(SifRpcClientData_t *cd, int fno, int mode,
                         void *send, int ssize, void *receive, int rsize,
                         SifRpcEndFunc_t endfunc, void *efarg)
{
    int rc;

    if (fno == 1 && send != NULL) {
        if (cd == HeaderClient) {
            struct SecrSifDownloadHeaderParams *param;
            param = (struct SecrSifDownloadHeaderParams *)send;
            param->port = physical_secr_port(param->port);
        } else if (cd == KbitClient) {
            struct SecrSifDownloadGetKbitParams *param;
            param = (struct SecrSifDownloadGetKbitParams *)send;
            param->port = physical_secr_port(param->port);
        } else if (cd == KcClient) {
            struct SecrSifDownloadGetKcParams *param;
            param = (struct SecrSifDownloadGetKcParams *)send;
            param->port = physical_secr_port(param->port);
        }
    }

    rc = __real_sceSifCallRpc(cd, fno, mode, send, ssize,
                              receive, rsize, endfunc, efarg);

    if (rc >= 0 && fno == 1 && receive != NULL) {
        if (cd == KbitClient) {
            struct SecrSifDownloadGetKbitParams *param;
            const unsigned char *trace = (const unsigned char *)receive + SECR_F2_TRACE_RPC_OFFSET;
            param = (struct SecrSifDownloadGetKbitParams *)receive;

            if (f2_trace_valid(trace, 'K')) {
                memcpy(F2KbitTrace, trace, F2_TRACE_SIZE);
                HaveF2KbitTrace = 1;
                if (trace[5] > 0) {
                    memcpy(PreKbit, (const unsigned char *)receive + SECR_PREKEY_RPC_OFFSET, 16);
                    HavePreKbit = 1;
                }
            }
            if (param->result != 0) {
                memcpy(FinalKbit, param->kbit, 16);
                HaveFinalKbit = 1;
            }
        } else if (cd == KcClient) {
            struct SecrSifDownloadGetKcParams *param;
            const unsigned char *trace = (const unsigned char *)receive + SECR_F2_TRACE_RPC_OFFSET;
            param = (struct SecrSifDownloadGetKcParams *)receive;

            if (f2_trace_valid(trace, 'C')) {
                memcpy(F2KcTrace, trace, F2_TRACE_SIZE);
                HaveF2KcTrace = 1;
                if (trace[5] > 0) {
                    memcpy(PreKc, (const unsigned char *)receive + SECR_PREKEY_RPC_OFFSET, 16);
                    HavePreKc = 1;
                }
            }
            if (param->result != 0) {
                memcpy(FinalKc, param->kc, 16);
                HaveFinalKc = 1;
            }
        } else if (cd == IcvClient) {
            struct SecrSifDownloadGetIcvps2Params *param;
            param = (struct SecrSifDownloadGetIcvps2Params *)receive;
            if (param->result != 0) {
                memcpy(Icvps2, param->icvps2, 8);
                HaveIcvps2 = 1;
            }
        }
    }

    return rc;
}
