/* SPDX-License-Identifier: MIT */
/*
 * Translate libmc-style logical memory-card ports (0/1) to the physical SIO2
 * memory-card channels (2/3) expected by SECRMAN CardAuth.
 *
 * dev.8 captures one explicit SecrAuthCard() transcript plus the normal one-pass
 * KELF crypto values. No MechaCon or CardAuth command is replayed by this EE
 * shim. The instrumented SECRMAN returns diagnostics in unused areas of the
 * existing 0x1000-byte SECRSIF replies.
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

#define SECR_TRACE_RPC_OFFSET 0x100u
#define SECR_TRACE_PATH_SIZE 192u
#define AUTH_TRACE_SIZE 80u

static SifRpcClientData_t *HeaderClient;
static SifRpcClientData_t *KbitClient;
static SifRpcClientData_t *KcClient;
static SifRpcClientData_t *IcvClient;

static unsigned char PreKbit[16];
static unsigned char PreKc[16];
static unsigned char FinalKbit[16];
static unsigned char FinalKc[16];
static unsigned char Icvps2[8];
static unsigned char AuthTrace[AUTH_TRACE_SIZE];
static int HavePreKbit;
static int HavePreKc;
static int HaveFinalKbit;
static int HaveFinalKc;
static int HaveIcvps2;
static int HaveAuthTrace;
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
    memset(AuthTrace, 0, sizeof(AuthTrace));
    HavePreKbit = HavePreKc = 0;
    HaveFinalKbit = HaveFinalKc = HaveIcvps2 = HaveAuthTrace = 0;
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
             "auth=%s cnum=%u preKbit=%s preKc=%s finalKbit=%s finalKc=%s icv=%s",
             HaveAuthTrace ? "captured" : "n/a",
             HaveAuthTrace ? (unsigned int)AuthTrace[5] : 0u,
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
    char text[1800];
    char a[33], b[33], c[33], d[33], e[17];
    char iv[17], material[17], nonce[17], mc1[17], mc2[17], mc3[17];
    char cr1[17], cr2[17], cr3[17];
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
    SAVE("auth-trace.bin", AuthTrace, HaveAuthTrace, AUTH_TRACE_SIZE);

    if (HaveAuthTrace) {
        SAVE("card-iv.bin", &AuthTrace[8], 1, 8);
        SAVE("card-material.bin", &AuthTrace[16], 1, 8);
        SAVE("card-nonce.bin", &AuthTrace[24], 1, 8);
        SAVE("mecha-challenge1.bin", &AuthTrace[32], 1, 8);
        SAVE("mecha-challenge2.bin", &AuthTrace[40], 1, 8);
        SAVE("mecha-challenge3.bin", &AuthTrace[48], 1, 8);
        SAVE("card-response1.bin", &AuthTrace[56], 1, 8);
        SAVE("card-response2.bin", &AuthTrace[64], 1, 8);
        SAVE("card-response3.bin", &AuthTrace[72], 1, 8);
    }
#undef SAVE

    bytes_to_hex(PreKbit, 16, a);
    bytes_to_hex(PreKc, 16, b);
    bytes_to_hex(FinalKbit, 16, c);
    bytes_to_hex(FinalKc, 16, d);
    bytes_to_hex(Icvps2, 8, e);
    bytes_to_hex(&AuthTrace[8], 8, iv);
    bytes_to_hex(&AuthTrace[16], 8, material);
    bytes_to_hex(&AuthTrace[24], 8, nonce);
    bytes_to_hex(&AuthTrace[32], 8, mc1);
    bytes_to_hex(&AuthTrace[40], 8, mc2);
    bytes_to_hex(&AuthTrace[48], 8, mc3);
    bytes_to_hex(&AuthTrace[56], 8, cr1);
    bytes_to_hex(&AuthTrace[64], 8, cr2);
    bytes_to_hex(&AuthTrace[72], 8, cr3);

    snprintf(text, sizeof(text),
             "PS2 Mecha Probe dev.8 explicit-auth + one-pass SECR trace\n"
             "auth captured:       %s\n"
             "auth cnum/port/slot: %u / %u / %u\n"
             "CardIV:              %s\n"
             "CardMaterial:        %s\n"
             "CardNonce:           %s\n"
             "MechaChallenge1:     %s\n"
             "MechaChallenge2:     %s\n"
             "MechaChallenge3:     %s\n"
             "CardResponse1:       %s\n"
             "CardResponse2:       %s\n"
             "CardResponse3:       %s\n"
             "\n0x94+0x95 pre-Kbit: %s\n"
             "0x96+0x97 pre-Kc:   %s\n"
             "final Kbit:         %s\n"
             "final Kc:           %s\n"
             "0x98 ICVPS2:        %s\n"
             "\nOne explicit SecrAuthCard established the captured session immediately before the normal KELF transaction. No SCMD/CardAuth command was replayed for tracing.\n",
             HaveAuthTrace ? "yes" : "no",
             HaveAuthTrace ? (unsigned int)AuthTrace[5] : 0u,
             HaveAuthTrace ? (unsigned int)AuthTrace[6] : 0u,
             HaveAuthTrace ? (unsigned int)AuthTrace[7] : 0u,
             HaveAuthTrace ? iv : "not captured",
             HaveAuthTrace ? material : "not captured",
             HaveAuthTrace ? nonce : "not captured",
             HaveAuthTrace ? mc1 : "not captured",
             HaveAuthTrace ? mc2 : "not captured",
             HaveAuthTrace ? mc3 : "not captured",
             HaveAuthTrace ? cr1 : "not captured",
             HaveAuthTrace ? cr2 : "not captured",
             HaveAuthTrace ? cr3 : "not captured",
             HavePreKbit ? a : "not captured",
             HavePreKc ? b : "not captured",
             HaveFinalKbit ? c : "not captured",
             HaveFinalKc ? d : "not captured",
             HaveIcvps2 ? e : "not captured");

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
            param = (struct SecrSifDownloadGetKbitParams *)receive;
            if (param->result != 0) {
                memcpy(FinalKbit, param->kbit, 16);
                memcpy(PreKbit, (const unsigned char *)receive + SECR_TRACE_RPC_OFFSET, 16);
                HaveFinalKbit = 1;
                HavePreKbit = 1;
            }
        } else if (cd == KcClient) {
            struct SecrSifDownloadGetKcParams *param;
            param = (struct SecrSifDownloadGetKcParams *)receive;
            if (param->result != 0) {
                memcpy(FinalKc, param->kc, 16);
                memcpy(PreKc, (const unsigned char *)receive + SECR_TRACE_RPC_OFFSET, 16);
                HaveFinalKc = 1;
                HavePreKc = 1;
            }
        } else if (cd == IcvClient) {
            struct SecrSifDownloadGetIcvps2Params *param;
            param = (struct SecrSifDownloadGetIcvps2Params *)receive;
            if (param->result != 0) {
                const unsigned char *auth = (const unsigned char *)receive + SECR_TRACE_RPC_OFFSET;
                memcpy(Icvps2, param->icvps2, 8);
                HaveIcvps2 = 1;
                if (auth[0] == 0x4d && auth[1] == 0x47 && auth[2] == 0x41 && auth[3] == 0x38 && auth[4] == 1) {
                    memcpy(AuthTrace, auth, AUTH_TRACE_SIZE);
                    HaveAuthTrace = 1;
                }
            }
        }
    }

    return rc;
}
