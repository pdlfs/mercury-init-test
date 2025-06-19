/*
 * Copyright (c) 2025, Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY
 * WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * mercury-init-test.c  test mercury init
 * 11-Apr-2025  chuck@ece.cmu.edu
 */

#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <mercury.h>

/*
 * mercury-init-test: init mercury using the requested plugin and
 * print out our self-address in text format.
 *
 * usage: mercury-init-test [flags] spec
 *
 * where spec is 'bmi+tcp' etc.
 *
 * flags:
 *  -a        - enable auto_sm mode
 *  -d <log>  - run as daemon and print output to given log file
 *  -n        - init with na listen=false (def=true)
 *  -s        - IP subnet spec (def=NULL)
 */

/*
 * init info:
 * struct hg_init_info: defaults set in HG_INIT_INFO_INITIALIZER
 * 
 * [1] na_init_info   (see below)
 *
 * [2] na_class -- allows NA to be setup before HG.  default=NULL
 *
 * [3] request_post_init -- only used if HG_Init_opt with listen==TRUE
 *      - number of unexpected RPC bufs posted at init() time for
 *        inbound RPC requests.  default (if 0): 512
 *     => default 512 (by setting init val to 0)
 *
 * [4] request_post_incr -- how many additional unexpected RPC bufs
 *        to allocate if we run out of the ones allocated in [3].
 *        if this is zero, inbound RPCs are blocked and the NA layer
 *        is supposed to internally queue inbound requests until
 *        a buffer req becomes available
 *        default (if 0): 512, if <0 sets to 0.
 *         note: if you set this to 0 (by setting request_post_incr < 0)
 *               it can disrupt user-level flow control schemes
 *     => default 512 (by setting init val to 0)
 *
 * [5] auto_sm -- allocated an internally managed instance of na+sm
 *        and route RPCs to processes on the same host to it  default=false.
 *          notes: works with NA_SM_Host_id_get() => saved in class->host_id
 *           - uses gethostid() to get unique host ID (see linux man page)
 *               - track local host_id, and one in hg_addr_t
 *        
 * [5b] sm_info_string -- only used if auto_sm is true.  if set, addition info
 *        passed to HG_Init for the na+sm instance.  instead of initing
 *        it as 'na+sm' it does sprintf('na+sm://%s', sm_info_string).
 *        default=NULL
 *
 * [6] checksum_level -- ( HG_CHECKSUM_NONE, HG_CHECKSUM_RPC_HEADERS,
 *                      HG_CHECKSUM_RPC_PAYLOAD ).
 *        controls mercury's checksum.   default=HG_CHECKSUM_NONE
 *
 * [7] no_bulk_eager -- turn on to prevent Mercury from inlining
 *        small bulk data inside of a serialized bulk handle
 *        instead of using rdma.   (the serialized bulk handle
 *        is normally sent in an RPC ...).
 *        default=false
 *          notes: sets flag HG_PROC_BULK_EAGER, see hg_proc_hg_bulk_t()
 *                 at encode time, compute size needed to inline data
 *                 and if room set HG_BULK_EAGER flag
 *                 goes into HG_Bulk_serialize()/hg_bulk_serialize()
 *
 * [8] no_loopback -- turn on to disable internal loopback (bypasses
 *                    NA layer when you send an RPC to yourself).
 *                    default: false
 *
 * [9] stats -- prints stats at exit time.  default=false
 *
 * [10] no_multi_recv -- disable multirecv when available and post
 *                       sep buffers.  default=false
 *
 * [11] release_input_early -- release input buffers after
 *        HG_Get_input() (rather than at handle release).
 *        (mainly relevant if RPC execution time is long)
 *        default=false
 *
 * [12] traffic_class -- preferred traffic class -- a hook in for
 *        QOS setting (best effort, low latency, bulk, etc.) if
 *        supported.  default=NA_TC_UNSPEC -- leave it to plugin to choose
 *
 * [13] no_overflow -- disables proc "extra buffer" feature.
 *                     default: false
 *
 * [14] multi_recv_op_max: number of multi-recv buffer posted.
 *      allows multiple recv's in a single larger buffer.  default=0
 *
 * [15] multi_recv_copy_threshold -- when to start copying data
 *      to release mutli-recv buffers.  start copying when at
 *      at most multi_recv_copy_threshold buffers remain.
 *      default=0.
 *
 * na_init_info:
 * [A] ip_subnet -- preferred IP subnet, default=NULL
 *     => useful for machines with more than one IP network interface
 *        to select which one to use.
 *
 * [B] auth_key -- for comm on some special fabrics, default=NULL
 * [C] max_unexpected_size -- hint for NA plugin, def=0
 * [D] max_expected_size -- hint for NA plugin, def=0
 * [E] progress_mode -- set to NA_NO_BLOCK to spin, def=0
 * [F] addr_format -- pref addr format (unspec, IPv4, IPv6, native)
 *         default=unspec
 * [G] max_contexts -- max# contexts we expect. def=1
 * [H] thread_mode -- set to NA_THREAD_MODE_SINGLE to if you are
 *                    single threaded to disable multithread protection
 *                    def=0
 * [I] request_mem_device -- req support for transfers to/from memory
 *                           devices (e.g. GPU, etc.) def=false
 * [J] traffic_class -- same as [12] above - redundant?
 */

static FILE *outfp = NULL;   /* where to write output msgs */

void usage(char *prog) {
    fprintf(stderr, "usage: %s [flags] spec\n", prog);
    fprintf(stderr, "where spec is 'bmi+tcp' etc.\n\n");
    fprintf(stderr, "flags:\n");
    fprintf(stderr, "\t-a     - enable auto_sm mode\n");
    fprintf(stderr, "\t-d log - run in daemon mode, output to log file\n");
    fprintf(stderr, "\t-n     - init with na listen=false (def=true)\n");
    fprintf(stderr, "\t-s s   - IP subnet spec (def=NULL)\n");
    fprintf(stderr, "\n");
    exit(1);
}

struct nstate {
    int *stop;
    hg_context_t *ctx;
};

/*
 * run_network: network support pthread
 */
static void *run_network(void *arg) {
    struct nstate *n = (struct nstate *)arg;
    unsigned int actual;
    hg_return_t ret;
    actual = 0;

    fprintf(outfp, "network thread running\n");

    /* run until we are asked to stop */
    while (*(n->stop) == 0) {

        do {
            ret = HG_Trigger(n->ctx, 0, 1, &actual);
        } while (ret == HG_SUCCESS && actual);

        /* recheck, trigger may set stop_progthread */
        if (*(n->stop) == 0) {
            HG_Progress(n->ctx, 100);
        }

    }
    fprintf(outfp, "network thread complete\n");
    return(NULL);
}

int main(int argc, char **argv) {
    char *prog = argv[0];

    /* default flag values */
    int auto_sm = 0;
    char *daemonlog = NULL;
    int listen = HG_TRUE;
    char *subnet = NULL;

    int ch, rv;
    char *spec;
    struct hg_init_info initinfo; /* for HG_Init_opt() */
    hg_class_t *hgclass;
    hg_context_t *hgctx;
    int stop_progthread;     /* set to stop the progress thread */
    struct nstate ns;
    pthread_t nthread;       /* network thread */
    hg_addr_t myaddr;
    hg_size_t asz;
    char *buf;
    
    setlinebuf(stdout);
    outfp = stdout;

    while ((ch = getopt(argc, argv, "ad:ns:")) != -1) {
        switch (ch) {   /* arg is in optarg */
             case 'a':
                 auto_sm = 1;
                 break;
             case 'd':
                 daemonlog = optarg;
                 break;
             case 'n':
                 listen = HG_FALSE;
                 break;
             case 's':
                 subnet = optarg;
                 break;
             case '?':
             default:
                 usage(prog);
             }
    }
    argc -= optind;
    argv += optind;

    if (argc != 1)
        usage(prog);
    spec = argv[0];

    fprintf(outfp, "mercury-init-test on %s\n", spec);
    fprintf(outfp, "\tauto_sm mode = %d\n", auto_sm);
    if (!daemonlog) {
        fprintf(outfp, "\tdaemon = off\n");
    } else {
        fprintf(outfp, "\tdaemon = on (logfile=%s)\n", daemonlog);
    }
    fprintf(outfp, "\tlisten mode = %d\n", listen);
    fprintf(outfp, "\tsubnet: %s\n", (subnet) ? subnet : "<none>");

    initinfo = HG_INIT_INFO_INITIALIZER;  /* see mercury_core_types.h */
    initinfo.auto_sm = auto_sm;
    initinfo.na_init_info.ip_subnet = subnet;

    if (daemonlog) {
        FILE *newout = fopen(daemonlog, "a");
        if (!newout) {
            fprintf(outfp, "cannot open daemonlog - failed!\n");
            exit(1);
        }
        setlinebuf(newout);
        outfp = newout;
        if (daemon(1, 0) == -1) {
            fprintf(outfp, "daemon() called failed w/-1\n");
            exit(1);
        }
    }

    hgclass = HG_Init_opt(spec, listen, &initinfo);
    if (!hgclass) {
        fprintf(outfp, "HG_Init_opt(%s,%d) failed\n", spec, listen);
        exit(1);
    }
    hgctx = HG_Context_create(hgclass);
    if (!hgctx) {
        fprintf(outfp, "HG_Context_create failed\n");
        exit(1);
    }

    stop_progthread = 0;
    ns.stop = &stop_progthread;
    ns.ctx = hgctx;
    rv = pthread_create(&nthread, NULL, run_network, (void*)&ns);
    if (rv != 0) {
        fprintf(outfp, "pthread create srvr failed %d\n", rv);
        exit(1);
    }
    sleep(1);

    /* get our address string size */
    if (HG_Addr_self(hgclass, &myaddr) != HG_SUCCESS) {
        fprintf(outfp, "HG_Addr_self failed?!\n");
        exit(1);
    }

    if (HG_Addr_to_string(hgclass, NULL, &asz, myaddr) != HG_SUCCESS) {
        fprintf(outfp, "addr to string failed to give needed size\n");
        exit(1);
    }
    if (asz < 1) {
        fprintf(outfp, "bad buffer size?\n");
        exit(1);
    }

    fprintf(outfp, "\n");

    fprintf(outfp, "requested addr buf size: %" PRId64 "\n", asz);
    buf = (char *)malloc(asz + 1);
    if (!buf) {
        fprintf(outfp, "malloc fail\n");
        exit(1);
    }
    buf[asz] = 'x';
    if (HG_Addr_to_string(hgclass, buf, &asz, myaddr) != HG_SUCCESS) {
        fprintf(outfp, "addr to string failed\n");
        exit(1);
    }
    if (buf[asz] != 'x') {
        fprintf(outfp, "buf sanity check\n");
        exit(1);
    }
    if (buf[asz-1] != '\0') {
        fprintf(outfp, "buf sanity check2\n");
        exit(1);
    }
    fprintf(outfp, "listening at: %s\n", buf);
    fprintf(outfp, "\n");

    free(buf);
    if (HG_Addr_free(hgclass, myaddr) != HG_SUCCESS) {
        fprintf(outfp, "HG_Addr_free failed?!\n");
        exit(1);
    }

    /*
     * done, shutdown
     */
    stop_progthread = 1;
    pthread_join(nthread, NULL);
    fprintf(outfp, "destroy context and finalize mercury\n");
    HG_Context_destroy(hgctx);
    HG_Finalize(hgclass);

    exit(0);
}
