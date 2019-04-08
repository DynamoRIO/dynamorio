/* **********************************************************
 * Copyright (c) 2015-2018 Google, Inc.  All rights reserved.
 * **********************************************************/

/*
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of Google, Inc. nor the names of its contributors may be
 *   used to endorse or promote products derived from this software without
 *   specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL GOOGLE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/*
 * ssljack hijacks (wraps) interesting OpenSSL and GnuTLS functions using the
 * drwrap extension. ssljack creates separate read and write files per SSL
 * context containing all the data the app read and wrote.
 *
 * Hacked together by Dhiru Kholia (dhiru [at] openwall [dot] com)
 *
 * "ssl-jack" was initially published on the https://github.com/kholia/dedrop
 * page by me. This revision integrates changes from Dragos-George Comaneci, to
 * improve logging.
 */

#include "dr_api.h"
#include "utils.h"
#include "drwrap.h"
#include "drmgr.h"
#include <stdio.h>
#include <string.h>

/* OpenSSL and GnuTLS - shared wrappers */
static void
wrap_pre_SSL_write(void *wrapcxt, OUT void **user_data);
static void
wrap_pre_SSL_read(void *wrapcxt, OUT void **user_data);
static void
wrap_post_SSL_read(void *wrapcxt, void *user_data);

struct SSL_read_data {
    unsigned char *read_buffer;
    void *ssl;
};

static void
module_load_event(void *drcontext, const module_data_t *mod, bool loaded)
{
    app_pc towrap = (app_pc)dr_get_proc_address(mod->handle, "SSL_write");
    if (towrap != NULL) {
        bool ok = drwrap_wrap(towrap, wrap_pre_SSL_write, NULL);
        if (!ok) {
            dr_fprintf(STDERR, "Couldn’t wrap SSL_write\n");
            DR_ASSERT(ok);
        }
    }

    towrap = (app_pc)dr_get_proc_address(mod->handle, "SSL_read");
    if (towrap != NULL) {
        bool ok = drwrap_wrap(towrap, wrap_pre_SSL_read, wrap_post_SSL_read);
        if (!ok) {
            dr_fprintf(STDERR, "Couldn’t wrap SSL_read\n");
            DR_ASSERT(ok);
        }
    }

    towrap = (app_pc)dr_get_proc_address(mod->handle, "gnutls_record_send");
    if (towrap != NULL) {
        bool ok = drwrap_wrap(towrap, wrap_pre_SSL_write, NULL);
        if (!ok) {
            dr_fprintf(STDERR, "Couldn’t wrap gnutls_record_send\n");
            DR_ASSERT(ok);
        }
    }

    towrap = (app_pc)dr_get_proc_address(mod->handle, "gnutls_record_recv");
    if (towrap != NULL) {
        bool ok = drwrap_wrap(towrap, wrap_pre_SSL_read, wrap_post_SSL_read);
        if (!ok) {
            dr_fprintf(STDERR, "Couldn’t wrap gnutls_record_recv\n");
            DR_ASSERT(ok);
        }
    }
}

static void
event_exit(void)
{
    drwrap_exit();
    drmgr_exit();
}

DR_EXPORT void
dr_init(client_id_t id)
{
    dr_set_client_name("DynamoRIO client 'ssljack'", "http://dynamorio.org/issues");
    /* make it easy to tell, by looking at log file, which client executed */
    dr_log(NULL, DR_LOG_ALL, 1, "Client ssljack initializing\n");
#ifdef SHOW_RESULTS
    if (dr_is_notify_on()) {
        dr_fprintf(STDERR, "Client ssljack running! See trace-* files for SSL logs!\n");
    }
#endif

    drmgr_init();
    drwrap_init();
    dr_register_exit_event(event_exit);
    drmgr_register_module_load_event(module_load_event);
}

static void
wrap_pre_SSL_write(void *wrapcxt, OUT void **user_data)
{
    /* int SSL_write(SSL *ssl, const void *buf, int num);
     *
     * ssize_t gnutls_record_send(gnutls_session_t session,
     *                            const void * data, size_t sizeofdata);
     */

    void *ssl = (void *)drwrap_get_arg(wrapcxt, 0);
    unsigned char *buf = (unsigned char *)drwrap_get_arg(wrapcxt, 1);
    size_t sz = (size_t)drwrap_get_arg(wrapcxt, 2);

    /* By generating unique filenames (per SSL context), we are able to
     * simplify logging of SSL traffic (no file locking is required).
     */
    char filename[MAXIMUM_PATH] = { 0 };
    dr_snprintf(filename, BUFFER_SIZE_ELEMENTS(filename), "trace-%x.write", ssl);
    NULL_TERMINATE_BUFFER(filename);
    FILE *fp = fopen(filename, "ab+");
    /* Error handling of logging operations isn't critical - in fact, we don't
     * even know what to do in such error conditions, so we simply return!
     */
    if (!fp) {
        dr_fprintf(STDERR, "Couldn’t open the output file %s\n", filename);
        return;
    }

    /* We assume that SSL_write always succeeds and writes the whole buffer. */
    fwrite(buf, 1, sz, fp);
    fclose(fp);
}

static void
wrap_pre_SSL_read(void *wrapcxt, OUT void **user_data)
{
    struct SSL_read_data *sd;

    /* int SSL_read(SSL *ssl, void *buf, int num);
     *
     * ssize_t gnutls_record_recv(gnutls_session_t session,
     *                            void * data, size_t sizeofdata);
     */

    sd = dr_global_alloc(sizeof(struct SSL_read_data));
    sd->read_buffer = (unsigned char *)drwrap_get_arg(wrapcxt, 1);
    sd->ssl = (void *)drwrap_get_arg(wrapcxt, 0);

    *user_data = (void *)sd;
}

static void
wrap_post_SSL_read(void *wrapcxt, void *user_data)
{
    struct SSL_read_data *sd = (struct SSL_read_data *)user_data;

    int actual_read = (int)(ptr_int_t)drwrap_get_retval(wrapcxt);

    char filename[MAXIMUM_PATH] = { 0 };
    dr_snprintf(filename, BUFFER_SIZE_ELEMENTS(filename), "trace-%x.read", sd->ssl);
    NULL_TERMINATE_BUFFER(filename);
    FILE *fp = fopen(filename, "ab+");
    if (!fp) {
        dr_fprintf(STDERR, "Couldn’t open the output file %s\n", filename);
        dr_global_free(user_data, sizeof(struct SSL_read_data));
        return;
    }

    if (actual_read > 0) {
        fwrite(sd->read_buffer, 1, actual_read, fp);
    }
    fclose(fp);

    dr_global_free(user_data, sizeof(struct SSL_read_data));
}
