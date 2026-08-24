/* SPDX-License-Identifier: MIT */
/*
 * Translate libmc-style logical memory-card ports (0/1) to the physical SIO2
 * memory-card channels (2/3) expected by SECRMAN CardAuth.
 *
 * This behavior was already hardware-validated in ps2-magicgate-inspector.
 * Keep the translation at the SECRSIF boundary so the rest of Mecha Probe can
 * continue to report ordinary mc0/mc1 semantics to the user and evidence log.
 */

#include <sifrpc.h>
#include <libsecr-common.h>
#include <secrsif.h>

static SifRpcClientData_t *HeaderClient;
static SifRpcClientData_t *KbitClient;
static SifRpcClientData_t *KcClient;

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
    }

    return rc;
}

int __wrap_sceSifCallRpc(SifRpcClientData_t *cd, int fno, int mode,
                         void *send, int ssize, void *receive, int rsize,
                         SifRpcEndFunc_t endfunc, void *efarg)
{
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

    return __real_sceSifCallRpc(cd, fno, mode, send, ssize,
                                receive, rsize, endfunc, efarg);
}
