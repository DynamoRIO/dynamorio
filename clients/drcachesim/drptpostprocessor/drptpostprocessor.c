/* **********************************************************
 * Copyright (c) 2022 Google, Inc.  All rights reserved.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL VMWARE, INC. OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

/* DrPTPostprocessor.c
 * command-line tool for decoding a PT trace, and converting it into an instruction-only
 * memtrace composed of 'memref_t's
 * XXX: Currently, it only counts and prints the instruction count in the trace data.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <linux/limits.h>

#include "load_elf.h"
#include "pt_cpu.h"
#include "intel-pt.h"
#include "libipt-sb.h"

#define IN
#define OUT
#define INOUT

/* DR IntelPT decoder*/
typedef struct _dript_decoder_t {
    /* libipt instruction decoder*/
    struct pt_insn_decoder *ptdecoder;

    /* The image section cache. */
    struct pt_image_section_cache *iscache;

    /* The sideband session. */
    struct pt_sb_session *sbsession;

    /* The perf event sideband decoder configuration. */
    struct pt_sb_pevent_config sbpevent;
} dript_decoder_t;

/* A collection of options. */
typedef struct _dript_options_t {
    /* Print statistics. */
    uint32_t print_stats : 1;
} dript_options_t;

/* A collection of output. */
typedef struct _dript_output_t {
    /* The number of instructions. */
    uint64_t instr_count;
} dript_output_t;

/* Diagnostic output for error event.
 * It will be called when 'pt_insn_sync_forward' return error status or decoder end before
 * reach pte_eos.
 */
static void
diagnose_error(struct pt_insn_decoder *decoder IN, int errcode IN, const char *errtype IN,
               uint64_t ip IN)
{
    int err;
    uint64_t pos;

    err = -pte_internal;
    pos = 0ull;

    err = pt_insn_get_offset(decoder, &pos);

    if (err < 0) {
        fprintf(stderr, "could not determine offset: %s\n", pt_errstr(pt_errcode(err)));
        fprintf(stderr, "[?, %" PRIx64 ": %s: %s]\n", ip, errtype,
                pt_errstr(pt_errcode(errcode)));
    } else
        fprintf(stderr, "[%" PRIx64 ", %" PRIx64 ": %s: %s]\n", pos, ip, errtype,
                pt_errstr(pt_errcode(errcode)));
}

static void
process_decode(dript_decoder_t *decoder IN, const dript_options_t *options IN,
               dript_output_t *output OUT)
{
    for (;;) {
        struct pt_insn insn;
        int status;

        memset(&insn, 0, sizeof(insn));

        /* Sync decoder to first Packet Stream Boundary (PSB) packet, and then decode
         * instructions. If there is no PSB packet, the decoder will be synced to the end
         * of the trace. If error occurs, we will call diagnose_error to print the error
         * information.
         * What is PSB packets?
         * Below answer is quoted from Intel® 64 and IA-32 Architectures Software
         * Developer’s Manual 32.1.1.1 Packet Summary
         * “Packet Stream Boundary (PSB) packets: PSB packets act as ‘heartbeats’ that are
         * generated at regular intervals (e.g., every 4K trace packet bytes). These
         * packets allow the packet decoder to find the packet boundaries within the
         * output data stream; a PSB packet should be the first packet that a decoder
         * looks for when beginning to decode a trace.”
         */
        status = pt_insn_sync_forward(decoder->ptdecoder);
        if (status < 0) {
            if (status == -pte_eos)
                break;
            diagnose_error(decoder->ptdecoder, status, "sync error", insn.ip);
            break;
        }

        /* Decode instructions. */
        for (;;) {
            /* Handle next status and all pending perf events. */
            int nextstatus = status;
            int errcode = 0;
            struct pt_image *image;
            while (nextstatus & pts_event_pending) {
                struct pt_event event;

                nextstatus = pt_insn_event(decoder->ptdecoder, &event, sizeof(event));
                if (nextstatus < 0)
                    break;

                /* Use a sideband session to check if pt_event is an image switch event.
                 * If so, change the image in pt_insn_deocer to the target image. */
                image = NULL;
                errcode = pt_sb_event(decoder->sbsession, &image, &event, sizeof(event),
                                      stdout, 0);
                if (errcode < 0)
                    break;
                if (image == NULL)
                    continue;

                errcode = pt_insn_set_image(decoder->ptdecoder, image);
                if (errcode < 0) {
                    break;
                }
            }
            if (nextstatus < 0) {
                diagnose_error(decoder->ptdecoder, nextstatus, "handle insn event error",
                               insn.ip);
                break;
            }
            if (errcode < 0) {
                diagnose_error(decoder->ptdecoder, errcode, "handle sideband event error",
                               insn.ip);
                break;
            }

            if ((nextstatus & pts_eos) != 0) {
                break;
            }

            /* Decode instructions. */
            status = pt_insn_next(decoder->ptdecoder, &insn, sizeof(insn));
            if (status < 0) {
                diagnose_error(decoder->ptdecoder, status, "decode error", insn.ip);
                break;
            }

            output->instr_count += 1;
            /* TODO i#5505: Converting insn into an instruction-only memref_t */
        }
    }
}

static void
print_stats(dript_output_t *output IN)
{
    printf("Number of Instructions: %" PRIu64 ".\n", output->instr_count);
}

static int
usage(const char *prog IN)
{
    printf("Usage: %s [<options>]", prog);
    printf("Command-line tool for decoding a PT trace, and converting it into an "
           "instruction-only memtrace composed of 'memref_t's.\n");
    printf("This version only counts and prints the instruction count in the trace "
           "data.\n\n");
    printf("Options:\n");
    printf("  --help|-h                    this text.\n");
    printf("  --stats                      print trace statistics.\n");
    printf("  --pt <file>                  load the processor trace data from <file>.\n");
    printf("  --img <file>:begin-end:<base>\n");
    printf("                               load a image binary from <file> at address "
           "<base>.\n");
    printf("\n");
    printf("Below is sideband mode\n");
    printf("  --cpu none|f/m[/s]           set cpu to the given value and decode "
           "according to:\n");
    printf("                               none     spec (default)\n");
    printf("                               f/m[/s]  family/model[/stepping]\n");
    printf("  --pevent:sample-type <val>   set perf_event_attr.sample_type to <val> "
           "(default: 0).\n");
    printf("  --pevent:primary/secondary <file>\n");
    printf("                               load a perf_event sideband stream from "
           "<file>.\n");
    printf("                               the offset range begin and range end must be "
           "given.\n");
    printf("  --pevent:kernel-start <val>  the start address of the kernel.\n");
    printf("  --pevent:kcore <file>        load the kernel from a core dump.\n");
    printf("\n");
    printf("If the trace data is recoder from other machine,  you must specify at least "
           "one binary file (--img).\n");
    printf("You must specify exactly one processor trace file (--pt).\n");

    return 1;
}

/* Load an image file into pt decoder.
 *
 * Returns zero on success, a negative error code otherwise.
 */
static int
load_image(char *arg IN, const char *prog IN, struct pt_image *image INOUT,
           struct pt_image_section_cache *iscache INOUT)
{
    uint64_t base, foffset, fend, fsize;
    char filepath[PATH_MAX];
    int isid, errcode;

    /* Preprocess a filename argument. The image file is followed by a file range and the
     * load base addr.*/
    if (sscanf(arg, "%[^:]:%x-%x:%x", filepath, &foffset, &fend, &base) != 4) {
        return -1;
    }
    if (fend <= foffset) {
        return -1;
    }
    fsize = fend - foffset;

    /* libipt uses 'iscache' to store the image sections and use 'image' to store the
     * image identifier. So to initialize the cache, we need call pt_iscache_add_file()
     * first to load the image to cache. Then we call pt_image_add_cached() to add the
     * image section identifier to the image.
     */
    /* pt_iscache_add_file():  add a file section to an image section cache returns an
     * image section identifier(ISID) that uniquely identifies the *section in this cache
     */
    isid = pt_iscache_add_file(iscache, filepath, foffset, fsize, base);
    if (isid < 0) {
        fprintf(stderr, "%s: failed to add %s at 0x%" PRIx64 " to iscache: %s.\n", prog,
                filepath, base, pt_errstr(pt_errcode(isid)));
        return -1;
    }

    /* pt_image_add_cached(): add a file section from an *image section cache to an
     * image.
     */
    errcode = pt_image_add_cached(image, iscache, isid, NULL);
    if (errcode < 0) {
        fprintf(stderr, "%s: failed to add %s at 0x%" PRIx64 " to image: %s.\n", prog,
                arg, base, pt_errstr(pt_errcode(errcode)));
        return -1;
    }

    return 0;
}

static int
load_pt_file(const char *prog IN, char *ptfile IN, struct pt_config *config OUT)
{
    FILE *file;
    uint8_t *buffer;
    size_t size;
    int readstatus;
    int errcode;

    errno = 0;
    file = fopen(ptfile, "rb");
    if (file == NULL) {
        fprintf(stderr, "%s: failed to open %s: %d.\n", prog, ptfile, errno);
        return -1;
    }

    errcode = fseek(file, 0, SEEK_END);
    if (errcode != 0) {
        fprintf(stderr, "%s: failed to determine size of %s: %d.\n", prog, ptfile, errno);
        goto err_file;
    }

    size = (size_t)ftell(file);
    if (size < 0) {
        fprintf(stderr, "%s: failed to determine size of %s: %d.\n", prog, ptfile, errno);
        goto err_file;
    }

    buffer = (uint8_t *)malloc(size);
    if (buffer == NULL) {
        fprintf(stderr, "%s: failed to allocated memory %s.\n", prog, ptfile);
        goto err_file;
    }

    errcode = fseek(file, 0, SEEK_SET);
    if (errcode != 0) {
        fprintf(stderr, "%s: failed to load %s: %d.\n", prog, ptfile, errno);
        goto err_buffer;
    }

    readstatus = fread(buffer, size, 1u, file);
    if (readstatus != 1) {
        fprintf(stderr, "%s: failed to load %s: %d.\n", prog, ptfile, errno);
        goto err_buffer;
    }

    fclose(file);

    config->begin = buffer;
    config->end = buffer + size;

    return 0;

err_buffer:
    free(buffer);
err_file:
    fclose(file);
    return -1;
}

static int
alloc_decoder(struct pt_config *conf IN, const char *prog IN,
              dript_decoder_t *decoder INOUT, struct pt_image *image INOUT)
{
    struct pt_config config;
    int errcode;

    config = *conf;
    decoder->ptdecoder = pt_insn_alloc_decoder(&config);
    if (decoder->ptdecoder == NULL) {
        fprintf(stderr, "%s: failed to create libipt decoder.\n", prog);
        return -pte_nomem;
    }

    errcode = pt_insn_set_image(decoder->ptdecoder, image);
    if (errcode < 0) {
        fprintf(stderr, "%s: failed to set image.\n", prog);
        return errcode;
    }

    return 0;
}

static int
alloc_sb_pevent_decoder(char *filename IN, const char *prog IN,
                        dript_decoder_t *decoder INOUT)
{
    struct pt_sb_pevent_config config;
    int errcode;

    config = decoder->sbpevent;
    config.filename = filename;
    config.begin = (size_t)0;
    config.end = 0;

    errcode = pt_sb_alloc_pevent_decoder(decoder->sbsession, &config);
    if (errcode < 0) {
        fprintf(stderr, "%s: error loading %s: %s.\n", prog, filename,
                pt_errstr(pt_errcode(errcode)));
        return -1;
    }

    return 0;
}

static int
process_args(int argc IN, char *argv[] IN, struct pt_config *config OUT,
             dript_decoder_t *decoder OUT, struct pt_image *image OUT,
             dript_options_t *options OUT)
{
    int argidx = 1;
    char *prog = argv[0];

    while (argidx < argc) {
        if (strcmp(argv[argidx], "--help") == 0 || strcmp(argv[argidx], "-h") == 0) {
            return usage(prog);
        } else if (strcmp(argv[argidx], "--stats") == 0) {
            options->print_stats = 1;
        } else if (strcmp(argv[argidx], "--pt") == 0) {
            if (argc <= ++argidx) {
                fprintf(stderr, "%s: --pt: missing argument.\n", prog);
                return -1;
            }
            if (config->cpu.vendor) {
                int errcode = pt_cpu_errata(&config->errata, &config->cpu);
                if (errcode < 0) {
                    fprintf(stderr, "%s: --pt: [0, 0: config error: %s]\n", prog,
                            pt_errstr(pt_errcode(errcode)));
                }
            }
            char *ptfile = argv[argidx];
            int errcode = load_pt_file(prog, ptfile, config);
            if (errcode < 0) {
                return errcode;
            }

            errcode = alloc_decoder(config, prog, decoder, image);
            if (errcode < 0) {
                return errcode;
            }
        } else if (strcmp(argv[argidx], "--img") == 0) {
            if (argc <= ++argidx) {
                fprintf(stderr, "%s: --img: missing argument.\n", prog);
                return -1;
            }
            int errcode = load_image(argv[argidx], prog, image, decoder->iscache);
            if (errcode < 0) {
                return errcode;
            }
        } else if (strcmp(argv[argidx], "--pevent:sample-type") == 0) {
            if (argc <= ++argidx) {
                fprintf(stderr, "%s: --pevent:sample-type: missing argument.\n", prog);
                return -1;
            }
            uint64_t sample_type;
            if (sscanf(argv[argidx], "%x", &sample_type) != 1) {
                fprintf(stderr, "%s: --pevent:sample-type: bad argument: %s.\n", prog,
                        argv[argidx]);
                return -1;
            }
            decoder->sbpevent.sample_type = sample_type;
        } else if (strcmp(argv[argidx], "--pevent:primary") == 0) {
            if (argc <= ++argidx) {
                fprintf(stderr,
                        "%s: --pevent:primary: "
                        "missing argument.\n",
                        prog);
                return -1;
            }
            char *primary_file = argv[argidx];
            decoder->sbpevent.primary = 1;
            int errcode = alloc_sb_pevent_decoder(primary_file, prog, decoder);
            if (errcode < 0) {
                return -1;
            }
        } else if (strcmp(argv[argidx], "--pevent:secondary") == 0) {
            if (argc <= ++argidx) {
                fprintf(stderr,
                        "%s: --pevent:secondary: "
                        "missing argument.\n",
                        prog);
                return -1;
            }
            char *secondary_file = argv[argidx];
            decoder->sbpevent.primary = 0;
            int errcode = alloc_sb_pevent_decoder(secondary_file, prog, decoder);
            if (errcode < 0) {
                return -1;
            }
        } else if (strcmp(argv[argidx], "--pevent:kernel-start") == 0) {
            if (argc <= ++argidx) {
                fprintf(stderr, "%s: --pevent:kernel-start: missing argument.\n", prog);
                return -1;
            }
            uint64_t kernel_start;
            if (sscanf(argv[argidx], "%x", &kernel_start) != 1) {
                fprintf(stderr, "%s: --pevent:kernel-start: bad argument: %s.\n", prog,
                        argv[argidx]);
                return -1;
            }
            decoder->sbpevent.kernel_start = kernel_start;
        } else if (strcmp(argv[argidx], "--pevent:kcore") == 0) {
            if (argc <= ++argidx) {
                fprintf(stderr,
                        "%s: --pevent:kcore: "
                        "missing argument.\n",
                        prog);
                return -1;
            }
            struct pt_image *kernel;
            int errcode;
            kernel = pt_sb_kernel_image(decoder->sbsession);
            errcode = load_elf(decoder->iscache, kernel, argv[argidx], 0, prog, 0);
            if (errcode < 0)
                return -1;
        } else if (strcmp(argv[argidx], "--cpu") == 0) {
            if (argc <= ++argidx) {
                fprintf(stderr,
                        "%s: --cpu: "
                        "missing argument.\n",
                        prog);
                return -1;
            }
            if (decoder->ptdecoder != NULL) {
                fprintf(stderr, "%s: please specify cpu before the pt source file.\n",
                        prog);
                return -1;
            }

            if (strcmp(argv[argidx], "none") == 0) {
                memset(&(config->cpu), 0, sizeof(config->cpu));
            } else {
                int errcode = pt_cpu_parse(&(config->cpu), argv[argidx]);
                if (errcode < 0) {
                    fprintf(stderr, "%s: cpu must be specified as f/m[/s]\n", prog);
                    return -1;
                }
            }
        } else {
            fprintf(stderr, "%s: unknown option: %s.\n", prog, argv[argidx]);
            return -1;
        }
        argidx++;
    }

    return 0;
}

static int
init_dript_decoder(dript_decoder_t *decoder INOUT)
{
    memset(decoder, 0, sizeof(*decoder));

    decoder->iscache = pt_iscache_alloc(NULL);
    if (decoder->iscache == NULL)
        return -pte_nomem;

    decoder->sbsession = pt_sb_alloc(decoder->iscache);
    if (decoder->sbsession == NULL) {
        pt_iscache_free(decoder->iscache);
        return -pte_nomem;
    }

    memset(&decoder->sbpevent, 0, sizeof(decoder->sbpevent));
    decoder->sbpevent.size = sizeof(decoder->sbpevent);
    decoder->sbpevent.kernel_start = UINT64_MAX;
    decoder->sbpevent.time_mult = 1;

    return 0;
}

static void
free_dript_decoder(dript_decoder_t *decoder INOUT)
{
    if (decoder == NULL)
        return;

    pt_insn_free_decoder(decoder->ptdecoder);
    pt_sb_free(decoder->sbsession);
    pt_iscache_free(decoder->iscache);
}

int
main(int argc IN, char **argv IN)
{
    dript_options_t options;
    dript_decoder_t decoder;
    dript_output_t output;

    struct pt_config config;
    struct pt_image *image = NULL;

    const char *prog;
    int errcode;

    prog = argv[0];

    memset(&options, 0, sizeof(options));
    memset(&output, 0, sizeof(output));

    /* Initialize the pt_config, dript_decoder, and pt_image structures. */
    pt_config_init(&config);
    errcode = init_dript_decoder(&decoder);
    if (errcode < 0) {
        fprintf(stderr, "%s: error initializing decoder: %s.\n", prog,
                pt_errstr(pt_errcode(errcode)));
        errcode = -1;
        goto out;
    }
    image = pt_image_alloc(NULL);
    if (image == NULL) {
        fprintf(stderr, "%s: failed to allocate image.\n", prog);
        errcode = -1;
        goto out;
    }

    /* Parse the command line.*/
    errcode = process_args(argc, argv, &config, &decoder, image, &options);
    if (errcode != 0) {
        if (errcode > 0)
            errcode = 0;
        goto out;
    }

    /* Ensure that the libipt instruction decoder is initialized. */
    if (decoder.ptdecoder == NULL) {
        fprintf(stderr, "%s: no pt file.\n", prog);
        errcode = -1;
        goto out;
    }

    /* Initialize the sideband session. It needs be called after all sideband decoders are
     * being initialized.
     */
    errcode = pt_sb_init_decoders(decoder.sbsession);
    if (errcode < 0) {
        fprintf(stderr, "%s: error initializing sideband decoders: %s.\n", prog,
                pt_errstr(pt_errcode(errcode)));
        goto out;
    }

    process_decode(&decoder, &options, &output);

    if (options.print_stats == 1)
        print_stats(&output);

out:
    free_dript_decoder(&decoder);
    pt_image_free(image);
    free(config.begin);
    return -errcode;
}
