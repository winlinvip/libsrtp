/*
 * srtp_driver.c
 *
 * a test driver for libSRTP
 *
 * David A. McGrew
 * Cisco Systems, Inc.
 */
/*
 *
 * Copyright (c) 2001-2017, Cisco Systems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 *
 *   Neither the name of the Cisco Systems, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <string.h>   /* for memcpy()          */
#include <time.h>     /* for clock()           */
#include <stdlib.h>   /* for malloc(), free()  */
#include <stdio.h>    /* for print(), fflush() */
#include "getopt_s.h" /* for local getopt()    */

#include "srtp_priv.h"
#include "stream_list_priv.h"
#include "util.h"

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#elif defined HAVE_WINSOCK2_H
#include <winsock2.h>
#endif

#define PRINT_REFERENCE_PACKET 1

srtp_err_status_t srtp_validate(void);

srtp_err_status_t srtp_validate_mki(void);

srtp_err_status_t srtp_validate_null_sha1_80(void);

srtp_err_status_t srtp_validate_null_null(void);

srtp_err_status_t srtp_validate_cryptex(void);

#ifdef GCM
srtp_err_status_t srtp_validate_gcm(void);

srtp_err_status_t srtp_validate_gcm_cryptex(void);
#endif

srtp_err_status_t srtp_validate_encrypted_extensions_headers(void);

#ifdef GCM
srtp_err_status_t srtp_validate_encrypted_extensions_headers_gcm(void);
#endif

srtp_err_status_t srtp_validate_aes_256(void);

srtp_err_status_t srtp_create_big_policy(srtp_policy_t **list);

srtp_err_status_t srtp_dealloc_big_policy(srtp_policy_t *list);

srtp_err_status_t srtp_test_empty_payload(void);

#ifdef GCM
srtp_err_status_t srtp_test_empty_payload_gcm(void);
#endif

srtp_err_status_t srtp_test_remove_stream(void);

srtp_err_status_t srtp_test_update(void);

srtp_err_status_t srtp_test_update_mki(void);

srtp_err_status_t srtp_test_protect_trailer_length(void);

srtp_err_status_t srtp_test_protect_rtcp_trailer_length(void);

srtp_err_status_t srtp_test_out_of_order_after_rollover(void);

srtp_err_status_t srtp_test_get_roc(void);

srtp_err_status_t srtp_test_set_receiver_roc(void);

srtp_err_status_t srtp_test_roc_mismatch(void);

srtp_err_status_t srtp_test_set_sender_roc(void);

srtp_err_status_t srtp_test_cryptex_csrc_but_no_extension_header(void);

double srtp_bits_per_second(size_t msg_len_octets, const srtp_policy_t *policy);

double srtp_rejections_per_second(size_t msg_len_octets,
                                  const srtp_policy_t *policy);

void srtp_do_timing(const srtp_policy_t *policy);

void srtp_do_rejection_timing(const srtp_policy_t *policy);

srtp_err_status_t srtp_test(const srtp_policy_t *policy,
                            bool test_extension_headers,
                            bool use_mki,
                            size_t mki_index);

srtp_err_status_t srtp_test_io_lengths(const srtp_policy_t *policy,
                                       bool test_extension_headers,
                                       bool use_mki,
                                       size_t mki_index);

srtp_err_status_t srtcp_test(const srtp_policy_t *policy,
                             bool use_mki,
                             size_t mki_index);

srtp_err_status_t srtcp_test_io_lengths(const srtp_policy_t *policy,
                                        bool use_mki,
                                        size_t mki_index);

srtp_err_status_t srtp_session_print_policy(srtp_t srtp);

srtp_err_status_t srtp_print_policy(const srtp_policy_t *policy);

char *srtp_packet_to_string(uint8_t *packet, size_t packet_len);
char *srtp_rtcp_packet_to_string(uint8_t *packet, size_t pkt_octet_len);

double mips_estimate(size_t num_trials, size_t *ignore);

srtp_err_status_t srtp_stream_list_test(void);

const uint8_t rtp_test_packet_extension_header[12] = {
    /* one-byte header */
    0xbe, 0xde,
    /* size */
    0x00, 0x02,
    /* id 1, length 1 (i.e. 2 bytes) */
    0x11,
    /* payload */
    0xca, 0xfe,
    /* padding */
    0x00,
    /* id 2, length 0 (i.e. 1 byte) */
    0x20,
    /* payload */
    0xba,
    /* padding */
    0x00, 0x00
};

#define TEST_MKI_ID_SIZE 4

typedef struct test_vectors_t {
    const char *name;
    const char *plaintext;
    const char *ciphertext;
} test_vectors_t;

extern uint8_t test_key[46];
extern uint8_t test_key_2[46];
extern uint8_t test_mki_id[TEST_MKI_ID_SIZE];
extern uint8_t test_mki_id_2[TEST_MKI_ID_SIZE];
extern uint8_t test_key_gcm[28];

// clang-format off
srtp_master_key_t master_key_1 = {
    test_key,
    test_mki_id
};

srtp_master_key_t master_key_2 = {
    test_key_2,
    test_mki_id_2
};

srtp_master_key_t *test_keys[2] = {
    &master_key_1,
    &master_key_2
};
// clang-format on

bool use_srtp_not_in_place_io_api = false;

srtp_err_status_t call_srtp_protect2(srtp_ctx_t *ctx,
                                     uint8_t *rtp,
                                     size_t rtp_len,
                                     size_t *srtp_len,
                                     size_t mki_index)
{
    srtp_err_status_t status = srtp_err_status_fail;
    if (use_srtp_not_in_place_io_api) {
        uint8_t in_buf[4048];
        if (rtp_len > sizeof(in_buf)) {
            printf("rtp_len greater than in_buf");
            exit(1);
        }
        memcpy(in_buf, rtp, rtp_len);
        status = srtp_protect(ctx, in_buf, rtp_len, rtp, srtp_len, mki_index);
    } else {
        status = srtp_protect(ctx, rtp, rtp_len, rtp, srtp_len, mki_index);
    }
    return status;
}

srtp_err_status_t call_srtp_protect(srtp_ctx_t *ctx,
                                    uint8_t *rtp,
                                    size_t *rtp_len,
                                    size_t mki_index)
{
    // an assumption
    size_t srtp_len = *rtp_len + SRTP_MAX_TRAILER_LEN;
    srtp_err_status_t status =
        call_srtp_protect2(ctx, rtp, *rtp_len, &srtp_len, mki_index);
    *rtp_len = srtp_len;
    return status;
}

srtp_err_status_t call_srtp_unprotect2(srtp_ctx_t *ctx,
                                       uint8_t *srtp,
                                       size_t srtp_len,
                                       size_t *rtp_len)
{
    srtp_err_status_t status = srtp_err_status_fail;
    if (use_srtp_not_in_place_io_api) {
        uint8_t in_buf[4048];
        if (srtp_len > sizeof(in_buf)) {
            printf("srtp_len greater than in_buf");
            exit(1);
        }
        memcpy(in_buf, srtp, srtp_len);
        status = srtp_unprotect(ctx, in_buf, srtp_len, srtp, rtp_len);
    } else {
        status = srtp_unprotect(ctx, srtp, srtp_len, srtp, rtp_len);
    }
    return status;
}

srtp_err_status_t call_srtp_unprotect(srtp_ctx_t *ctx,
                                      uint8_t *srtp,
                                      size_t *srtp_len)
{
    return call_srtp_unprotect2(ctx, srtp, *srtp_len, srtp_len);
}

srtp_err_status_t call_srtp_protect_rtcp2(srtp_ctx_t *ctx,
                                          uint8_t *rtcp,
                                          size_t rtcp_len,
                                          size_t *srtcp_len,
                                          size_t mki_index)
{
    srtp_err_status_t status = srtp_err_status_fail;
    if (use_srtp_not_in_place_io_api) {
        uint8_t in_buf[4048];
        if (rtcp_len > sizeof(in_buf)) {
            printf("rtcp_len greater than in_buf");
            exit(1);
        }
        memcpy(in_buf, rtcp, rtcp_len);
        status = srtp_protect_rtcp(ctx, in_buf, rtcp_len, rtcp, srtcp_len,
                                   mki_index);
    } else {
        status =
            srtp_protect_rtcp(ctx, rtcp, rtcp_len, rtcp, srtcp_len, mki_index);
    }
    return status;
}

srtp_err_status_t call_srtp_protect_rtcp(srtp_ctx_t *ctx,
                                         uint8_t *rtcp,
                                         size_t *rtcp_len,
                                         size_t mki_index)
{
    // an assumption
    size_t srtcp_len = *rtcp_len + SRTP_MAX_SRTCP_TRAILER_LEN;
    srtp_err_status_t status =
        call_srtp_protect_rtcp2(ctx, rtcp, *rtcp_len, &srtcp_len, mki_index);
    *rtcp_len = srtcp_len;
    return status;
}

srtp_err_status_t call_srtp_unprotect_rtcp2(srtp_ctx_t *ctx,
                                            uint8_t *srtcp,
                                            size_t srtcp_len,
                                            size_t *rtcp_len)
{
    srtp_err_status_t status = srtp_err_status_fail;
    if (use_srtp_not_in_place_io_api) {
        uint8_t in_buf[4048];
        if (srtcp_len > sizeof(in_buf)) {
            printf("srtcp_len greater than in_buf");
            exit(1);
        }
        memcpy(in_buf, srtcp, srtcp_len);
        status = srtp_unprotect_rtcp(ctx, in_buf, srtcp_len, srtcp, rtcp_len);
    } else {
        status = srtp_unprotect_rtcp(ctx, srtcp, srtcp_len, srtcp, rtcp_len);
    }
    return status;
}

srtp_err_status_t call_srtp_unprotect_rtcp(srtp_ctx_t *ctx,
                                           uint8_t *srtcp,
                                           size_t *srtcp_len)
{
    return call_srtp_unprotect_rtcp2(ctx, srtcp, *srtcp_len, srtcp_len);
}

void usage(char *prog_name)
{
    printf("usage: %s [ -t ][ -c ][ -v ][ -s ][ -o ][-d <debug_module> ]* [ -l "
           "][ -n ]\n"
           "  -t         run timing test\n"
           "  -r         run rejection timing test\n"
           "  -c         run codec timing test\n"
           "  -v         run validation tests\n"
           "  -s         run stream list tests only\n"
           "  -o         output logging to stdout\n"
           "  -d <mod>   turn on debugging module <mod>\n"
           "  -l         list debugging modules\n"
           "  -n         run with not-in-place io api\n",
           prog_name);
    exit(1);
}

void log_handler(srtp_log_level_t level, const char *msg, void *data)
{
    char level_char = '?';
    (void)data;
    switch (level) {
    case srtp_log_level_error:
        level_char = 'e';
        break;
    case srtp_log_level_warning:
        level_char = 'w';
        break;
    case srtp_log_level_info:
        level_char = 'i';
        break;
    case srtp_log_level_debug:
        level_char = 'd';
        break;
    }
    printf("SRTP-LOG [%c]: %s\n", level_char, msg);
}

/*
 * The policy_array and invalid_policy_array are null-terminated arrays of
 * policy structs. They is declared at the end of this file.
 */

extern const srtp_policy_t *policy_array[];
extern const srtp_policy_t *invalid_policy_array[];

/* the wildcard_policy is declared below; it has a wildcard ssrc */

extern const srtp_policy_t wildcard_policy;

/*
 * mod_driver debug module - debugging module for this test driver
 *
 * we use the crypto_kernel debugging system in this driver, which
 * makes the interface uniform and increases portability
 */

srtp_debug_module_t mod_driver = {
    false,   /* debugging is off by default */
    "driver" /* printable name for module   */
};

int main(int argc, char *argv[])
{
    int q;
    bool do_timing_test = false;
    bool do_rejection_test = false;
    bool do_codec_timing = false;
    bool do_validation = false;
    bool do_stream_list = false;
    bool do_list_mods = false;
    bool do_log_stdout = false;
    srtp_err_status_t status;
    const size_t hdr_size = 12;

    /*
     * verify that the compiler has interpreted the header data
     * structure srtp_hdr_t correctly
     */
    if (sizeof(srtp_hdr_t) != hdr_size) {
        printf("error: srtp_hdr_t has incorrect size"
               "(size is %ld bytes, expected %ld)\n",
               (long)sizeof(srtp_hdr_t), (long)hdr_size);
        exit(1);
    }

    /* initialize srtp library */
    status = srtp_init();
    if (status) {
        printf("error: srtp init failed with error code %d\n", status);
        exit(1);
    }

    /*  load srtp_driver debug module */
    status = srtp_crypto_kernel_load_debug_module(&mod_driver);
    if (status) {
        printf("error: load of srtp_driver debug module failed "
               "with error code %d\n",
               status);
        exit(1);
    }

    /* process input arguments */
    while (1) {
        q = getopt_s(argc, argv, "trcvsold:n");
        if (q == -1) {
            break;
        }
        switch (q) {
        case 't':
            do_timing_test = true;
            break;
        case 'r':
            do_rejection_test = true;
            break;
        case 'c':
            do_codec_timing = true;
            break;
        case 'v':
            do_validation = true;
            do_stream_list = true;
            break;
        case 's':
            do_stream_list = true;
            break;
        case 'o':
            do_log_stdout = true;
            break;
        case 'l':
            do_list_mods = true;
            break;
        case 'd':
            status = srtp_set_debug_module(optarg_s, true);
            if (status) {
                printf("error: set debug module (%s) failed\n", optarg_s);
                exit(1);
            }
            break;
        case 'n':
            printf("using srtp not-in-place io api\n");
            use_srtp_not_in_place_io_api = true;
            break;
        default:
            usage(argv[0]);
        }
    }

    if (!do_validation && !do_timing_test && !do_codec_timing &&
        !do_list_mods && !do_rejection_test && !do_stream_list) {
        usage(argv[0]);
    }

    if (do_log_stdout) {
        status = srtp_install_log_handler(log_handler, NULL);
        if (status) {
            printf("error: install log handler failed\n");
            exit(1);
        }
    }

    if (do_list_mods) {
        status = srtp_list_debug_modules();
        if (status) {
            printf("error: list of debug modules failed\n");
            exit(1);
        }
    }

    if (do_validation) {
        const srtp_policy_t **policy = policy_array;
        srtp_policy_t *big_policy;
        srtp_t srtp_sender;

        /* loop over policy array, testing srtp and srtcp for each policy */
        while (*policy != NULL) {
            printf("testing srtp_protect and srtp_unprotect\n");
            if (srtp_test(*policy, false, false, 0) == srtp_err_status_ok) {
                printf("passed\n\n");
            } else {
                printf("failed\n");
                exit(1);
            }

            printf("testing srtp_protect and srtp_unprotect io lengths\n");
            if (srtp_test_io_lengths(*policy, false, false, 0) ==
                srtp_err_status_ok) {
                printf("passed\n\n");
            } else {
                printf("failed\n");
                exit(1);
            }

            printf("testing srtp_protect and srtp_unprotect with encrypted "
                   "extensions headers\n");
            if (srtp_test(*policy, true, false, 0) == srtp_err_status_ok) {
                printf("passed\n\n");
            } else {
                printf("failed\n");
                exit(1);
            }

            printf("testing srtp_protect and srtp_unprotect io lengths with "
                   "encrypted extension headers\n");
            if (srtp_test_io_lengths(*policy, true, false, 0) ==
                srtp_err_status_ok) {
                printf("passed\n\n");
            } else {
                printf("failed\n");
                exit(1);
            }

            printf("testing srtp_protect_rtcp and srtp_unprotect_rtcp\n");
            if (srtcp_test(*policy, false, 0) == srtp_err_status_ok) {
                printf("passed\n\n");
            } else {
                printf("failed\n");
                exit(1);
            }

            printf("testing srtp_protect_rtcp and srtp_unprotect_rtcp io "
                   "lengths\n");
            if (srtcp_test_io_lengths(*policy, false, 0) ==
                srtp_err_status_ok) {
                printf("passed\n\n");
            } else {
                printf("failed\n");
                exit(1);
            }

            printf("testing srtp_protect_rtp and srtp_unprotect_rtp with MKI "
                   "index set to 0\n");
            if (srtp_test(*policy, false, true, 0) == srtp_err_status_ok) {
                printf("passed\n\n");
            } else {
                printf("failed\n");
                exit(1);
            }
            printf("testing srtp_protect_rtp and srtp_unprotect_rtp with MKI "
                   "index set to 1\n");
            if (srtp_test(*policy, false, true, 1) == srtp_err_status_ok) {
                printf("passed\n\n");
            } else {
                printf("failed\n");
                exit(1);
            }

            printf("testing srtp_protect and srtp_unprotect io lengths with "
                   "MKI\n");
            if (srtp_test_io_lengths(*policy, false, true, 1) ==
                srtp_err_status_ok) {
                printf("passed\n\n");
            } else {
                printf("failed\n");
                exit(1);
            }

            printf("testing srtp_protect_rtcp and srtp_unprotect_rtcp with MKI "
                   "index set to 0\n");
            if (srtcp_test(*policy, true, 0) == srtp_err_status_ok) {
                printf("passed\n\n");
            } else {
                printf("failed\n");
                exit(1);
            }

            printf("testing srtp_protect_rtcp and srtp_unprotect_rtcp with MKI "
                   "index set to 1\n");
            if (srtcp_test(*policy, true, 1) == srtp_err_status_ok) {
                printf("passed\n\n");
            } else {
                printf("failed\n");
                exit(1);
            }

            printf("testing srtp_protect_rtcp and srtp_unprotect_rtcp io "
                   "lengths with MKI\n");
            if (srtcp_test_io_lengths(*policy, true, 1) == srtp_err_status_ok) {
                printf("passed\n\n");
            } else {
                printf("failed\n");
                exit(1);
            }

            policy++;
        }

        /* loop over invalid policy array, testing that an SRTP context cannot
         * be created with the policy */
        policy = invalid_policy_array;
        while (*policy != NULL) {
            printf("testing srtp_create fails with invalid policy\n");
            if (srtp_create(&srtp_sender, *policy) != srtp_err_status_ok) {
                printf("passed\n\n");
            } else {
                printf("failed\n");
                exit(1);
            }

            policy++;
        }

        /* create a big policy list and run tests on it */
        status = srtp_create_big_policy(&big_policy);
        if (status) {
            printf("unexpected failure with error code %d\n", status);
            exit(1);
        }
        printf("testing srtp_protect and srtp_unprotect with big policy\n");
        if (srtp_test(big_policy, false, false, 0) == srtp_err_status_ok) {
            printf("passed\n\n");
        } else {
            printf("failed\n");
            exit(1);
        }
        printf("testing srtp_protect and srtp_unprotect with big policy and "
               "encrypted extensions headers\n");
        if (srtp_test(big_policy, true, false, 0) == srtp_err_status_ok) {
            printf("passed\n\n");
        } else {
            printf("failed\n");
            exit(1);
        }
        status = srtp_dealloc_big_policy(big_policy);
        if (status) {
            printf("unexpected failure with error code %d\n", status);
            exit(1);
        }

        /* run test on wildcard policy */
        printf("testing srtp_protect and srtp_unprotect on "
               "wildcard ssrc policy\n");
        if (srtp_test(&wildcard_policy, false, false, 0) ==
            srtp_err_status_ok) {
            printf("passed\n\n");
        } else {
            printf("failed\n");
            exit(1);
        }
        printf("testing srtp_protect and srtp_unprotect on "
               "wildcard ssrc policy and encrypted extensions headers\n");
        if (srtp_test(&wildcard_policy, true, false, 0) == srtp_err_status_ok) {
            printf("passed\n\n");
        } else {
            printf("failed\n");
            exit(1);
        }

        /*
         * run validation test against the reference packets - note
         * that this test only covers the default policy
         */
        printf("testing srtp_protect and srtp_unprotect against "
               "reference packet\n");
        if (srtp_validate() == srtp_err_status_ok) {
            printf("passed\n\n");
        } else {
            printf("failed\n");
            exit(1);
        }

        /*
         * run validation test against the reference packets - note
         * that this test only covers the default policy
         */
        printf("testing srtp_protect and srtp_unprotect against "
               "reference packet using mki\n");
        if (srtp_validate_mki() == srtp_err_status_ok) {
            printf("passed\n\n");
        } else {
            printf("failed\n");
            exit(1);
        }

        printf("testing srtp_protect and srtp_unprotect against "
               "reference packet using null cipher and SHA1-80 HMAC\n");
        if (srtp_validate_null_sha1_80() == srtp_err_status_ok) {
            printf("passed\n\n");
        } else {
            printf("failed\n");
            exit(1);
        }

        printf("testing srtp_protect and srtp_unprotect against "
               "reference packet using null cipher and null HMAC\n");
        if (srtp_validate_null_null() == srtp_err_status_ok) {
            printf("passed\n\n");
        } else {
            printf("failed\n");
            exit(1);
        }

        printf("testing srtp_protect and srtp_unprotect against "
               "reference cryptex packet\n");
        if (srtp_validate_cryptex() == srtp_err_status_ok) {
            printf("passed\n\n");
        } else {
            printf("failed\n");
            exit(1);
        }

#ifdef GCM
        printf("testing srtp_protect and srtp_unprotect against "
               "reference packet using GCM\n");
        if (srtp_validate_gcm() == srtp_err_status_ok) {
            printf("passed\n\n");
        } else {
            printf("failed\n");
            exit(1);
        }

        printf("testing srtp_protect and srtp_unprotect against "
               "reference cryptex packet using GCM\n");
        if (srtp_validate_gcm_cryptex() == srtp_err_status_ok) {
            printf("passed\n\n");
        } else {
            printf("failed\n");
            exit(1);
        }
#endif

        printf("testing srtp_protect and srtp_unprotect against "
               "reference packet with encrypted extensions headers\n");
        if (srtp_validate_encrypted_extensions_headers() ==
            srtp_err_status_ok) {
            printf("passed\n\n");
        } else {
            printf("failed\n");
            exit(1);
        }

#ifdef GCM
        printf("testing srtp_protect and srtp_unprotect against "
               "reference packet with encrypted extension headers (GCM)\n");
        if (srtp_validate_encrypted_extensions_headers_gcm() ==
            srtp_err_status_ok) {
            printf("passed\n\n");
        } else {
            printf("failed\n");
            exit(1);
        }
#endif

        /*
         * run validation test against the reference packets for
         * AES-256
         */
        printf("testing srtp_protect and srtp_unprotect against "
               "reference packet (AES-256)\n");
        if (srtp_validate_aes_256() == srtp_err_status_ok) {
            printf("passed\n\n");
        } else {
            printf("failed\n");
            exit(1);
        }

        /*
         * test packets with empty payload
         */
        printf("testing srtp_protect and srtp_unprotect against "
               "packet with empty payload\n");
        if (srtp_test_empty_payload() == srtp_err_status_ok) {
            printf("passed\n");
        } else {
            printf("failed\n");
            exit(1);
        }
#ifdef GCM
        printf("testing srtp_protect and srtp_unprotect against "
               "packet with empty payload (GCM)\n");
        if (srtp_test_empty_payload_gcm() == srtp_err_status_ok) {
            printf("passed\n");
        } else {
            printf("failed\n");
            exit(1);
        }
#endif

        /*
         * test the function srtp_stream_remove()
         */
        printf("testing srtp_stream_remove()...");
        if (srtp_test_remove_stream() == srtp_err_status_ok) {
            printf("passed\n");
        } else {
            printf("failed\n");
            exit(1);
        }

        /*
         * test the function srtp_update()
         */
        printf("testing srtp_update()...");
        if (srtp_test_update() == srtp_err_status_ok) {
            printf("passed\n");
        } else {
            printf("failed\n");
            exit(1);
        }

        /*
         * test the function srtp_update()
         */
        printf("testing srtp_update_mki()...");
        if (srtp_test_update_mki() == srtp_err_status_ok) {
            printf("passed\n");
        } else {
            printf("failed\n");
            exit(1);
        }

        /*
         * test the functions srtp_get_protect_trailer_length
         * and srtp_get_protect_rtcp_trailer_length
         */
        printf("testing srtp_get_protect_trailer_length()...");
        if (srtp_test_protect_trailer_length() == srtp_err_status_ok) {
            printf("passed\n");
        } else {
            printf("failed\n");
            exit(1);
        }

        printf("testing srtp_get_protect_rtcp_trailer_length()...");
        if (srtp_test_protect_rtcp_trailer_length() == srtp_err_status_ok) {
            printf("passed\n");
        } else {
            printf("failed\n");
            exit(1);
        }

        printf("testing srtp_test_out_of_order_after_rollover()...");
        if (srtp_test_out_of_order_after_rollover() == srtp_err_status_ok) {
            printf("passed\n");
        } else {
            printf("failed\n");
            exit(1);
        }

        printf("testing srtp_test_get_roc()...");
        if (srtp_test_get_roc() == srtp_err_status_ok) {
            printf("passed\n");
        } else {
            printf("failed\n");
            exit(1);
        }

        printf("testing srtp_test_set_receiver_roc()...");
        if (srtp_test_set_receiver_roc() == srtp_err_status_ok) {
            printf("passed\n");
        } else {
            printf("failed\n");
            exit(1);
        }

        printf("testing srtp_test_roc_mismatch()...");
        if (srtp_test_roc_mismatch() == srtp_err_status_ok) {
            printf("passed\n");
        } else {
            printf("failed\n");
            exit(1);
        }

        printf("testing srtp_test_set_sender_roc()...");
        if (srtp_test_set_sender_roc() == srtp_err_status_ok) {
            printf("passed\n");
        } else {
            printf("failed\n");
            exit(1);
        }

        printf("testing cryptex_csrc_but_no_extension_header()...");
        if (srtp_test_cryptex_csrc_but_no_extension_header() ==
            srtp_err_status_ok) {
            printf("passed\n");
        } else {
            printf("failed\n");
            exit(1);
        }
    }

    if (do_stream_list) {
        printf("testing srtp_stream_list...");
        if (srtp_stream_list_test() == srtp_err_status_ok) {
            printf("passed\n");
        } else {
            printf("failed\n");
            exit(1);
        }
    }

    if (do_timing_test) {
        const srtp_policy_t **policy = policy_array;

        /* loop over policies, run timing test for each */
        while (*policy != NULL) {
            srtp_print_policy(*policy);
            srtp_do_timing(*policy);
            policy++;
        }
    }

    if (do_rejection_test) {
        const srtp_policy_t **policy = policy_array;

        /* loop over policies, run rejection timing test for each */
        while (*policy != NULL) {
            srtp_print_policy(*policy);
            srtp_do_rejection_timing(*policy);
            policy++;
        }
    }

    if (do_codec_timing) {
        srtp_policy_t policy;
        size_t ignore;
        double mips_value = mips_estimate(1000000000, &ignore);

        memset(&policy, 0, sizeof(policy));
        srtp_crypto_policy_set_rtp_default(&policy.rtp);
        srtp_crypto_policy_set_rtcp_default(&policy.rtcp);
        policy.ssrc.type = ssrc_specific;
        policy.ssrc.value = 0xdecafbad;
        policy.key = test_key;
        policy.window_size = 128;
        policy.allow_repeat_tx = false;
        policy.next = NULL;

        printf("mips estimate: %e\n", mips_value);

        printf("testing srtp processing time for voice codecs:\n");
        printf("codec\t\tlength (octets)\t\tsrtp instructions/second\n");
        printf("G.711\t\t%d\t\t\t%e\n", 80,
               (double)mips_value * (80 * 8) /
                   srtp_bits_per_second(80, &policy) / .01);
        printf("G.711\t\t%d\t\t\t%e\n", 160,
               (double)mips_value * (160 * 8) /
                   srtp_bits_per_second(160, &policy) / .02);
        printf("G.726-32\t%d\t\t\t%e\n", 40,
               (double)mips_value * (40 * 8) /
                   srtp_bits_per_second(40, &policy) / .01);
        printf("G.726-32\t%d\t\t\t%e\n", 80,
               (double)mips_value * (80 * 8) /
                   srtp_bits_per_second(80, &policy) / .02);
        printf("G.729\t\t%d\t\t\t%e\n", 10,
               (double)mips_value * (10 * 8) /
                   srtp_bits_per_second(10, &policy) / .01);
        printf("G.729\t\t%d\t\t\t%e\n", 20,
               (double)mips_value * (20 * 8) /
                   srtp_bits_per_second(20, &policy) / .02);
        printf("Wideband\t%d\t\t\t%e\n", 320,
               (double)mips_value * (320 * 8) /
                   srtp_bits_per_second(320, &policy) / .01);
        printf("Wideband\t%d\t\t\t%e\n", 640,
               (double)mips_value * (640 * 8) /
                   srtp_bits_per_second(640, &policy) / .02);
    }

    status = srtp_shutdown();
    if (status) {
        printf("error: srtp shutdown failed with error code %d\n", status);
        exit(1);
    }

    return 0;
}

/*
 * create_rtp_test_packet returns a pointer to a (malloced) example
 * RTP packet whose data field has the length given by payload_len.
 * There is room at the end of the packet for the max SRTP trailer.
 * The space for the trailer space and the following four bytes
 * are filled with overrun marker to enable testing for overwrites.
 *
 * The size of the packet is returned rtp_len, the complete size of
 * the buffer is returned in buffer_len.
 *
 * note that the location of the test packet must be
 * deallocated with the free() call once it is no longer needed.
 */
uint8_t *create_rtp_test_packet(size_t payload_len,
                                uint32_t ssrc,
                                uint16_t seq,
                                uint32_t ts,
                                bool add_hdr_xtn,
                                size_t *rtp_len,
                                size_t *buffer_len)
{
    uint8_t *buffer;
    srtp_hdr_t *hdr;
    size_t bytes_in_hdr = 12;
    size_t len = 0;

    *rtp_len = payload_len + bytes_in_hdr;

    if (add_hdr_xtn) {
        *rtp_len += sizeof(rtp_test_packet_extension_header);
    }

    // allocate enough for max trailer and 4 byte overrun detection
    len = *rtp_len + SRTP_MAX_TRAILER_LEN + 4;

    buffer = (uint8_t *)malloc(len);
    if (!buffer) {
        printf("rtp test packet allocation failed\n");
        exit(1);
    }

    overrun_check_prepare(buffer, 0, len);

    hdr = (srtp_hdr_t *)buffer;
    hdr->version = 2;
    hdr->p = 0;
    hdr->x = add_hdr_xtn ? 1 : 0;
    hdr->cc = 0;
    hdr->m = 0;
    hdr->pt = 0xf;
    hdr->seq = htons(seq);
    hdr->ts = htonl(ts);
    hdr->ssrc = htonl(ssrc);
    buffer += bytes_in_hdr;

    if (add_hdr_xtn) {
        memcpy(buffer, rtp_test_packet_extension_header,
               sizeof(rtp_test_packet_extension_header));
        buffer += sizeof(rtp_test_packet_extension_header);
    }

    /* set RTP data to 0xab */
    memset(buffer, 0xab, payload_len);
    buffer += payload_len;

    if (buffer_len) {
        *buffer_len = len;
    }

    return buffer - *rtp_len;
}

uint8_t *create_rtcp_test_packet(size_t payload_len,
                                 uint32_t ssrc,
                                 size_t *rtcp_len,
                                 size_t *buffer_len)
{
    uint8_t *buffer;
    srtcp_hdr_t *hdr;
    size_t bytes_in_hdr = 8;
    size_t len = 0;

    *rtcp_len = payload_len + bytes_in_hdr;

    // allocate enough for max trailer and 4 byte overrun detection
    len = *rtcp_len + SRTP_MAX_SRTCP_TRAILER_LEN + 4;

    buffer = (uint8_t *)malloc(len);
    if (!buffer) {
        printf("rtcp test packet allocation failed\n");
        exit(1);
    }

    overrun_check_prepare(buffer, 0, len);

    hdr = (srtcp_hdr_t *)buffer;
    hdr->version = 2; /* RTP version two     */
    hdr->p = 0;       /* no padding needed   */
    hdr->rc = 0;      /* no reports          */
    hdr->pt = 0xc8;   /* sender report (200) */
    hdr->len = ((bytes_in_hdr + payload_len) % 4) - 1;
    hdr->ssrc = htonl(ssrc); /* synch. source       */
    buffer += bytes_in_hdr;

    /* set data to 0xab */
    memset(buffer, 0xab, payload_len);
    buffer += payload_len;

    if (buffer_len) {
        *buffer_len = len;
    }

    return buffer - *rtcp_len;
}

void srtp_do_timing(const srtp_policy_t *policy)
{
    int len;

    /*
     * note: the output of this function is formatted so that it
     * can be used in gnuplot.  '#' indicates a comment, and "\r\n"
     * terminates a record
     */

    printf("# testing srtp throughput:\r\n");
    printf("# mesg length (octets)\tthroughput (megabits per second)\r\n");

    for (len = 16; len <= 2048; len *= 2) {
        printf("%d\t\t\t%f\r\n", len,
               srtp_bits_per_second(len, policy) / 1.0E6);
    }

    /* these extra linefeeds let gnuplot know that a dataset is done */
    printf("\r\n\r\n");
}

void srtp_do_rejection_timing(const srtp_policy_t *policy)
{
    int len;

    /*
     * note: the output of this function is formatted so that it
     * can be used in gnuplot.  '#' indicates a comment, and "\r\n"
     * terminates a record
     */

    printf("# testing srtp rejection throughput:\r\n");
    printf("# mesg length (octets)\trejections per second\r\n");

    for (len = 8; len <= 2048; len *= 2) {
        printf("%d\t\t\t%e\r\n", len, srtp_rejections_per_second(len, policy));
    }

    /* these extra linefeeds let gnuplot know that a dataset is done */
    printf("\r\n\r\n");
}

#define MAX_MSG_LEN 1024

double srtp_bits_per_second(size_t msg_len_octets, const srtp_policy_t *policy)
{
    srtp_t srtp;
    uint8_t *mesg;
    clock_t timer;
    size_t num_trials = 100000;
    size_t input_len, len;
    uint32_t ssrc;
    srtp_err_status_t status;

    /*
     * allocate and initialize an srtp session
     */
    status = srtp_create(&srtp, policy);
    if (status) {
        printf("error: srtp_create() failed with error code %d\n", status);
        exit(1);
    }

    /*
     * if the ssrc is unspecified, use a predetermined one
     */
    if (policy->ssrc.type != ssrc_specific) {
        ssrc = 0xdeadbeef;
    } else {
        ssrc = policy->ssrc.value;
    }

    /*
     * create a test packet
     */
    mesg = create_rtp_test_packet(msg_len_octets, ssrc, 1, 1, false, &input_len,
                                  NULL);
    if (mesg == NULL) {
        return 0.0; /* indicate failure by returning zero */
    }
    timer = clock();
    for (size_t i = 0; i < num_trials; i++) {
        len = input_len;
        /* srtp protect message */
        status = call_srtp_protect(srtp, mesg, &len, 0);
        if (status) {
            printf("error: srtp_protect() failed with error code %d\n", status);
            exit(1);
        }

        /* increment message number */
        {
            /* hack sequence to avoid problems with macros for htons/ntohs on
             * some systems */
            srtp_hdr_t *hdr = (srtp_hdr_t *)mesg;
            short new_seq = ntohs(hdr->seq) + 1;
            hdr->seq = htons(new_seq);
        }
    }
    timer = clock() - timer;

    free(mesg);

    status = srtp_dealloc(srtp);
    if (status) {
        printf("error: srtp_dealloc() failed with error code %d\n", status);
        exit(1);
    }

    return msg_len_octets * 8.0 * num_trials * CLOCKS_PER_SEC / timer;
}

double srtp_rejections_per_second(size_t msg_len_octets,
                                  const srtp_policy_t *policy)
{
    srtp_ctx_t *srtp;
    uint8_t *mesg;
    size_t len;
    clock_t timer;
    size_t num_trials = 1000000;
    uint32_t ssrc = policy->ssrc.value;
    srtp_err_status_t status;

    /*
     * allocate and initialize an srtp session
     */
    status = srtp_create(&srtp, policy);
    if (status) {
        printf("error: srtp_create() failed with error code %d\n", status);
        exit(1);
    }

    mesg =
        create_rtp_test_packet(msg_len_octets, ssrc, 1, 1, false, &len, NULL);
    if (mesg == NULL) {
        return 0.0; /* indicate failure by returning zero */
    }
    call_srtp_protect(srtp, mesg, &len, 0);

    timer = clock();
    for (size_t i = 0; i < num_trials; i++) {
        len = msg_len_octets;
        call_srtp_unprotect(srtp, mesg, &len);
    }
    timer = clock() - timer;

    free(mesg);

    status = srtp_dealloc(srtp);
    if (status) {
        printf("error: srtp_dealloc() failed with error code %d\n", status);
        exit(1);
    }

    return (double)num_trials * CLOCKS_PER_SEC / timer;
}

srtp_err_status_t srtp_test(const srtp_policy_t *policy,
                            bool test_extension_headers,
                            bool use_mki,
                            size_t mki_index)
{
    size_t i;
    srtp_t srtp_sender;
    srtp_t srtp_rcvr;
    srtp_err_status_t status = srtp_err_status_ok;
    uint8_t *hdr, *hdr2;
    uint8_t hdr_enc[64];
    size_t msg_len_octets, msg_len_enc, msg_len;
    size_t len, len2;
    size_t tag_length;
    size_t buffer_len;
    uint32_t ssrc;
    srtp_policy_t send_policy;
    srtp_policy_t rcvr_policy;
    uint8_t header = 1;

    memcpy(&send_policy, policy, sizeof(srtp_policy_t));

    send_policy.use_mki = use_mki;
    if (!use_mki) {
        send_policy.mki_size = 0;
    }

    if (test_extension_headers) {
        send_policy.enc_xtn_hdr = &header;
        send_policy.enc_xtn_hdr_count = 1;
    }

    CHECK_OK(srtp_create(&srtp_sender, &send_policy));

    /* print out policy */
    CHECK_OK(srtp_session_print_policy(srtp_sender));

    /*
     * initialize data buffer, using the ssrc in the policy unless that
     * value is a wildcard, in which case we'll just use an arbitrary
     * one
     */
    if (policy->ssrc.type != ssrc_specific) {
        ssrc = 0xdecafbad;
    } else {
        ssrc = policy->ssrc.value;
    }
    msg_len_octets = 28;
    hdr = create_rtp_test_packet(msg_len_octets, ssrc, 1, 1,
                                 test_extension_headers, &len, &buffer_len);
    hdr2 = create_rtp_test_packet(msg_len_octets, ssrc, 1, 1,
                                  test_extension_headers, &len2, NULL);

    /* save original msg len */
    msg_len = len;

    if (hdr == NULL) {
        free(hdr2);
        return srtp_err_status_alloc_fail;
    }
    if (hdr2 == NULL) {
        free(hdr);
        return srtp_err_status_alloc_fail;
    }

    debug_print(mod_driver, "before protection:\n%s",
                srtp_packet_to_string(hdr, len));

#if PRINT_REFERENCE_PACKET
    debug_print(mod_driver, "reference packet before protection:\n%s",
                octet_string_hex_string(hdr, len));
#endif
    CHECK_OK(call_srtp_protect(srtp_sender, hdr, &len, mki_index));

    debug_print(mod_driver, "after protection:\n%s",
                srtp_packet_to_string(hdr, len));
#if PRINT_REFERENCE_PACKET
    debug_print(mod_driver, "after protection:\n%s",
                octet_string_hex_string(hdr, len));
#endif

    /* save protected message and length */
    memcpy(hdr_enc, hdr, len);
    msg_len_enc = len;

    /*
     * check for overrun of the srtp_protect() function
     *
     * The packet is followed by a value of 0xffffffff; if the value of the
     * data following the packet is different, then we know that the
     * protect function is overwriting the end of the packet.
     */
    CHECK_OK(
        srtp_get_protect_trailer_length(srtp_sender, mki_index, &tag_length));
    CHECK_OVERRUN(hdr, msg_len + tag_length, buffer_len);

    /*
     * if the policy includes confidentiality, check that ciphertext is
     * different than plaintext
     *
     * Note that this check will give false negatives, with some small
     * probability, especially if the packets are short.  For that
     * reason, we skip this check if the plaintext is less than four
     * octets long.
     */
    if ((policy->rtp.sec_serv & sec_serv_conf) && (msg_len_octets >= 4)) {
        printf("testing that ciphertext is distinct from plaintext...");
        status = srtp_err_status_algo_fail;
        for (i = 12; i < msg_len_octets + 12; i++) {
            if (hdr[i] != hdr2[i]) {
                status = srtp_err_status_ok;
            }
        }
        if (status) {
            printf("failed\n");
            free(hdr);
            free(hdr2);
            return status;
        }
        printf("passed\n");
    }

    memcpy(&rcvr_policy, &send_policy, sizeof(srtp_policy_t));
    rcvr_policy.ssrc.type = ssrc_any_inbound;

    CHECK_OK(srtp_create(&srtp_rcvr, &rcvr_policy));

    CHECK_OK(call_srtp_unprotect(srtp_rcvr, hdr, &len));

    debug_print(mod_driver, "after unprotection:\n%s",
                srtp_packet_to_string(hdr, len));

    /* verify that the unprotected packet matches the original one */
    for (i = 0; i < len; i++) {
        if (hdr[i] != hdr2[i]) {
            fprintf(stdout, "mismatch at octet %zu\n", i);
            status = srtp_err_status_algo_fail;
        }
    }
    if (status) {
        free(hdr);
        free(hdr2);
        return status;
    }

    /*
     * if the policy includes authentication, then test for false positives
     */
    if (policy->rtp.sec_serv & sec_serv_auth) {
        uint8_t *data = hdr + (test_extension_headers ? 24 : 12);

        printf("testing for false positives in replay check...");

        /* unprotect a second time - should fail with a replay error */
        status = call_srtp_unprotect(srtp_rcvr, hdr, &msg_len_enc);
        if (status != srtp_err_status_replay_fail) {
            printf("failed with error code %d\n", status);
            free(hdr);
            free(hdr2);
            return srtp_err_status_algo_fail;
        } else {
            printf("passed\n");
        }

        printf("testing for false positives in auth check...");

        /* increment sequence number in header */
        ((srtp_hdr_t *)hdr)->seq++;

        /* apply protection */
        CHECK_OK(call_srtp_protect(srtp_sender, hdr, &len, mki_index));

        /* flip bits in packet */
        data[0] ^= 0xff;

        /* unprotect, and check for authentication failure */
        status = call_srtp_unprotect(srtp_rcvr, hdr, &len);
        if (status != srtp_err_status_auth_fail) {
            printf("failed with error code %d\n", status);
            printf("failed\n");
            free(hdr);
            free(hdr2);
            return srtp_err_status_algo_fail;
        } else {
            printf("passed\n");
        }
    }

    CHECK_OK(srtp_dealloc(srtp_sender));
    CHECK_OK(srtp_dealloc(srtp_rcvr));

    free(hdr);
    free(hdr2);
    return srtp_err_status_ok;
}

srtp_err_status_t srtp_test_io_lengths(const srtp_policy_t *policy,
                                       bool test_extension_headers,
                                       bool use_mki,
                                       size_t mki_index)
{
    srtp_t srtp_sender;
    srtp_policy_t send_policy;
    uint32_t ssrc;
    uint16_t seq = 1;
    uint32_t ts = 1234;
    uint8_t *rtp;
    size_t rtp_len, buffer_len, srtp_len;
    size_t rtp_header_len = 12;
    uint8_t xtn_header_id = 1;

    memcpy(&send_policy, policy, sizeof(srtp_policy_t));

    send_policy.use_mki = use_mki;
    if (!use_mki) {
        send_policy.mki_size = 0;
    }

    if (test_extension_headers) {
        send_policy.enc_xtn_hdr = &xtn_header_id;
        send_policy.enc_xtn_hdr_count = 1;
        rtp_header_len += sizeof(rtp_test_packet_extension_header);
    }

    CHECK_OK(srtp_create(&srtp_sender, &send_policy));

    // get required trailer length
    size_t trailer_len;
    CHECK_OK(
        srtp_get_protect_trailer_length(srtp_sender, mki_index, &trailer_len));

    CHECK_OK(srtp_session_print_policy(srtp_sender));

    if (policy->ssrc.type != ssrc_specific) {
        ssrc = 0xdecafbad;
    } else {
        ssrc = policy->ssrc.value;
    }

    // 0 byte input
    rtp = create_rtp_test_packet(28, ssrc, seq++, ts, test_extension_headers,
                                 &rtp_len, &buffer_len);
    srtp_len = buffer_len;
    overrun_check_prepare(rtp, 0, buffer_len);
    CHECK_RETURN(call_srtp_protect2(srtp_sender, rtp, 0, &srtp_len, mki_index),
                 srtp_err_status_bad_param);
    CHECK_OVERRUN(rtp, 0, buffer_len);
    free(rtp);

    // 1 byte input
    rtp = create_rtp_test_packet(28, ssrc, seq++, ts, test_extension_headers,
                                 &rtp_len, &buffer_len);
    srtp_len = buffer_len;
    overrun_check_prepare(rtp, 1, buffer_len);
    CHECK_RETURN(call_srtp_protect2(srtp_sender, rtp, 1, &srtp_len, mki_index),
                 srtp_err_status_bad_param);
    CHECK_OVERRUN(rtp, 1, buffer_len);
    free(rtp);

    // too short header
    rtp = create_rtp_test_packet(28, ssrc, seq++, ts, test_extension_headers,
                                 &rtp_len, &buffer_len);
    srtp_len = buffer_len;
    overrun_check_prepare(rtp, rtp_header_len - 1, buffer_len);
    CHECK_RETURN(call_srtp_protect2(srtp_sender, rtp, rtp_header_len - 1,
                                    &srtp_len, mki_index),
                 srtp_err_status_bad_param);
    CHECK_OVERRUN(rtp, rtp_header_len - 1, buffer_len);
    free(rtp);

    // zero payload
    rtp = create_rtp_test_packet(28, ssrc, seq++, ts, test_extension_headers,
                                 &rtp_len, &buffer_len);
    srtp_len = buffer_len;
    overrun_check_prepare(rtp, rtp_header_len, buffer_len);
    CHECK_OK(call_srtp_protect2(srtp_sender, rtp, rtp_header_len, &srtp_len,
                                mki_index));
    CHECK(srtp_len == rtp_header_len + trailer_len);
    CHECK_OVERRUN(rtp, srtp_len, buffer_len);
    free(rtp);

    // 1 byte payload
    rtp = create_rtp_test_packet(28, ssrc, seq++, ts, test_extension_headers,
                                 &rtp_len, &buffer_len);
    srtp_len = buffer_len;
    overrun_check_prepare(rtp, rtp_header_len + 1, buffer_len);
    CHECK_OK(call_srtp_protect2(srtp_sender, rtp, rtp_header_len + 1, &srtp_len,
                                mki_index));
    CHECK(srtp_len == rtp_header_len + 1 + trailer_len);
    CHECK_OVERRUN(rtp, srtp_len, buffer_len);
    free(rtp);

    // 0 byte output
    rtp = create_rtp_test_packet(28, ssrc, seq++, ts, test_extension_headers,
                                 &rtp_len, &buffer_len);
    srtp_len = 0;
    overrun_check_prepare(rtp, rtp_len, buffer_len);
    CHECK_RETURN(
        call_srtp_protect2(srtp_sender, rtp, rtp_len, &srtp_len, mki_index),
        srtp_err_status_buffer_small);
    CHECK_OVERRUN(rtp, rtp_len, buffer_len);
    free(rtp);

    // 1 byte output
    rtp = create_rtp_test_packet(28, ssrc, seq++, ts, test_extension_headers,
                                 &rtp_len, &buffer_len);
    srtp_len = 1;
    overrun_check_prepare(rtp, rtp_len, buffer_len);
    CHECK_RETURN(
        call_srtp_protect2(srtp_sender, rtp, rtp_len, &srtp_len, mki_index),
        srtp_err_status_buffer_small);
    CHECK_OVERRUN(rtp, rtp_len, buffer_len);
    free(rtp);

    if (trailer_len != 0) {
        // no space for trailer output
        rtp = create_rtp_test_packet(
            28, ssrc, seq++, ts, test_extension_headers, &rtp_len, &buffer_len);
        srtp_len = rtp_len;
        overrun_check_prepare(rtp, rtp_len, buffer_len);
        CHECK_RETURN(
            call_srtp_protect2(srtp_sender, rtp, rtp_len, &srtp_len, mki_index),
            srtp_err_status_buffer_small);
        CHECK_OVERRUN(rtp, rtp_len, buffer_len);
        free(rtp);
    }

    // 1 byte too small output
    rtp = create_rtp_test_packet(28, ssrc, seq++, ts, test_extension_headers,
                                 &rtp_len, &buffer_len);
    srtp_len = rtp_len + trailer_len - 1;
    overrun_check_prepare(rtp, rtp_len, buffer_len);
    CHECK_RETURN(
        call_srtp_protect2(srtp_sender, rtp, rtp_len, &srtp_len, mki_index),
        srtp_err_status_buffer_small);
    CHECK_OVERRUN(rtp, rtp_len, buffer_len);
    free(rtp);

    // full payload
    rtp = create_rtp_test_packet(28, ssrc, seq++, ts, test_extension_headers,
                                 &rtp_len, &buffer_len);
    srtp_len = buffer_len;
    CHECK_OK(
        call_srtp_protect2(srtp_sender, rtp, rtp_len, &srtp_len, mki_index));
    CHECK(srtp_len == rtp_len + trailer_len);
    CHECK_OVERRUN(rtp, srtp_len, buffer_len);

    CHECK_OK(srtp_dealloc(srtp_sender));

    // unprotect
    srtp_t srtp_receiver;
    srtp_policy_t receive_policy;

    memcpy(&receive_policy, &send_policy, sizeof(srtp_policy_t));
    receive_policy.ssrc.type = ssrc_any_inbound;

    CHECK_OK(srtp_create(&srtp_receiver, &receive_policy));

    // unprotect zero byte input
    rtp_len = buffer_len;
    CHECK_RETURN(call_srtp_unprotect2(srtp_receiver, rtp, 0, &rtp_len),
                 srtp_err_status_bad_param);

    // unprotect 1 byte input
    rtp_len = buffer_len;
    CHECK_RETURN(call_srtp_unprotect2(srtp_receiver, rtp, 1, &rtp_len),
                 srtp_err_status_bad_param);

    // unprotect short header
    rtp_len = buffer_len;
    CHECK_RETURN(
        call_srtp_unprotect2(srtp_receiver, rtp, rtp_header_len - 1, &rtp_len),
        srtp_err_status_bad_param);

    // 0 byte output
    rtp_len = 0;
    CHECK_RETURN(call_srtp_unprotect2(srtp_receiver, rtp, srtp_len, &rtp_len),
                 srtp_err_status_buffer_small);

    // 1 byte output
    rtp_len = 1;
    CHECK_RETURN(call_srtp_unprotect2(srtp_receiver, rtp, srtp_len, &rtp_len),
                 srtp_err_status_buffer_small);

    // 1 byte too small output
    rtp_len = srtp_len - trailer_len - 1;
    CHECK_RETURN(call_srtp_unprotect2(srtp_receiver, rtp, srtp_len, &rtp_len),
                 srtp_err_status_buffer_small);

    // full unprotect
    rtp_len = buffer_len;
    CHECK_OK(call_srtp_unprotect2(srtp_receiver, rtp, srtp_len, &rtp_len));
    CHECK(rtp_len == srtp_len - trailer_len);

    free(rtp);

    CHECK_OK(srtp_dealloc(srtp_receiver));

    return srtp_err_status_ok;
}

srtp_err_status_t srtcp_test(const srtp_policy_t *policy,
                             bool use_mki,
                             size_t mki_index)
{
    srtp_t srtcp_sender;
    srtp_t srtcp_rcvr;
    srtp_err_status_t status = srtp_err_status_ok;
    uint8_t *hdr, *hdr2;
    uint8_t hdr_enc[64];
    size_t msg_len_octets, msg_len_enc, msg_len;
    size_t len, len2;
    size_t buffer_len;
    size_t tag_length;
    uint32_t ssrc;
    srtp_policy_t send_policy;
    srtp_policy_t rcvr_policy;

    memcpy(&send_policy, policy, sizeof(srtp_policy_t));

    send_policy.use_mki = use_mki;
    if (!use_mki) {
        send_policy.mki_size = 0;
    }

    CHECK_OK(srtp_create(&srtcp_sender, &send_policy));

    /* print out policy */
    CHECK_OK(srtp_session_print_policy(srtcp_sender));

    /*
     * initialize data buffer, using the ssrc in the policy unless that
     * value is a wildcard, in which case we'll just use an arbitrary
     * one
     */
    if (policy->ssrc.type != ssrc_specific) {
        ssrc = 0xdecafbad;
    } else {
        ssrc = policy->ssrc.value;
    }
    msg_len_octets = 28;
    hdr = create_rtcp_test_packet(msg_len_octets, ssrc, &len, &buffer_len);
    /* save message len */
    msg_len = len;

    if (hdr == NULL) {
        return srtp_err_status_alloc_fail;
    }
    hdr2 = create_rtcp_test_packet(msg_len_octets, ssrc, &len2, NULL);
    if (hdr2 == NULL) {
        free(hdr);
        return srtp_err_status_alloc_fail;
    }

    debug_print(mod_driver, "before protection:\n%s",
                srtp_rtcp_packet_to_string(hdr, len));

#if PRINT_REFERENCE_PACKET
    debug_print(mod_driver, "reference packet before protection:\n%s",
                octet_string_hex_string(hdr, len));
#endif
    CHECK_OK(call_srtp_protect_rtcp(srtcp_sender, hdr, &len, mki_index));

    debug_print(mod_driver, "after protection:\n%s",
                srtp_rtcp_packet_to_string(hdr, len));
#if PRINT_REFERENCE_PACKET
    debug_print(mod_driver, "after protection:\n%s",
                octet_string_hex_string(hdr, len));
#endif

    /* save protected message and length */
    memcpy(hdr_enc, hdr, len);
    msg_len_enc = len;

    /*
     * check for overrun of the srtp_protect_rtcp() function
     *
     * The packet is followed by a value of 0xffffffff; if the value of the
     * data following the packet is different, then we know that the
     * protect function is overwriting the end of the packet.
     */
    srtp_get_protect_rtcp_trailer_length(srtcp_sender, mki_index, &tag_length);
    CHECK_OVERRUN(hdr, msg_len + tag_length, buffer_len);

    /*
     * if the policy includes confidentiality, check that ciphertext is
     * different than plaintext
     *
     * Note that this check will give false negatives, with some small
     * probability, especially if the packets are short.  For that
     * reason, we skip this check if the plaintext is less than four
     * octets long.
     */
    if ((policy->rtcp.sec_serv & sec_serv_conf) && (msg_len_octets >= 4)) {
        printf("testing that ciphertext is distinct from plaintext...");
        status = srtp_err_status_algo_fail;
        for (size_t i = 12; i < msg_len_octets + 12; i++) {
            if (hdr[i] != hdr2[i]) {
                status = srtp_err_status_ok;
            }
        }
        if (status) {
            printf("failed\n");
            free(hdr);
            free(hdr2);
            return status;
        }
        printf("passed\n");
    }

    memcpy(&rcvr_policy, &send_policy, sizeof(srtp_policy_t));
    if (send_policy.ssrc.type == ssrc_any_outbound) {
        rcvr_policy.ssrc.type = ssrc_any_inbound;
    }

    CHECK_OK(srtp_create(&srtcp_rcvr, &rcvr_policy));

    CHECK_OK(call_srtp_unprotect_rtcp(srtcp_rcvr, hdr, &len));

    debug_print(mod_driver, "after unprotection:\n%s",
                srtp_rtcp_packet_to_string(hdr, len));

    /* verify that the unprotected packet matches the original one */
    for (size_t i = 0; i < len; i++) {
        if (hdr[i] != hdr2[i]) {
            fprintf(stdout, "mismatch at octet %zu\n", i);
            status = srtp_err_status_algo_fail;
        }
    }
    if (status) {
        free(hdr);
        free(hdr2);
        return status;
    }

    /*
     * if the policy includes authentication, then test for false positives
     */
    if (policy->rtp.sec_serv & sec_serv_auth) {
        uint8_t *data = hdr + 12;

        printf("testing for false positives in replay check...");

        /* unprotect a second time - should fail with a replay error */
        status = call_srtp_unprotect_rtcp(srtcp_rcvr, hdr, &msg_len_enc);
        if (status != srtp_err_status_replay_fail) {
            printf("failed with error code %d\n", status);
            free(hdr);
            free(hdr2);
            return srtp_err_status_algo_fail;
        } else {
            printf("passed\n");
        }

        printf("testing for false positives in auth check...");

        /* apply protection */
        CHECK_OK(call_srtp_protect_rtcp(srtcp_sender, hdr, &len, mki_index));

        /* flip bits in packet */
        data[0] ^= 0xff;

        /* unprotect, and check for authentication failure */
        status = call_srtp_unprotect_rtcp(srtcp_rcvr, hdr, &len);
        if (status != srtp_err_status_auth_fail) {
            printf("failed with error code %d\n", status);
            printf("failed\n");
            free(hdr);
            free(hdr2);
            return srtp_err_status_algo_fail;
        } else {
            printf("passed\n");
        }
    }

    CHECK_OK(srtp_dealloc(srtcp_sender));
    CHECK_OK(srtp_dealloc(srtcp_rcvr));

    free(hdr);
    free(hdr2);
    return srtp_err_status_ok;
}

srtp_err_status_t srtcp_test_io_lengths(const srtp_policy_t *policy,
                                        bool use_mki,
                                        size_t mki_index)
{
    srtp_t srtp_sender;
    srtp_policy_t send_policy;
    uint32_t ssrc;
    uint8_t *rtcp;
    size_t rtcp_len, buffer_len, srtcp_len;
    size_t rtcp_header_len = 8;

    memcpy(&send_policy, policy, sizeof(srtp_policy_t));

    send_policy.use_mki = use_mki;
    if (!use_mki) {
        send_policy.mki_size = 0;
    }

    CHECK_OK(srtp_create(&srtp_sender, &send_policy));

    // get required trailer length
    size_t trailer_len;
    CHECK_OK(srtp_get_protect_rtcp_trailer_length(srtp_sender, mki_index,
                                                  &trailer_len));

    CHECK_OK(srtp_session_print_policy(srtp_sender));

    if (policy->ssrc.type != ssrc_specific) {
        ssrc = 0xdecafbad;
    } else {
        ssrc = policy->ssrc.value;
    }

    // 0 byte input
    rtcp = create_rtcp_test_packet(28, ssrc, &rtcp_len, &buffer_len);
    srtcp_len = buffer_len;
    overrun_check_prepare(rtcp, 0, buffer_len);
    CHECK_RETURN(
        call_srtp_protect_rtcp2(srtp_sender, rtcp, 0, &srtcp_len, mki_index),
        srtp_err_status_bad_param);
    CHECK_OVERRUN(rtcp, 0, buffer_len);
    free(rtcp);

    // 1 byte input
    rtcp = create_rtcp_test_packet(28, ssrc, &rtcp_len, &buffer_len);
    srtcp_len = buffer_len;
    overrun_check_prepare(rtcp, 1, buffer_len);
    CHECK_RETURN(
        call_srtp_protect_rtcp2(srtp_sender, rtcp, 1, &srtcp_len, mki_index),
        srtp_err_status_bad_param);
    CHECK_OVERRUN(rtcp, 1, buffer_len);
    free(rtcp);

    // too short header
    rtcp = create_rtcp_test_packet(28, ssrc, &rtcp_len, &buffer_len);
    srtcp_len = buffer_len;
    overrun_check_prepare(rtcp, rtcp_header_len - 1, buffer_len);
    CHECK_RETURN(call_srtp_protect_rtcp2(srtp_sender, rtcp, rtcp_header_len - 1,
                                         &srtcp_len, mki_index),
                 srtp_err_status_bad_param);
    CHECK_OVERRUN(rtcp, rtcp_header_len - 1, buffer_len);
    free(rtcp);

    // zero payload
    rtcp = create_rtcp_test_packet(28, ssrc, &rtcp_len, &buffer_len);
    srtcp_len = buffer_len;
    overrun_check_prepare(rtcp, rtcp_header_len, buffer_len);
    CHECK_OK(call_srtp_protect_rtcp2(srtp_sender, rtcp, rtcp_header_len,
                                     &srtcp_len, mki_index));
    CHECK(srtcp_len == rtcp_header_len + trailer_len);
    CHECK_OVERRUN(rtcp, srtcp_len, buffer_len);
    free(rtcp);

    // 1 byte payload
    rtcp = create_rtcp_test_packet(28, ssrc, &rtcp_len, &buffer_len);
    srtcp_len = buffer_len;
    overrun_check_prepare(rtcp, rtcp_header_len + 1, buffer_len);
    CHECK_OK(call_srtp_protect_rtcp2(srtp_sender, rtcp, rtcp_header_len + 1,
                                     &srtcp_len, mki_index));
    CHECK(srtcp_len == rtcp_header_len + 1 + trailer_len);
    CHECK_OVERRUN(rtcp, srtcp_len, buffer_len);
    free(rtcp);

    // 0 byte output
    rtcp = create_rtcp_test_packet(28, ssrc, &rtcp_len, &buffer_len);
    srtcp_len = 0;
    overrun_check_prepare(rtcp, rtcp_len, buffer_len);
    CHECK_RETURN(call_srtp_protect_rtcp2(srtp_sender, rtcp, rtcp_len,
                                         &srtcp_len, mki_index),
                 srtp_err_status_buffer_small);
    CHECK_OVERRUN(rtcp, rtcp_len, buffer_len);
    free(rtcp);

    // 1 byte output
    rtcp = create_rtcp_test_packet(28, ssrc, &rtcp_len, &buffer_len);
    srtcp_len = 1;
    overrun_check_prepare(rtcp, rtcp_len, buffer_len);
    CHECK_RETURN(call_srtp_protect_rtcp2(srtp_sender, rtcp, rtcp_len,
                                         &srtcp_len, mki_index),
                 srtp_err_status_buffer_small);
    CHECK_OVERRUN(rtcp, rtcp_len, buffer_len);
    free(rtcp);

    if (trailer_len != 0) {
        // no space for trailer output
        rtcp = create_rtcp_test_packet(28, ssrc, &rtcp_len, &buffer_len);
        srtcp_len = rtcp_len;
        overrun_check_prepare(rtcp, rtcp_len, buffer_len);
        CHECK_RETURN(call_srtp_protect_rtcp2(srtp_sender, rtcp, rtcp_len,
                                             &srtcp_len, mki_index),
                     srtp_err_status_buffer_small);
        CHECK_OVERRUN(rtcp, rtcp_len, buffer_len);
        free(rtcp);
    }

    // 1 byte too small output
    rtcp = create_rtcp_test_packet(28, ssrc, &rtcp_len, &buffer_len);
    srtcp_len = rtcp_len + trailer_len - 1;
    overrun_check_prepare(rtcp, rtcp_len, buffer_len);
    CHECK_RETURN(call_srtp_protect_rtcp2(srtp_sender, rtcp, rtcp_len,
                                         &srtcp_len, mki_index),
                 srtp_err_status_buffer_small);
    CHECK_OVERRUN(rtcp, rtcp_len, buffer_len);
    free(rtcp);

    // full payload
    rtcp = create_rtcp_test_packet(28, ssrc, &rtcp_len, &buffer_len);
    srtcp_len = buffer_len;
    CHECK_OK(call_srtp_protect_rtcp2(srtp_sender, rtcp, rtcp_len, &srtcp_len,
                                     mki_index));
    CHECK(srtcp_len == rtcp_len + trailer_len);
    CHECK_OVERRUN(rtcp, srtcp_len, buffer_len);

    CHECK_OK(srtp_dealloc(srtp_sender));

    // unprotect
    srtp_t srtp_receiver;
    srtp_policy_t receive_policy;

    memcpy(&receive_policy, &send_policy, sizeof(srtp_policy_t));
    receive_policy.ssrc.type = ssrc_any_inbound;

    CHECK_OK(srtp_create(&srtp_receiver, &receive_policy));

    // unprotect zero byte input
    rtcp_len = buffer_len;
    CHECK_RETURN(call_srtp_unprotect_rtcp2(srtp_receiver, rtcp, 0, &rtcp_len),
                 srtp_err_status_bad_param);

    // unprotect 1 byte input
    rtcp_len = buffer_len;
    CHECK_RETURN(call_srtp_unprotect_rtcp2(srtp_receiver, rtcp, 1, &rtcp_len),
                 srtp_err_status_bad_param);

    // unprotect short header
    rtcp_len = buffer_len;
    CHECK_RETURN(call_srtp_unprotect_rtcp2(srtp_receiver, rtcp,
                                           rtcp_header_len - 1, &rtcp_len),
                 srtp_err_status_bad_param);

    // 0 byte output
    rtcp_len = 0;
    CHECK_RETURN(
        call_srtp_unprotect_rtcp2(srtp_receiver, rtcp, srtcp_len, &rtcp_len),
        srtp_err_status_buffer_small);

    // 1 byte output
    rtcp_len = 1;
    CHECK_RETURN(
        call_srtp_unprotect_rtcp2(srtp_receiver, rtcp, srtcp_len, &rtcp_len),
        srtp_err_status_buffer_small);

    // 1 byte too small output
    rtcp_len = srtcp_len - trailer_len - 1;
    CHECK_RETURN(
        call_srtp_unprotect_rtcp2(srtp_receiver, rtcp, srtcp_len, &rtcp_len),
        srtp_err_status_buffer_small);

    // full unprotect
    rtcp_len = buffer_len;
    CHECK_OK(
        call_srtp_unprotect_rtcp2(srtp_receiver, rtcp, srtcp_len, &rtcp_len));
    CHECK(rtcp_len == srtcp_len - trailer_len);

    free(rtcp);

    CHECK_OK(srtp_dealloc(srtp_receiver));

    return srtp_err_status_ok;
}

struct srtp_session_print_stream_data {
    // set by callback to indicate failure
    srtp_err_status_t status;
    // indicates if it is the template stream or a regular stream
    int is_template;
};

bool srtp_session_print_stream(srtp_stream_t stream, void *raw_data)
{
    static const char *serv_descr[4] = { "none", "confidentiality",
                                         "authentication",
                                         "confidentiality and authentication" };
    static const char *direction[3] = { "unknown", "outbound", "inbound" };

    struct srtp_session_print_stream_data *data =
        (struct srtp_session_print_stream_data *)raw_data;
    srtp_session_keys_t *session_keys = &stream->session_keys[0];
    char ssrc_text[32];

    if (!data->is_template && stream->rtp_services > sec_serv_conf_and_auth) {
        data->status = srtp_err_status_bad_param;
        return false;
    }

    if (data->is_template) {
        snprintf(ssrc_text, sizeof(ssrc_text), "any %s",
                 direction[stream->direction]);
    } else {
        snprintf(ssrc_text, sizeof(ssrc_text), "0x%08x", stream->ssrc);
    }

    printf("# SSRC:          %s\r\n"
           "# rtp cipher:    %s\r\n"
           "# rtp auth:      %s\r\n"
           "# rtp services:  %s\r\n"
           "# rtcp cipher:   %s\r\n"
           "# rtcp auth:     %s\r\n"
           "# rtcp services: %s\r\n"
           "# num keys:      %zu\r\n"
           "# use mki:       %s\r\n"
           "# mki size:      %zu\r\n"
           "# window size:   %zu\r\n"
           "# tx rtx allowed:%s\r\n",
           ssrc_text, session_keys->rtp_cipher->type->description,
           session_keys->rtp_auth->type->description,
           serv_descr[stream->rtp_services],
           session_keys->rtcp_cipher->type->description,
           session_keys->rtcp_auth->type->description,
           serv_descr[stream->rtcp_services], stream->num_master_keys,
           stream->use_mki ? "true" : "false", stream->mki_size,
           srtp_rdbx_get_window_size(&stream->rtp_rdbx),
           stream->allow_repeat_tx ? "true" : "false");

    printf("# Encrypted extension headers: ");
    if (stream->enc_xtn_hdr && stream->enc_xtn_hdr_count > 0) {
        uint8_t *enc_xtn_hdr = stream->enc_xtn_hdr;
        size_t count = stream->enc_xtn_hdr_count;
        while (count > 0) {
            printf("%d ", *enc_xtn_hdr);
            enc_xtn_hdr++;
            count--;
        }
        printf("\n");
    } else {
        printf("none\n");
    }

    return true;
}

srtp_err_status_t srtp_session_print_policy(srtp_t srtp)
{
    struct srtp_session_print_stream_data data = { srtp_err_status_ok, 0 };

    /* sanity checking */
    if (srtp == NULL) {
        return srtp_err_status_fail;
    }

    /* if there's a template stream, print it out */
    if (srtp->stream_template != NULL) {
        data.is_template = 1;
        srtp_session_print_stream(srtp->stream_template, &data);
    }

    /* loop over streams in session, printing the policy of each */
    data.is_template = 0;
    srtp_stream_list_for_each(srtp->stream_list, srtp_session_print_stream,
                              &data);

    return data.status;
}

srtp_err_status_t srtp_print_policy(const srtp_policy_t *policy)
{
    srtp_err_status_t status;
    srtp_t session;

    status = srtp_create(&session, policy);
    if (status) {
        return status;
    }
    status = srtp_session_print_policy(session);
    if (status) {
        return status;
    }
    status = srtp_dealloc(session);
    if (status) {
        return status;
    }
    return srtp_err_status_ok;
}

/*
 * srtp_print_packet(...) is for debugging only
 * it prints an RTP packet to the stdout
 *
 * note that this function is *not* threadsafe
 */

#include <stdio.h>

#define MTU 2048

char packet_string[MTU];

char *srtp_packet_to_string(uint8_t *packet, size_t pkt_octet_len)
{
    srtp_hdr_t *hdr = (srtp_hdr_t *)packet;
    size_t octets_in_rtp_header = 12;
    uint8_t *data = packet + octets_in_rtp_header;
    size_t data_len = pkt_octet_len - octets_in_rtp_header;

    /* sanity checking */
    if ((hdr == NULL) || (pkt_octet_len > MTU)) {
        return NULL;
    }

    /* write packet into string */
    int offset = snprintf(packet_string, sizeof(packet_string),
                          "(s)rtp packet: {\n"
                          "   version:\t%d\n"
                          "   p:\t\t%d\n"
                          "   x:\t\t%d\n"
                          "   cc:\t\t%d\n"
                          "   m:\t\t%d\n"
                          "   pt:\t\t%x\n"
                          "   seq:\t\t%x\n"
                          "   ts:\t\t%x\n"
                          "   ssrc:\t%x",
                          hdr->version, hdr->p, hdr->x, hdr->cc, hdr->m,
                          hdr->pt, hdr->seq, hdr->ts, hdr->ssrc);

    if (hdr->cc) {
        offset += snprintf(packet_string + offset,
                           sizeof(packet_string) - offset, "\n   csrc:\t");
        for (int i = 0; i < hdr->cc; i++) {
            offset +=
                snprintf(packet_string + offset, sizeof(packet_string) - offset,
                         "%x ", *((&hdr->ssrc) + 1 + i));
        }
        data += 4 * hdr->cc;
        data_len -= 4 * hdr->cc;
    }

    if (hdr->x) {
        uint16_t profile = *((uint16_t *)data);
        data += 2;
        data_len -= 2;
        uint16_t length = ntohs(*((uint16_t *)data)) * 4;
        data += 2;
        data_len -= 2;
        offset += snprintf(packet_string + offset,
                           sizeof(packet_string) - offset, "\n   xtn:\t\t%x %s",
                           profile, octet_string_hex_string(data, length));
        data += length;
        data_len -= length;
    }

    snprintf(packet_string + offset, sizeof(packet_string) - offset,
             "\n   data:\t%s\n"
             "} (%zu octets in total)\n",
             octet_string_hex_string(data, data_len), pkt_octet_len);

    return packet_string;
}

char *srtp_rtcp_packet_to_string(uint8_t *packet, size_t pkt_octet_len)
{
    srtcp_hdr_t *hdr = (srtcp_hdr_t *)packet;
    size_t octets_in_rtcp_header = 8;
    uint8_t *data = packet + octets_in_rtcp_header;
    size_t hex_len = pkt_octet_len - octets_in_rtcp_header;

    /* sanity checking */
    if ((hdr == NULL) || (pkt_octet_len > MTU)) {
        return NULL;
    }

    /* write packet into string */
    snprintf(packet_string, sizeof(packet_string),
             "(s)rtcp packet: {\n"
             "   version:\t%d\n"
             "   p:\t\t%d\n"
             "   rc:\t\t%d\n"
             "   pt:\t\t%x\n"
             "   len:\t\t%x\n"
             "   ssrc:\t%x\n"
             "   data:\t%s\n"
             "} (%zu octets in total)\n",
             hdr->version, hdr->p, hdr->rc, hdr->pt, hdr->len, hdr->ssrc,
             octet_string_hex_string(data, hex_len), pkt_octet_len);

    return packet_string;
}

/*
 * mips_estimate() is a simple function to estimate the number of
 * instructions per second that the host can perform.  note that this
 * function can be grossly wrong; you may want to have a manual sanity
 * check of its output!
 *
 * the 'ignore' pointer is there to convince the compiler to not just
 * optimize away the function
 */

double mips_estimate(size_t num_trials, size_t *ignore)
{
    clock_t t;
    volatile size_t sum;

    sum = 0;
    t = clock();
    for (size_t i = 0; i < num_trials; i++) {
        sum += i;
    }
    t = clock() - t;
    if (t < 1) {
        t = 1;
    }

    /*   printf("%d\n", sum); */
    *ignore = sum;

    return (double)num_trials * CLOCKS_PER_SEC / t;
}

/*
 * srtp_validate() verifies the correctness of libsrtp by comparing
 * some computed packets against some pre-computed reference values.
 * These packets were made with the default SRTP policy.
 */

srtp_err_status_t srtp_validate(void)
{
    // clang-format off
    uint8_t srtp_plaintext_ref[28] = {
        0x80, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab
    };
    uint8_t srtp_plaintext[38] = {
        0x80, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    uint8_t srtp_ciphertext[38] = {
        0x80, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0x4e, 0x55, 0xdc, 0x4c,
        0xe7, 0x99, 0x78, 0xd8, 0x8c, 0xa4, 0xd2, 0x15,
        0x94, 0x9d, 0x24, 0x02, 0xb7, 0x8d, 0x6a, 0xcc,
        0x99, 0xea, 0x17, 0x9b, 0x8d, 0xbb
    };
    uint8_t rtcp_plaintext_ref[24] = {
        0x81, 0xc8, 0x00, 0x0b, 0xca, 0xfe, 0xba, 0xbe,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
    };
    uint8_t rtcp_plaintext[38] = {
        0x81, 0xc8, 0x00, 0x0b, 0xca, 0xfe, 0xba, 0xbe,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    uint8_t srtcp_ciphertext[38] = {
        0x81, 0xc8, 0x00, 0x0b, 0xca, 0xfe, 0xba, 0xbe,
        0x71, 0x28, 0x03, 0x5b, 0xe4, 0x87, 0xb9, 0xbd,
        0xbe, 0xf8, 0x90, 0x41, 0xf9, 0x77, 0xa5, 0xa8,
        0x80, 0x00, 0x00, 0x01, 0x99, 0x3e, 0x08, 0xcd,
        0x54, 0xd6, 0xc1, 0x23, 0x07, 0x98
    };
    // clang-format on

    srtp_t srtp_snd, srtp_recv;
    srtp_err_status_t status;
    size_t len;
    srtp_policy_t policy;

    /*
     * create a session with a single stream using the default srtp
     * policy and with the SSRC value 0xcafebabe
     */
    memset(&policy, 0, sizeof(policy));
    srtp_crypto_policy_set_rtp_default(&policy.rtp);
    srtp_crypto_policy_set_rtcp_default(&policy.rtcp);
    policy.ssrc.type = ssrc_specific;
    policy.ssrc.value = 0xcafebabe;
    policy.key = test_key;
    policy.window_size = 128;
    policy.allow_repeat_tx = false;
    policy.next = NULL;

    status = srtp_create(&srtp_snd, &policy);
    if (status) {
        return status;
    }

    /*
     * protect plaintext, then compare with ciphertext
     */
    len = 28;
    status = call_srtp_protect(srtp_snd, srtp_plaintext, &len, 0);
    if (status || (len != 38)) {
        return srtp_err_status_fail;
    }

    debug_print(mod_driver, "ciphertext:\n  %s",
                octet_string_hex_string(srtp_plaintext, len));
    debug_print(mod_driver, "ciphertext reference:\n  %s",
                octet_string_hex_string(srtp_ciphertext, len));

    if (!srtp_octet_string_equal(srtp_plaintext, srtp_ciphertext, len)) {
        return srtp_err_status_fail;
    }

    /*
     * protect plaintext rtcp, then compare with srtcp ciphertext
     */
    len = 24;
    status = call_srtp_protect_rtcp(srtp_snd, rtcp_plaintext, &len, 0);
    if (status || (len != 38)) {
        return srtp_err_status_fail;
    }

    debug_print(mod_driver, "srtcp ciphertext:\n  %s",
                octet_string_hex_string(rtcp_plaintext, len));
    debug_print(mod_driver, "srtcp ciphertext reference:\n  %s",
                octet_string_hex_string(srtcp_ciphertext, len));

    if (!srtp_octet_string_equal(rtcp_plaintext, srtcp_ciphertext, len)) {
        return srtp_err_status_fail;
    }

    /*
     * create a receiver session context comparable to the one created
     * above - we need to do this so that the replay checking doesn't
     * complain
     */
    status = srtp_create(&srtp_recv, &policy);
    if (status) {
        return status;
    }

    /*
     * unprotect ciphertext, then compare with plaintext
     */
    status = call_srtp_unprotect(srtp_recv, srtp_ciphertext, &len);
    if (status || (len != 28)) {
        return status;
    }

    if (!srtp_octet_string_equal(srtp_ciphertext, srtp_plaintext_ref, len)) {
        return srtp_err_status_fail;
    }

    /*
     * unprotect srtcp ciphertext, then compare with rtcp plaintext
     */
    len = 38;
    status = call_srtp_unprotect_rtcp(srtp_recv, srtcp_ciphertext, &len);
    if (status || (len != 24)) {
        return status;
    }

    if (!srtp_octet_string_equal(srtcp_ciphertext, rtcp_plaintext_ref, len)) {
        return srtp_err_status_fail;
    }

    status = srtp_dealloc(srtp_snd);
    if (status) {
        return status;
    }

    status = srtp_dealloc(srtp_recv);
    if (status) {
        return status;
    }

    return srtp_err_status_ok;
}

/*
 * srtp_validate_mki() verifies the correctness of libsrtp by comparing
 * some computed packets against some pre-computed reference values.
 * These packets were made with the default SRTP policy.
 */

srtp_err_status_t srtp_validate_mki(void)
{
    // clang-format off
    uint8_t srtp_plaintext_ref[28] = {
        0x80, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab
    };
    uint8_t srtp_plaintext[42] = {
        0x80, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00
    };
    uint8_t srtp_ciphertext[42] = {
        0x80, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0x4e, 0x55, 0xdc, 0x4c,
        0xe7, 0x99, 0x78, 0xd8, 0x8c, 0xa4, 0xd2, 0x15,
        0x94, 0x9d, 0x24, 0x02, 0xe1, 0xf9, 0x7a, 0x0d,
        0xb7, 0x8d, 0x6a, 0xcc, 0x99, 0xea, 0x17, 0x9b,
        0x8d, 0xbb
    };
    uint8_t rtcp_plaintext_ref[24] = {
        0x81, 0xc8, 0x00, 0x0b, 0xca, 0xfe, 0xba, 0xbe,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
    };
    uint8_t rtcp_plaintext[42] = {
        0x81, 0xc8, 0x00, 0x0b, 0xca, 0xfe, 0xba, 0xbe,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00
    };
    uint8_t srtcp_ciphertext[42] = {
        0x81, 0xc8, 0x00, 0x0b, 0xca, 0xfe, 0xba, 0xbe,
        0x71, 0x28, 0x03, 0x5b, 0xe4, 0x87, 0xb9, 0xbd,
        0xbe, 0xf8, 0x90, 0x41, 0xf9, 0x77, 0xa5, 0xa8,
        0x80, 0x00, 0x00, 0x01, 0xe1, 0xf9, 0x7a, 0x0d,
        0x99, 0x3e, 0x08, 0xcd, 0x54, 0xd6, 0xc1, 0x23,
        0x07, 0x98
    };
    // clang-format on

    srtp_t srtp_snd, srtp_recv;
    srtp_err_status_t status;
    size_t len;
    srtp_policy_t policy;

    /*
     * create a session with a single stream using the default srtp
     * policy and with the SSRC value 0xcafebabe
     */
    memset(&policy, 0, sizeof(policy));
    srtp_crypto_policy_set_rtp_default(&policy.rtp);
    srtp_crypto_policy_set_rtcp_default(&policy.rtcp);
    policy.ssrc.type = ssrc_specific;
    policy.ssrc.value = 0xcafebabe;
    policy.keys = test_keys;
    policy.num_master_keys = 2;
    policy.use_mki = true;
    policy.mki_size = TEST_MKI_ID_SIZE;
    policy.window_size = 128;
    policy.allow_repeat_tx = false;
    policy.next = NULL;

    status = srtp_create(&srtp_snd, &policy);
    if (status) {
        return status;
    }

    /*
     * protect plaintext, then compare with ciphertext
     */
    len = 28;
    status = call_srtp_protect(srtp_snd, srtp_plaintext, &len, 0);
    if (status) {
        return status;
    }

    debug_print(mod_driver, "ciphertext:\n  %s",
                octet_string_hex_string(srtp_plaintext, len));
    debug_print(
        mod_driver, "ciphertext reference:\n  %s",
        octet_string_hex_string(srtp_ciphertext, sizeof(srtp_ciphertext)));

    if (len != sizeof(srtp_ciphertext)) {
        return srtp_err_status_fail;
    }

    if (!srtp_octet_string_equal(srtp_plaintext, srtp_ciphertext, len)) {
        return srtp_err_status_fail;
    }

    /*
     * protect plaintext rtcp, then compare with srtcp ciphertext
     */
    len = 24;
    status = call_srtp_protect_rtcp(srtp_snd, rtcp_plaintext, &len, 0);
    if (status) {
        return status;
    }

    debug_print(mod_driver, "srtcp ciphertext:\n  %s",
                octet_string_hex_string(rtcp_plaintext, len));
    debug_print(
        mod_driver, "srtcp ciphertext reference:\n  %s",
        octet_string_hex_string(srtcp_ciphertext, sizeof(srtcp_ciphertext)));

    if (len != sizeof(srtcp_ciphertext)) {
        return srtp_err_status_fail;
    }

    if (!srtp_octet_string_equal(rtcp_plaintext, srtcp_ciphertext, len)) {
        return srtp_err_status_fail;
    }

    /*
     * create a receiver session context comparable to the one created
     * above - we need to do this so that the replay checking doesn't
     * complain
     */
    status = srtp_create(&srtp_recv, &policy);
    if (status) {
        return status;
    }

    /*
     * unprotect ciphertext, then compare with plaintext
     */
    len = 42;
    status = call_srtp_unprotect(srtp_recv, srtp_ciphertext, &len);
    if (status || (len != 28)) {
        return status;
    }

    if (!srtp_octet_string_equal(srtp_ciphertext, srtp_plaintext_ref, len)) {
        return srtp_err_status_fail;
    }

    /*
     * unprotect srtcp ciphertext, then compare with rtcp plaintext
     */
    len = 42;
    status = call_srtp_unprotect_rtcp(srtp_recv, srtcp_ciphertext, &len);
    if (status || (len != 24)) {
        return status;
    }

    if (!srtp_octet_string_equal(srtcp_ciphertext, rtcp_plaintext_ref, len)) {
        return srtp_err_status_fail;
    }

    status = srtp_dealloc(srtp_snd);
    if (status) {
        return status;
    }

    status = srtp_dealloc(srtp_recv);
    if (status) {
        return status;
    }

    return srtp_err_status_ok;
}

/*
 * srtp_validate_null_sha1_80() verifies the correctness of libsrtp by comparing
 * some computed packets against some pre-computed reference values.
 * These packets were made with a policy that applies null encryption
 * and HMAC authentication.
 */

srtp_err_status_t srtp_validate_null_sha1_80(void)
{
    // clang-format off
    uint8_t srtp_plaintext_ref[28] = {
        0x80, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab
    };
    uint8_t srtp_plaintext[38] = {
        0x80, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    uint8_t srtp_ciphertext[38] = {
        0x80, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xa1, 0x36, 0x27,
        0x0b, 0x67, 0x91, 0x34, 0xce, 0x9b
    };
    uint8_t rtcp_plaintext_ref[24] = {
        0x81, 0xc8, 0x00, 0x0b, 0xca, 0xfe, 0xba, 0xbe,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
    };
    uint8_t rtcp_plaintext[38] = {
        0x81, 0xc8, 0x00, 0x0b, 0xca, 0xfe, 0xba, 0xbe,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    uint8_t srtcp_ciphertext[38] = {
        0x81, 0xc8, 0x00, 0x0b, 0xca, 0xfe, 0xba, 0xbe,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0x00, 0x00, 0x00, 0x01, 0xfe, 0x88, 0xc7, 0xfd,
        0xfd, 0x37, 0xeb, 0xce, 0x61, 0x5d,
    };
    // clang-format on

    srtp_t srtp_snd, srtp_recv;
    srtp_err_status_t status;
    size_t len;
    srtp_policy_t policy;

    /*
     * create a session with a single stream using the null cipher
     * and sha1_80 policy and with the SSRC value 0xcafebabe
     */
    memset(&policy, 0, sizeof(policy));
    srtp_crypto_policy_set_null_cipher_hmac_sha1_80(&policy.rtp);
    srtp_crypto_policy_set_null_cipher_hmac_sha1_80(&policy.rtcp);
    policy.ssrc.type = ssrc_specific;
    policy.ssrc.value = 0xcafebabe;
    policy.key = test_key;
    policy.window_size = 128;
    policy.allow_repeat_tx = false;
    policy.next = NULL;

    status = srtp_create(&srtp_snd, &policy);
    if (status) {
        return status;
    }

    /*
     * protect plaintext, then compare with ciphertext
     */
    len = 28;
    status = call_srtp_protect(srtp_snd, srtp_plaintext, &len, 0);
    if (status || (len != 38)) {
        return srtp_err_status_fail;
    }

    debug_print(mod_driver, "ciphertext:\n  %s",
                octet_string_hex_string(srtp_plaintext, len));
    debug_print(mod_driver, "ciphertext reference:\n  %s",
                octet_string_hex_string(srtp_ciphertext, len));

    if (!srtp_octet_string_equal(srtp_plaintext, srtp_ciphertext, len)) {
        return srtp_err_status_fail;
    }

    /*
     * protect plaintext rtcp, then compare with srtcp ciphertext
     */
    len = 24;
    status = call_srtp_protect_rtcp(srtp_snd, rtcp_plaintext, &len, 0);
    if (status || (len != 38)) {
        return srtp_err_status_fail;
    }

    debug_print(mod_driver, "srtcp ciphertext:\n  %s",
                octet_string_hex_string(rtcp_plaintext, len));
    debug_print(mod_driver, "srtcp ciphertext reference:\n  %s",
                octet_string_hex_string(srtcp_ciphertext, len));

    if (!srtp_octet_string_equal(rtcp_plaintext, srtcp_ciphertext, len)) {
        return srtp_err_status_fail;
    }

    /*
     * create a receiver session context comparable to the one created
     * above - we need to do this so that the replay checking doesn't
     * complain
     */
    status = srtp_create(&srtp_recv, &policy);
    if (status) {
        return status;
    }

    /*
     * unprotect ciphertext, then compare with plaintext
     */
    status = call_srtp_unprotect(srtp_recv, srtp_ciphertext, &len);
    if (status || (len != 28)) {
        return status;
    }

    if (!srtp_octet_string_equal(srtp_ciphertext, srtp_plaintext_ref, len)) {
        return srtp_err_status_fail;
    }

    /*
     * unprotect srtcp ciphertext, then compare with rtcp plaintext
     */
    len = 38;
    status = call_srtp_unprotect_rtcp(srtp_recv, srtcp_ciphertext, &len);
    if (status || (len != 24)) {
        return status;
    }

    if (!srtp_octet_string_equal(srtcp_ciphertext, rtcp_plaintext_ref, len)) {
        return srtp_err_status_fail;
    }

    status = srtp_dealloc(srtp_snd);
    if (status) {
        return status;
    }

    status = srtp_dealloc(srtp_recv);
    if (status) {
        return status;
    }

    return srtp_err_status_ok;
}

/*
 * srtp_validate_null_null() verifies the correctness of libsrtp by comparing
 * some computed packets against some pre-computed reference values.
 * These packets were made with a policy that applies null encryption
 * and null authentication.
 */

srtp_err_status_t srtp_validate_null_null(void)
{
    // clang-format off
    uint8_t srtp_plaintext_ref[28] = {
        0x80, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab
    };
    uint8_t srtp_plaintext[28] = {
        0x80, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab,
    };
    uint8_t srtp_ciphertext[28] = {
        0x80, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab,
    };
    uint8_t rtcp_plaintext_ref[24] = {
        0x81, 0xc8, 0x00, 0x0b, 0xca, 0xfe, 0xba, 0xbe,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
    };
    uint8_t rtcp_plaintext[28] = {
        0x81, 0xc8, 0x00, 0x0b, 0xca, 0xfe, 0xba, 0xbe,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0x00, 0x00, 0x00, 0x00,
    };
    uint8_t srtcp_ciphertext[28] = {
        0x81, 0xc8, 0x00, 0x0b, 0xca, 0xfe, 0xba, 0xbe,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0x00, 0x00, 0x00, 0x01,
    };
    // clang-format on

    srtp_t srtp_snd, srtp_recv;
    srtp_err_status_t status;
    size_t len;
    srtp_policy_t policy;

    /*
     * create a session with a single stream using the null cipher
     * and null hmac policy and with the SSRC value 0xcafebabe
     */
    memset(&policy, 0, sizeof(policy));
    srtp_crypto_policy_set_null_cipher_hmac_null(&policy.rtp);
    srtp_crypto_policy_set_null_cipher_hmac_null(&policy.rtcp);
    policy.ssrc.type = ssrc_specific;
    policy.ssrc.value = 0xcafebabe;
    /*
     * We need some non-zero value set here
     */
    policy.key = (void *)(uintptr_t)-1;
    policy.window_size = 128;
    policy.allow_repeat_tx = false;
    policy.next = NULL;

    status = srtp_create(&srtp_snd, &policy);
    if (status) {
        return status;
    }

    /*
     * protect plaintext, then compare with ciphertext
     */
    len = 28;
    status = call_srtp_protect(srtp_snd, srtp_plaintext, &len, 0);
    if (!status && (len != 28)) {
        status = srtp_err_status_fail;
    }
    if (status) {
        return status;
    }

    debug_print(mod_driver, "ciphertext:\n  %s",
                octet_string_hex_string(srtp_plaintext, len));
    debug_print(mod_driver, "ciphertext reference:\n  %s",
                octet_string_hex_string(srtp_ciphertext, len));

    if (!srtp_octet_string_equal(srtp_plaintext, srtp_ciphertext, len)) {
        return srtp_err_status_fail;
    }

    /*
     * protect plaintext rtcp, then compare with srtcp ciphertext
     */
    len = 24;
    status = call_srtp_protect_rtcp(srtp_snd, rtcp_plaintext, &len, 0);
    if (!status && (len != 28)) {
        status = srtp_err_status_fail;
    }
    if (status) {
        return status;
    }

    debug_print(mod_driver, "srtcp ciphertext:\n  %s",
                octet_string_hex_string(rtcp_plaintext, len));
    debug_print(mod_driver, "srtcp ciphertext reference:\n  %s",
                octet_string_hex_string(srtcp_ciphertext, len));

    if (!srtp_octet_string_equal(rtcp_plaintext, srtcp_ciphertext, len)) {
        return srtp_err_status_fail;
    }

    /*
     * create a receiver session context comparable to the one created
     * above - we need to do this so that the replay checking doesn't
     * complain
     */
    status = srtp_create(&srtp_recv, &policy);
    if (status) {
        return status;
    }

    /*
     * unprotect ciphertext, then compare with plaintext
     */
    status = call_srtp_unprotect(srtp_recv, srtp_ciphertext, &len);
    if (!status && (len != 28)) {
        status = srtp_err_status_fail;
    }
    if (status) {
        return status;
    }

    if (!srtp_octet_string_equal(srtp_ciphertext, srtp_plaintext_ref, len)) {
        return srtp_err_status_fail;
    }

    /*
     * unprotect srtcp ciphertext, then compare with rtcp plaintext
     */
    len = 28;
    status = call_srtp_unprotect_rtcp(srtp_recv, srtcp_ciphertext, &len);
    if (!status && (len != 24)) {
        status = srtp_err_status_fail;
    }
    if (status) {
        return status;
    }

    if (!srtp_octet_string_equal(srtcp_ciphertext, rtcp_plaintext_ref, len)) {
        return srtp_err_status_fail;
    }

    status = srtp_dealloc(srtp_snd);
    if (status) {
        return status;
    }

    status = srtp_dealloc(srtp_recv);
    if (status) {
        return status;
    }

    return srtp_err_status_ok;
}

/*
 * srtp_validate_cryptex() verifies the correctness of libsrtp by comparing
 * some computed packets against some pre-computed reference values.
 * These packets were made with the default SRTP policy.
 */
srtp_err_status_t srtp_validate_cryptex(void)
{
    // clang-format off
    /* Plaintext packet with 1-byte header extension */
    const char *srtp_1bytehdrext_ref =
        "900f1235"
        "decafbad"
        "cafebabe"
        "bede0001"
        "51000200"
        "abababab"
        "abababab"
        "abababab"
        "abababab";

    /* AES-CTR/HMAC-SHA1 Ciphertext packet with 1-byte header extension */
    const char *srtp_1bytehdrext_cryptex =
        "900f1235"
        "decafbad"
        "cafebabe"
        "c0de0001"
        "eb923652"
        "51c3e036"
        "f8de27e9"
        "c27ee3e0"
        "b4651d9f"
        "bc4218a7"
        "0244522f"
        "34a5";

    /* Plaintext packet with 2-byte header extension */
    const char *srtp_2bytehdrext_ref =
        "900f1236"
        "decafbad"
        "cafebabe"
        "10000001"
        "05020002"
        "abababab"
        "abababab"
        "abababab"
        "abababab";

    /* AES-CTR/HMAC-SHA1 Ciphertext packet with 2-byte header extension */
    const char *srtp_2bytehdrext_cryptex =
        "900f1236"
        "decafbad"
        "cafebabe"
        "c2de0001"
        "4ed9cc4e"
        "6a712b30"
        "96c5ca77"
        "339d4204"
        "ce0d7739"
        "6cab6958"
        "5fbce381"
        "94a5";

    /* Plaintext packet with 1-byte header extension and CSRC fields. */
    const char *srtp_1bytehdrext_cc_ref =
        "920f1238"
        "decafbad"
        "cafebabe"
        "0001e240"
        "0000b26e"
        "bede0001"
        "51000200"
        "abababab"
        "abababab"
        "abababab"
        "abababab";

    const char *srtp_1bytehdrext_cc_cryptex =
        "920f1238"
        "decafbad"
        "cafebabe"
        "8bb6e12b"
        "5cff16dd"
        "c0de0001"
        "92838c8c"
        "09e58393"
        "e1de3a9a"
        "74734d67"
        "45671338"
        "c3acf11d"
        "a2df8423"
        "bee0";

    /* Plaintext packet with 2-byte header extension and CSRC fields. */
    const char *srtp_2bytehdrext_cc_ref =
        "920f1239"
        "decafbad"
        "cafebabe"
        "0001e240"
        "0000b26e"
        "10000001"
        "05020002"
        "abababab"
        "abababab"
        "abababab"
        "abababab";

    const char *srtp_2bytehdrext_cc_cryptex =
        "920f1239"
        "decafbad"
        "cafebabe"
        "f70e513e"
        "b90b9b25"
        "c2de0001"
        "bbed4848"
        "faa64466"
        "5f3d7f34"
        "125914e9"
        "f4d0ae92"
        "3c6f479b"
        "95a0f7b5"
        "3133";

    /* Plaintext packet with empty 1-byte header extension and CSRC fields. */
    const char *srtp_1byte_empty_hdrext_cc_ref =
        "920f123a"
        "decafbad"
        "cafebabe"
        "0001e240"
        "0000b26e"
        "bede0000"
        "abababab"
        "abababab"
        "abababab"
        "abababab";

    const char *srtp_1byte_empty_hdrext_cc_cryptex =
        "920f123a"
        "decafbad"
        "cafebabe"
        "7130b6ab"
        "fe2ab0e3"
        "c0de0000"
        "e3d9f64b"
        "25c9e74c"
        "b4cf8e43"
        "fb92e378"
        "1c2c0cea"
        "b6b3a499"
        "a14c";

    /* Plaintext packet with empty 2-byte header extension and CSRC fields. */
    const char *srtp_2byte_empty_hdrext_cc_ref =
        "920f123b"
        "decafbad"
        "cafebabe"
        "0001e240"
        "0000b26e"
        "10000000"
        "abababab"
        "abababab"
        "abababab"
        "abababab";

    const char *srtp_2byte_empty_hdrext_cc_cryptex =
        "920f123b"
        "decafbad"
        "cafebabe"
        "cbf24c12"
        "4330e1c8"
        "c2de0000"
        "599dd45b"
        "c9d687b6"
        "03e8b59d"
        "771fd38e"
        "88b170e0"
        "cd31e125"
        "eabe";

    // clang-format on

    const struct test_vectors_t vectors[6] = {
        { "Plaintext packet with 1-byte header extension", srtp_1bytehdrext_ref,
          srtp_1bytehdrext_cryptex },
        { "Plaintext packet with 2-byte header extension", srtp_2bytehdrext_ref,
          srtp_2bytehdrext_cryptex },
        { "Plaintext packet with 1-byte header extension and CSRC fields",
          srtp_1bytehdrext_cc_ref, srtp_1bytehdrext_cc_cryptex },
        { "Plaintext packet with 2-byte header extension and CSRC fields",
          srtp_2bytehdrext_cc_ref, srtp_2bytehdrext_cc_cryptex },
        { "Plaintext packet with empty 1-byte header extension and CSRC fields",
          srtp_1byte_empty_hdrext_cc_ref, srtp_1byte_empty_hdrext_cc_cryptex },
        { "Plaintext packet with empty 2-byte header extension and CSRC fields",
          srtp_2byte_empty_hdrext_cc_ref, srtp_2byte_empty_hdrext_cc_cryptex },
    };
    const size_t num_vectors = sizeof(vectors) / sizeof(vectors[0]);

    srtp_t srtp_snd, srtp_recv;
    size_t len, ref_len, enc_len;
    srtp_policy_t policy;

    /*
     * create a session with a single stream using the default srtp
     * policy and with the SSRC value 0xcafebabe
     */
    memset(&policy, 0, sizeof(policy));
    srtp_crypto_policy_set_rtp_default(&policy.rtp);
    srtp_crypto_policy_set_rtcp_default(&policy.rtcp);
    policy.ssrc.type = ssrc_specific;
    policy.ssrc.value = 0xcafebabe;
    policy.key = test_key;
    policy.window_size = 128;
    policy.allow_repeat_tx = 0;
    policy.use_cryptex = true;
    policy.next = NULL;

    for (size_t i = 0; i < num_vectors; ++i) {
        uint8_t packet[1400];
        uint8_t reference[1400];
        uint8_t ciphertext[1400];

        /* Initialize reference test vectors */
        ref_len = hex_string_to_octet_string(reference, vectors[i].plaintext,
                                             sizeof(reference)) /
                  2;
        enc_len = hex_string_to_octet_string(ciphertext, vectors[i].ciphertext,
                                             sizeof(ciphertext)) /
                  2;

        /* Initialize test packet */
        len = ref_len;
        memcpy(packet, reference, len);
        printf("%s\n", vectors[i].name);
        /*
         * protect plaintext, then compare with ciphertext
         */
        debug_print(mod_driver, "test vector: %s\n", vectors[i].name);

        CHECK_OK(srtp_create(&srtp_snd, &policy));

        CHECK_OK(call_srtp_protect(srtp_snd, packet, &len, 0));
        CHECK(len == enc_len);

        debug_print(mod_driver, "ciphertext:\n  %s",
                    octet_string_hex_string(packet, len));
        debug_print(mod_driver, "ciphertext reference:\n  %s",
                    octet_string_hex_string(ciphertext, len));

        CHECK_BUFFER_EQUAL(packet, ciphertext, len);

        CHECK_OK(srtp_dealloc(srtp_snd));

        CHECK_OK(srtp_create(&srtp_recv, &policy));

        /*
         * unprotect ciphertext, then compare with plaintext
         */
        CHECK_OK(call_srtp_unprotect(srtp_recv, packet, &len));
        CHECK(len == ref_len);

        CHECK_BUFFER_EQUAL(packet, reference, len);

        CHECK_OK(srtp_dealloc(srtp_recv));
    }

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_test_cryptex_csrc_but_no_extension_header(void)
{
    // clang-format off
    /* Plaintext packet with no header extension but CSRC fields. */
    const char *srtp_cc_ref =
        "820f1238"
        "decafbad"
        "cafebabe"
        "0001e240"
        "0000b26e"
        "abababab"
        "abababab"
        "abababab"
        "abababab";
    // clang-format on

    /*
     * create a session with a single stream using the default srtp
     * policy and with the SSRC value 0xcafebabe
     */
    srtp_policy_t policy;
    memset(&policy, 0, sizeof(policy));
    srtp_crypto_policy_set_rtp_default(&policy.rtp);
    srtp_crypto_policy_set_rtcp_default(&policy.rtcp);
    policy.ssrc.type = ssrc_specific;
    policy.ssrc.value = 0xcafebabe;
    policy.key = test_key;
    policy.window_size = 128;
    policy.allow_repeat_tx = 0;
    policy.use_cryptex = true;
    policy.next = NULL;

    srtp_t srtp_snd;
    CHECK_OK(srtp_create(&srtp_snd, &policy));

    uint8_t packet[1400];
    size_t packet_len =
        hex_string_to_octet_string(packet, srtp_cc_ref, sizeof(packet)) / 2;

    CHECK_RETURN(call_srtp_protect(srtp_snd, packet, &packet_len, 0),
                 srtp_err_status_cryptex_err);

    CHECK_OK(srtp_dealloc(srtp_snd));

    return srtp_err_status_ok;
}

#ifdef GCM
/*
 * srtp_validate_gcm() verifies the correctness of libsrtp by comparing
 * an computed packet against the known ciphertext for the plaintext.
 */
srtp_err_status_t srtp_validate_gcm(void)
{
    // clang-format off
    uint8_t rtp_plaintext_ref[28] = {
        0x80, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab
    };
    uint8_t rtp_plaintext[44] = {
        0x80, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    uint8_t srtp_ciphertext[44] = {
        0x80, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xc5, 0x00, 0x2e, 0xde,
        0x04, 0xcf, 0xdd, 0x2e, 0xb9, 0x11, 0x59, 0xe0,
        0x88, 0x0a, 0xa0, 0x6e, 0xd2, 0x97, 0x68, 0x26,
        0xf7, 0x96, 0xb2, 0x01, 0xdf, 0x31, 0x31, 0xa1,
        0x27, 0xe8, 0xa3, 0x92
    };
    uint8_t rtcp_plaintext_ref[24] = {
        0x81, 0xc8, 0x00, 0x0b, 0xca, 0xfe, 0xba, 0xbe,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
    };
    uint8_t rtcp_plaintext[44] = {
        0x81, 0xc8, 0x00, 0x0b, 0xca, 0xfe, 0xba, 0xbe,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00
    };
    uint8_t srtcp_ciphertext[44] = {
        0x81, 0xc8, 0x00, 0x0b, 0xca, 0xfe, 0xba, 0xbe,
        0xc9, 0x8b, 0x8b, 0x5d, 0xf0, 0x39, 0x2a, 0x55,
        0x85, 0x2b, 0x6c, 0x21, 0xac, 0x8e, 0x70, 0x25,
        0xc5, 0x2c, 0x6f, 0xbe, 0xa2, 0xb3, 0xb4, 0x46,
        0xea, 0x31, 0x12, 0x3b, 0xa8, 0x8c, 0xe6, 0x1e,
        0x80, 0x00, 0x00, 0x01
    };
    // clang-format on

    srtp_t srtp_snd, srtp_recv;
    srtp_err_status_t status;
    size_t len;
    srtp_policy_t policy;

    /*
     * create a session with a single stream using the default srtp
     * policy and with the SSRC value 0xcafebabe
     */
    memset(&policy, 0, sizeof(policy));
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtp);
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtcp);
    policy.ssrc.type = ssrc_specific;
    policy.ssrc.value = 0xcafebabe;
    policy.key = test_key_gcm;
    policy.window_size = 128;
    policy.allow_repeat_tx = false;
    policy.next = NULL;

    status = srtp_create(&srtp_snd, &policy);
    if (status) {
        return status;
    }

    /*
     * protect plaintext rtp, then compare with srtp ciphertext
     */
    len = 28;
    status = call_srtp_protect(srtp_snd, rtp_plaintext, &len, 0);
    if (status || (len != 44)) {
        return srtp_err_status_fail;
    }

    debug_print(mod_driver, "srtp ciphertext:\n  %s",
                octet_string_hex_string(rtp_plaintext, len));
    debug_print(mod_driver, "srtp ciphertext reference:\n  %s",
                octet_string_hex_string(srtp_ciphertext, len));

    if (!srtp_octet_string_equal(rtp_plaintext, srtp_ciphertext, len)) {
        return srtp_err_status_fail;
    }

    /*
     * protect plaintext rtcp, then compare with srtcp ciphertext
     */
    len = 24;
    status = call_srtp_protect_rtcp(srtp_snd, rtcp_plaintext, &len, 0);
    if (status || (len != 44)) {
        return srtp_err_status_fail;
    }

    debug_print(mod_driver, "srtcp ciphertext:\n  %s",
                octet_string_hex_string(rtcp_plaintext, len));
    debug_print(mod_driver, "srtcp ciphertext reference:\n  %s",
                octet_string_hex_string(srtcp_ciphertext, len));

    if (!srtp_octet_string_equal(rtcp_plaintext, srtcp_ciphertext, len)) {
        return srtp_err_status_fail;
    }

    /*
     * create a receiver session context comparable to the one created
     * above - we need to do this so that the replay checking doesn't
     * complain
     */
    status = srtp_create(&srtp_recv, &policy);
    if (status) {
        return status;
    }

    /*
     * unprotect srtp ciphertext, then compare with rtp plaintext
     */
    len = 44;
    status = call_srtp_unprotect(srtp_recv, srtp_ciphertext, &len);
    if (status || (len != 28)) {
        return status;
    }

    if (!srtp_octet_string_equal(srtp_ciphertext, rtp_plaintext_ref, len)) {
        return srtp_err_status_fail;
    }

    /*
     * unprotect srtcp ciphertext, then compare with rtcp plaintext
     */
    len = 44;
    status = call_srtp_unprotect_rtcp(srtp_recv, srtcp_ciphertext, &len);
    if (status || (len != 24)) {
        return status;
    }

    debug_print(mod_driver, "srtcp plain:\n  %s",
                octet_string_hex_string(srtcp_ciphertext, len));
    debug_print(mod_driver, "srtcp plain reference:\n  %s",
                octet_string_hex_string(rtcp_plaintext_ref,
                                        sizeof(rtcp_plaintext_ref)));

    if (!srtp_octet_string_equal(srtcp_ciphertext, rtcp_plaintext_ref, len)) {
        return srtp_err_status_fail;
    }

    status = srtp_dealloc(srtp_snd);
    if (status) {
        return status;
    }

    status = srtp_dealloc(srtp_recv);
    if (status) {
        return status;
    }

    return srtp_err_status_ok;
}

/*
 * srtp_validate_gcm() verifies the correctness of libsrtp by comparing
 * an computed packet against the known ciphertext for the plaintext.
 */
srtp_err_status_t srtp_validate_gcm_cryptex(void)
{
    // clang-format off
    unsigned char test_key_gcm_cryptex[28] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
        0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
        0xa8, 0xa9, 0xaa, 0xab
    };

    /* Plaintext packet with 1-byte header extension */
    const char *srtp_1bytehdrext_ref =
        "900f1235"
        "decafbad"
        "cafebabe"
        "bede0001"
        "51000200"
        "abababab"
        "abababab"
        "abababab"
        "abababab";

    /* GCM Ciphertext packet with 1-byte header extension */
    const char *srtp_1bytehdrext_cryptex_gcm =
        "900f1235"
        "decafbad"
        "cafebabe"
        "c0de0001"
        "39972dc9"
        "572c4d99"
        "e8fc355d"
        "e743fb2e"
        "94f9d8ff"
        "54e72f41"
        "93bbc5c7"
        "4ffab0fa"
        "9fa0fbeb";

    /* Plaintext packet with 2-byte header extension */
    const char *srtp_2bytehdrext_ref =
        "900f1236"
        "decafbad"
        "cafebabe"
        "10000001"
        "05020002"
        "abababab"
        "abababab"
        "abababab"
        "abababab";

    /* GCM Ciphertext packet with 2-byte header extension */
    const char *srtp_2bytehdrext_cryptex_gcm =
        "900f1236"
        "decafbad"
        "cafebabe"
        "c2de0001"
        "bb75a4c5"
        "45cd1f41"
        "3bdb7daa"
        "2b1e3263"
        "de313667"
        "c9632490"
        "81b35a65"
        "f5cb6c88"
        "b394235f";

    /* Plaintext packet with 1-byte header extension and CSRC fields. */
    const char *srtp_1bytehdrext_cc_ref =
        "920f1238"
        "decafbad"
        "cafebabe"
        "0001e240"
        "0000b26e"
        "bede0001"
        "51000200"
        "abababab"
        "abababab"
        "abababab"
        "abababab";

    const char *srtp_1bytehdrext_cc_cryptex_gcm =
        "920f1238"
        "decafbad"
        "cafebabe"
        "63bbccc4"
        "a7f695c4"
        "c0de0001"
        "8ad7c71f"
        "ac70a80c"
        "92866b4c"
        "6ba98546"
        "ef913586"
        "e95ffaaf"
        "fe956885"
        "bb0647a8"
        "bc094ac8";


    /* Plaintext packet with 2-byte header extension and CSRC fields. */
    const char *srtp_2bytehdrext_cc_ref =
        "920f1239"
        "decafbad"
        "cafebabe"
        "0001e240"
        "0000b26e"
        "10000001"
        "05020002"
        "abababab"
        "abababab"
        "abababab"
        "abababab";

    const char *srtp_2bytehdrext_cc_cryptex_gcm =
        "920f1239"
        "decafbad"
        "cafebabe"
        "3680524f"
        "8d312b00"
        "c2de0001"
        "c78d1200"
        "38422bc1"
        "11a7187a"
        "18246f98"
        "0c059cc6"
        "bc9df8b6"
        "26394eca"
        "344e4b05"
        "d80fea83";

    /* Plaintext packet with empty 1-byte header extension and CSRC fields. */
    const char *srtp_1byte_empty_hdrext_cc_ref =
        "920f123a"
        "decafbad"
        "cafebabe"
        "0001e240"
        "0000b26e"
        "bede0000"
        "abababab"
        "abababab"
        "abababab"
        "abababab";

    const char *srtp_1byte_empty_hdrext_cc_cryptex_gcm =
        "920f123a"
        "decafbad"
        "cafebabe"
        "15b6bb43"
        "37906fff"
        "c0de0000"
        "b7b96453"
        "7a2b03ab"
        "7ba5389c"
        "e9331712"
        "6b5d974d"
        "f30c6884"
        "dcb651c5"
        "e120c1da";

    /* Plaintext packet with empty 2-byte header extension and CSRC fields. */
    const char *srtp_2byte_empty_hdrext_cc_ref =
        "920f123b"
        "decafbad"
        "cafebabe"
        "0001e240"
        "0000b26e"
        "10000000"
        "abababab"
        "abababab"
        "abababab"
        "abababab";

    const char *srtp_2byte_empty_hdrext_cc_cryptex_gcm =
        "920f123b"
        "decafbad"
        "cafebabe"
        "dcb38c9e"
        "48bf95f4"
        "c2de0000"
        "61ee432c"
        "f9203170"
        "76613258"
        "d3ce4236"
        "c06ac429"
        "681ad084"
        "13512dc9"
        "8b5207d8";
    // clang-format on

    const struct test_vectors_t vectors[6] = {
        { "Plaintext packet with 1-byte header extension", srtp_1bytehdrext_ref,
          srtp_1bytehdrext_cryptex_gcm },
        { "Plaintext packet with 2-byte header extension", srtp_2bytehdrext_ref,
          srtp_2bytehdrext_cryptex_gcm },
        { "Plaintext packet with 1-byte header extension and CSRC fields",
          srtp_1bytehdrext_cc_ref, srtp_1bytehdrext_cc_cryptex_gcm },
        { "Plaintext packet with 2-byte header extension and CSRC fields",
          srtp_2bytehdrext_cc_ref, srtp_2bytehdrext_cc_cryptex_gcm },
        { "Plaintext packet with empty 1-byte header extension and CSRC fields",
          srtp_1byte_empty_hdrext_cc_ref,
          srtp_1byte_empty_hdrext_cc_cryptex_gcm },
        { "Plaintext packet with empty 2-byte header extension and CSRC fields",
          srtp_2byte_empty_hdrext_cc_ref,
          srtp_2byte_empty_hdrext_cc_cryptex_gcm },
    };
    const size_t num_vectors = sizeof(vectors) / sizeof(vectors[0]);

    srtp_t srtp_snd, srtp_recv;
    size_t len, ref_len, enc_len;
    srtp_policy_t policy;

    /*
     * create a session with a single stream using the default srtp
     * policy and with the SSRC value 0xcafebabe
     */
    memset(&policy, 0, sizeof(policy));
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtp);
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtcp);
    policy.ssrc.type = ssrc_specific;
    policy.ssrc.value = 0xcafebabe;
    policy.key = test_key_gcm_cryptex;
    policy.window_size = 128;
    policy.allow_repeat_tx = 0;
    policy.use_cryptex = true;
    policy.next = NULL;

    CHECK_OK(srtp_create(&srtp_snd, &policy));

    for (size_t i = 0; i < num_vectors; ++i) {
        uint8_t packet[1400];
        uint8_t reference[1400];
        uint8_t ciphertext[1400];

        /* Initialize reference test vectors */
        ref_len = hex_string_to_octet_string(reference, vectors[i].plaintext,
                                             sizeof(reference)) /
                  2;
        enc_len = hex_string_to_octet_string(ciphertext, vectors[i].ciphertext,
                                             sizeof(ciphertext)) /
                  2;

        /* Initialize test packet */
        len = ref_len;
        memcpy(packet, reference, len);
        printf("%s\n", vectors[i].name);
        /*
         * protect plaintext, then compare with ciphertext
         */
        debug_print(mod_driver, "test vector: %s\n", vectors[i].name);

        const srtp_hdr_t *hdr = (const srtp_hdr_t *)reference;
        if (use_srtp_not_in_place_io_api && hdr->cc) {
            // the combination of cryptex, cc, GCM & not inplace is not
            // supported
            CHECK_RETURN(call_srtp_protect(srtp_snd, packet, &len, 0),
                         srtp_err_status_cryptex_err);
            continue;
        }
        CHECK_OK(call_srtp_protect(srtp_snd, packet, &len, 0));
        CHECK(len == enc_len);

        debug_print(mod_driver, "ciphertext:\n  %s",
                    octet_string_hex_string(packet, len));
        debug_print(mod_driver, "ciphertext reference:\n  %s",
                    octet_string_hex_string(ciphertext, len));

        CHECK_BUFFER_EQUAL(packet, ciphertext, len);

        /*
         * create a receiver session context comparable to the one created
         * above - we need to do this so that the replay checking doesn't
         * complain
         */
        CHECK_OK(srtp_create(&srtp_recv, &policy));

        /*
         * unprotect ciphertext, then compare with plaintext
         */
        CHECK_OK(call_srtp_unprotect(srtp_recv, packet, &len));
        CHECK(len == ref_len);

        CHECK_BUFFER_EQUAL(packet, reference, len);

        CHECK_OK(srtp_dealloc(srtp_recv));
    }

    CHECK_OK(srtp_dealloc(srtp_snd));

    return srtp_err_status_ok;
}

#endif

/*
 * Test vectors taken from RFC 6904, Appendix A
 */
srtp_err_status_t srtp_validate_encrypted_extensions_headers(void)
{
    // clang-format off
    uint8_t test_key_ext_headers[30] = {
        0xe1, 0xf9, 0x7a, 0x0d, 0x3e, 0x01, 0x8b, 0xe0,
        0xd6, 0x4f, 0xa3, 0x2c, 0x06, 0xde, 0x41, 0x39,
        0x0e, 0xc6, 0x75, 0xad, 0x49, 0x8a, 0xfe, 0xeb,
        0xb6, 0x96, 0x0b, 0x3a, 0xab, 0xe6
    };
    uint8_t srtp_plaintext_ref[56] = {
        0x90, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xBE, 0xDE, 0x00, 0x06,
        0x17, 0x41, 0x42, 0x73, 0xA4, 0x75, 0x26, 0x27,
        0x48, 0x22, 0x00, 0x00, 0xC8, 0x30, 0x8E, 0x46,
        0x55, 0x99, 0x63, 0x86, 0xB3, 0x95, 0xFB, 0x00,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab
    };
    uint8_t srtp_plaintext[66] = {
        0x90, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xBE, 0xDE, 0x00, 0x06,
        0x17, 0x41, 0x42, 0x73, 0xA4, 0x75, 0x26, 0x27,
        0x48, 0x22, 0x00, 0x00, 0xC8, 0x30, 0x8E, 0x46,
        0x55, 0x99, 0x63, 0x86, 0xB3, 0x95, 0xFB, 0x00,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00
    };
    uint8_t srtp_ciphertext[66] = {
        0x90, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xBE, 0xDE, 0x00, 0x06,
        0x17, 0x58, 0x8A, 0x92, 0x70, 0xF4, 0xE1, 0x5E,
        0x1C, 0x22, 0x00, 0x00, 0xC8, 0x30, 0x95, 0x46,
        0xA9, 0x94, 0xF0, 0xBC, 0x54, 0x78, 0x97, 0x00,
        0x4e, 0x55, 0xdc, 0x4c, 0xe7, 0x99, 0x78, 0xd8,
        0x8c, 0xa4, 0xd2, 0x15, 0x94, 0x9d, 0x24, 0x02,
        0x5a, 0x46, 0xb3, 0xca, 0x35, 0xc5, 0x35, 0xa8,
        0x91, 0xc7
    };
    // clang-format on

    srtp_t srtp_snd, srtp_recv;
    srtp_err_status_t status;
    size_t len;
    srtp_policy_t policy;
    uint8_t headers[3] = { 1, 3, 4 };

    /*
     * create a session with a single stream using the default srtp
     * policy and with the SSRC value 0xcafebabe
     */
    memset(&policy, 0, sizeof(policy));
    srtp_crypto_policy_set_rtp_default(&policy.rtp);
    srtp_crypto_policy_set_rtcp_default(&policy.rtcp);
    policy.ssrc.type = ssrc_specific;
    policy.ssrc.value = 0xcafebabe;
    policy.key = test_key_ext_headers;
    policy.window_size = 128;
    policy.allow_repeat_tx = false;
    policy.enc_xtn_hdr = headers;
    policy.enc_xtn_hdr_count = sizeof(headers) / sizeof(headers[0]);
    policy.next = NULL;

    status = srtp_create(&srtp_snd, &policy);
    if (status) {
        return status;
    }

    /*
     * protect plaintext, then compare with ciphertext
     */
    len = sizeof(srtp_plaintext_ref);
    status = call_srtp_protect(srtp_snd, srtp_plaintext, &len, 0);
    if (status || (len != sizeof(srtp_plaintext))) {
        return srtp_err_status_fail;
    }

    debug_print(mod_driver, "ciphertext:\n  %s",
                srtp_octet_string_hex_string(srtp_plaintext, len));
    debug_print(mod_driver, "ciphertext reference:\n  %s",
                srtp_octet_string_hex_string(srtp_ciphertext, len));

    if (!srtp_octet_string_equal(srtp_plaintext, srtp_ciphertext, len)) {
        return srtp_err_status_fail;
    }

    /*
     * create a receiver session context comparable to the one created
     * above - we need to do this so that the replay checking doesn't
     * complain
     */
    status = srtp_create(&srtp_recv, &policy);
    if (status) {
        return status;
    }

    /*
     * unprotect ciphertext, then compare with plaintext
     */
    status = call_srtp_unprotect(srtp_recv, srtp_ciphertext, &len);
    if (status) {
        return status;
    } else if (len != sizeof(srtp_plaintext_ref)) {
        return srtp_err_status_fail;
    }

    if (!srtp_octet_string_equal(srtp_ciphertext, srtp_plaintext_ref, len)) {
        return srtp_err_status_fail;
    }

    status = srtp_dealloc(srtp_snd);
    if (status) {
        return status;
    }

    status = srtp_dealloc(srtp_recv);
    if (status) {
        return status;
    }

    return srtp_err_status_ok;
}

#ifdef GCM
/*
 * Headers of test vectors taken from RFC 6904, Appendix A
 */
srtp_err_status_t srtp_validate_encrypted_extensions_headers_gcm(void)
{
    // clang-format off
    uint8_t test_key_ext_headers[30] = {
        0xe1, 0xf9, 0x7a, 0x0d, 0x3e, 0x01, 0x8b, 0xe0,
        0xd6, 0x4f, 0xa3, 0x2c, 0x06, 0xde, 0x41, 0x39,
        0x0e, 0xc6, 0x75, 0xad, 0x49, 0x8a, 0xfe, 0xeb,
        0xb6, 0x96, 0x0b, 0x3a, 0xab, 0xe6
    };
    uint8_t srtp_plaintext_ref[56] = {
        0x90, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xBE, 0xDE, 0x00, 0x06,
        0x17, 0x41, 0x42, 0x73, 0xA4, 0x75, 0x26, 0x27,
        0x48, 0x22, 0x00, 0x00, 0xC8, 0x30, 0x8E, 0x46,
        0x55, 0x99, 0x63, 0x86, 0xB3, 0x95, 0xFB, 0x00,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab
    };
    uint8_t srtp_plaintext[72] = {
        0x90, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xBE, 0xDE, 0x00, 0x06,
        0x17, 0x41, 0x42, 0x73, 0xA4, 0x75, 0x26, 0x27,
        0x48, 0x22, 0x00, 0x00, 0xC8, 0x30, 0x8E, 0x46,
        0x55, 0x99, 0x63, 0x86, 0xB3, 0x95, 0xFB, 0x00,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    uint8_t srtp_ciphertext[72] = {
        0x90, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xBE, 0xDE, 0x00, 0x06,
        0x17, 0x12, 0xe0, 0x20, 0x5b, 0xfa, 0x94, 0x9b,
        0x1C, 0x22, 0x00, 0x00, 0xC8, 0x30, 0xbb, 0x46,
        0x73, 0x27, 0x78, 0xd9, 0x92, 0x9a, 0xab, 0x00,
        0x0e, 0xca, 0x0c, 0xf9, 0x5e, 0xe9, 0x55, 0xb2,
        0x6c, 0xd3, 0xd2, 0x88, 0xb4, 0x9f, 0x6c, 0xa9,
        0xf4, 0xb1, 0xb7, 0x59, 0x71, 0x9e, 0xb5, 0xbc,
        0x11, 0x3b, 0x9f, 0xf1, 0xd4, 0x0c, 0xd2, 0x5a
    };
    // clang-format on

    srtp_t srtp_snd, srtp_recv;
    srtp_err_status_t status;
    size_t len;
    srtp_policy_t policy;
    uint8_t headers[3] = { 1, 3, 4 };

    /*
     * create a session with a single stream using the default srtp
     * policy and with the SSRC value 0xcafebabe
     */
    memset(&policy, 0, sizeof(policy));
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtp);
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtcp);
    policy.ssrc.type = ssrc_specific;
    policy.ssrc.value = 0xcafebabe;
    policy.key = test_key_ext_headers;
    policy.window_size = 128;
    policy.allow_repeat_tx = false;
    policy.enc_xtn_hdr = headers;
    policy.enc_xtn_hdr_count = sizeof(headers) / sizeof(headers[0]);
    policy.next = NULL;

    status = srtp_create(&srtp_snd, &policy);
    if (status) {
        return status;
    }

    /*
     * protect plaintext, then compare with ciphertext
     */
    len = sizeof(srtp_plaintext_ref);
    status = call_srtp_protect(srtp_snd, srtp_plaintext, &len, 0);
    if (status || (len != sizeof(srtp_plaintext))) {
        return srtp_err_status_fail;
    }

    debug_print(mod_driver, " ? ciphertext:\n  %s",
                srtp_octet_string_hex_string(srtp_plaintext, len));
    debug_print(mod_driver, " ? ciphertext reference:\n  %s",
                srtp_octet_string_hex_string(srtp_ciphertext, len));

    if (!srtp_octet_string_equal(srtp_plaintext, srtp_ciphertext, len)) {
        return srtp_err_status_fail;
    }

    /*
     * create a receiver session context comparable to the one created
     * above - we need to do this so that the replay checking doesn't
     * complain
     */
    status = srtp_create(&srtp_recv, &policy);
    if (status) {
        return status;
    }

    /*
     * unprotect ciphertext, then compare with plaintext
     */
    status = call_srtp_unprotect(srtp_recv, srtp_ciphertext, &len);
    if (status) {
        return status;
    } else if (len != sizeof(srtp_plaintext_ref)) {
        return srtp_err_status_fail;
    }

    if (!srtp_octet_string_equal(srtp_ciphertext, srtp_plaintext_ref, len)) {
        return srtp_err_status_fail;
    }

    status = srtp_dealloc(srtp_snd);
    if (status) {
        return status;
    }

    status = srtp_dealloc(srtp_recv);
    if (status) {
        return status;
    }

    return srtp_err_status_ok;
}
#endif

/*
 * srtp_validate_aes_256() verifies the correctness of libsrtp by comparing
 * some computed packets against some pre-computed reference values.
 * These packets were made with the AES-CM-256/HMAC-SHA-1-80 policy.
 */

srtp_err_status_t srtp_validate_aes_256(void)
{
    // clang-format off
    uint8_t aes_256_test_key[46] = {
        0xf0, 0xf0, 0x49, 0x14, 0xb5, 0x13, 0xf2, 0x76,
        0x3a, 0x1b, 0x1f, 0xa1, 0x30, 0xf1, 0x0e, 0x29,
        0x98, 0xf6, 0xf6, 0xe4, 0x3e, 0x43, 0x09, 0xd1,
        0xe6, 0x22, 0xa0, 0xe3, 0x32, 0xb9, 0xf1, 0xb6,

        0x3b, 0x04, 0x80, 0x3d, 0xe5, 0x1e, 0xe7, 0xc9,
        0x64, 0x23, 0xab, 0x5b, 0x78, 0xd2
    };
    uint8_t srtp_plaintext_ref[28] = {
        0x80, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab
    };
    uint8_t srtp_plaintext[38] = {
        0x80, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab, 0xab,
        0xab, 0xab, 0xab, 0xab, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    uint8_t srtp_ciphertext[38] = {
        0x80, 0x0f, 0x12, 0x34, 0xde, 0xca, 0xfb, 0xad,
        0xca, 0xfe, 0xba, 0xbe, 0xf1, 0xd9, 0xde, 0x17,
        0xff, 0x25, 0x1f, 0xf1, 0xaa, 0x00, 0x77, 0x74,
        0xb0, 0xb4, 0xb4, 0x0d, 0xa0, 0x8d, 0x9d, 0x9a,
        0x5b, 0x3a, 0x55, 0xd8, 0x87, 0x3b
    };
    // clang-format on

    srtp_t srtp_snd, srtp_recv;
    srtp_err_status_t status;
    size_t len;
    srtp_policy_t policy;

    /*
     * create a session with a single stream using the default srtp
     * policy and with the SSRC value 0xcafebabe
     */
    memset(&policy, 0, sizeof(policy));
    srtp_crypto_policy_set_aes_cm_256_hmac_sha1_80(&policy.rtp);
    srtp_crypto_policy_set_aes_cm_256_hmac_sha1_80(&policy.rtcp);
    policy.ssrc.type = ssrc_specific;
    policy.ssrc.value = 0xcafebabe;
    policy.key = aes_256_test_key;
    policy.window_size = 128;
    policy.allow_repeat_tx = false;
    policy.next = NULL;

    status = srtp_create(&srtp_snd, &policy);
    if (status) {
        return status;
    }

    /*
     * protect plaintext, then compare with ciphertext
     */
    len = 28;
    status = call_srtp_protect(srtp_snd, srtp_plaintext, &len, 0);
    if (status || (len != 38)) {
        return srtp_err_status_fail;
    }

    debug_print(mod_driver, "ciphertext:\n  %s",
                octet_string_hex_string(srtp_plaintext, len));
    debug_print(mod_driver, "ciphertext reference:\n  %s",
                octet_string_hex_string(srtp_ciphertext, len));

    if (!srtp_octet_string_equal(srtp_plaintext, srtp_ciphertext, len)) {
        return srtp_err_status_fail;
    }

    /*
     * create a receiver session context comparable to the one created
     * above - we need to do this so that the replay checking doesn't
     * complain
     */
    status = srtp_create(&srtp_recv, &policy);
    if (status) {
        return status;
    }

    /*
     * unprotect ciphertext, then compare with plaintext
     */
    status = call_srtp_unprotect(srtp_recv, srtp_ciphertext, &len);
    if (status || (len != 28)) {
        return status;
    }

    if (!srtp_octet_string_equal(srtp_ciphertext, srtp_plaintext_ref, len)) {
        return srtp_err_status_fail;
    }

    status = srtp_dealloc(srtp_snd);
    if (status) {
        return status;
    }

    status = srtp_dealloc(srtp_recv);
    if (status) {
        return status;
    }

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_create_big_policy(srtp_policy_t **list)
{
    extern const srtp_policy_t *policy_array[];
    srtp_policy_t *p = NULL;
    srtp_policy_t *tmp;
    int i = 0;
    uint32_t ssrc = 0;

    /* sanity checking */
    if ((list == NULL) || (policy_array[0] == NULL)) {
        return srtp_err_status_bad_param;
    }

    /*
     * loop over policy list, mallocing a new list and copying values
     * into it (and incrementing the SSRC value as we go along)
     */
    tmp = NULL;
    while (policy_array[i] != NULL) {
        p = (srtp_policy_t *)malloc(sizeof(srtp_policy_t));
        if (p == NULL) {
            return srtp_err_status_bad_param;
        }
        memcpy(p, policy_array[i], sizeof(srtp_policy_t));
        p->ssrc.type = ssrc_specific;
        p->ssrc.value = ssrc++;
        p->next = tmp;
        tmp = p;
        i++;
    }
    *list = p;

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_dealloc_big_policy(srtp_policy_t *list)
{
    srtp_policy_t *p, *next;

    for (p = list; p != NULL; p = next) {
        next = p->next;
        free(p);
    }

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_test_empty_payload(void)
{
    srtp_t srtp_snd, srtp_recv;
    srtp_err_status_t status;
    size_t len;
    srtp_policy_t policy;
    uint8_t *mesg;

    /*
     * create a session with a single stream using the default srtp
     * policy and with the SSRC value 0xcafebabe
     */
    memset(&policy, 0, sizeof(policy));
    srtp_crypto_policy_set_rtp_default(&policy.rtp);
    srtp_crypto_policy_set_rtcp_default(&policy.rtcp);
    policy.ssrc.type = ssrc_specific;
    policy.ssrc.value = 0xcafebabe;
    policy.key = test_key;
    policy.window_size = 128;
    policy.allow_repeat_tx = false;
    policy.next = NULL;

    status = srtp_create(&srtp_snd, &policy);
    if (status) {
        return status;
    }

    mesg =
        create_rtp_test_packet(0, policy.ssrc.value, 1, 1, false, &len, NULL);
    if (mesg == NULL) {
        return srtp_err_status_fail;
    }

    status = call_srtp_protect(srtp_snd, mesg, &len, 0);
    if (status) {
        return status;
    } else if (len != 12 + 10) {
        return srtp_err_status_fail;
    }

    /*
     * create a receiver session context comparable to the one created
     * above - we need to do this so that the replay checking doesn't
     * complain
     */
    status = srtp_create(&srtp_recv, &policy);
    if (status) {
        return status;
    }

    /*
     * unprotect ciphertext, then compare with plaintext
     */
    status = call_srtp_unprotect(srtp_recv, mesg, &len);
    if (status) {
        return status;
    } else if (len != 12) {
        return srtp_err_status_fail;
    }

    status = srtp_dealloc(srtp_snd);
    if (status) {
        return status;
    }

    status = srtp_dealloc(srtp_recv);
    if (status) {
        return status;
    }

    free(mesg);

    return srtp_err_status_ok;
}

#ifdef GCM
srtp_err_status_t srtp_test_empty_payload_gcm(void)
{
    srtp_t srtp_snd, srtp_recv;
    srtp_err_status_t status;
    size_t len;
    srtp_policy_t policy;
    uint8_t *mesg;

    /*
     * create a session with a single stream using the default srtp
     * policy and with the SSRC value 0xcafebabe
     */
    memset(&policy, 0, sizeof(policy));
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtp);
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtcp);
    policy.ssrc.type = ssrc_specific;
    policy.ssrc.value = 0xcafebabe;
    policy.key = test_key;
    policy.window_size = 128;
    policy.allow_repeat_tx = false;
    policy.next = NULL;

    status = srtp_create(&srtp_snd, &policy);
    if (status) {
        return status;
    }

    mesg =
        create_rtp_test_packet(0, policy.ssrc.value, 1, 1, false, &len, NULL);
    if (mesg == NULL) {
        return srtp_err_status_fail;
    }

    status = call_srtp_protect(srtp_snd, mesg, &len, 0);
    if (status) {
        return status;
    } else if (len != 12 + 16) {
        return srtp_err_status_fail;
    }

    /*
     * create a receiver session context comparable to the one created
     * above - we need to do this so that the replay checking doesn't
     * complain
     */
    status = srtp_create(&srtp_recv, &policy);
    if (status) {
        return status;
    }

    /*
     * unprotect ciphertext, then compare with plaintext
     */
    status = call_srtp_unprotect(srtp_recv, mesg, &len);
    if (status) {
        return status;
    } else if (len != 12) {
        return srtp_err_status_fail;
    }

    status = srtp_dealloc(srtp_snd);
    if (status) {
        return status;
    }

    status = srtp_dealloc(srtp_recv);
    if (status) {
        return status;
    }

    free(mesg);

    return srtp_err_status_ok;
}
#endif // GCM

srtp_err_status_t srtp_test_remove_stream(void)
{
    srtp_err_status_t status;
    srtp_policy_t *policy_list, policy;
    srtp_t session;
    srtp_stream_t stream;

    /*
     * srtp_get_stream() is a libSRTP internal function that we declare
     * here so that we can use it to verify the correct operation of the
     * library
     */
    extern srtp_stream_t srtp_get_stream(srtp_t srtp, uint32_t ssrc);

    status = srtp_create_big_policy(&policy_list);
    if (status) {
        return status;
    }

    status = srtp_create(&session, policy_list);
    if (status) {
        return status;
    }

    /*
     * check for false positives by trying to remove a stream that's not
     * in the session
     */
    status = srtp_stream_remove(session, 0xaaaaaaaa);
    if (status != srtp_err_status_no_ctx) {
        return srtp_err_status_fail;
    }

    /*
     * check for false negatives by removing stream 0x1, then
     * searching for streams 0x0 and 0x2
     */
    status = srtp_stream_remove(session, 0x1);
    if (status != srtp_err_status_ok) {
        return srtp_err_status_fail;
    }
    stream = srtp_get_stream(session, htonl(0x0));
    if (stream == NULL) {
        return srtp_err_status_fail;
    }
    stream = srtp_get_stream(session, htonl(0x2));
    if (stream == NULL) {
        return srtp_err_status_fail;
    }

    status = srtp_dealloc(session);
    if (status != srtp_err_status_ok) {
        return status;
    }

    status = srtp_dealloc_big_policy(policy_list);
    if (status != srtp_err_status_ok) {
        return status;
    }

    /* Now test adding and removing a single stream */
    memset(&policy, 0, sizeof(policy));
    srtp_crypto_policy_set_rtp_default(&policy.rtp);
    srtp_crypto_policy_set_rtcp_default(&policy.rtcp);
    policy.ssrc.type = ssrc_specific;
    policy.ssrc.value = 0xcafebabe;
    policy.key = test_key;
    policy.window_size = 128;
    policy.allow_repeat_tx = false;
    policy.next = NULL;

    status = srtp_create(&session, NULL);
    if (status != srtp_err_status_ok) {
        return status;
    }

    status = srtp_stream_add(session, &policy);
    if (status != srtp_err_status_ok) {
        return status;
    }

    status = srtp_stream_remove(session, 0xcafebabe);
    if (status != srtp_err_status_ok) {
        return status;
    }

    status = srtp_dealloc(session);
    if (status != srtp_err_status_ok) {
        return status;
    }

    return srtp_err_status_ok;
}

// clang-format off
uint8_t test_alt_key[46] = {
  0xe5, 0x19, 0x6f, 0x01, 0x5e, 0xf1, 0x9b, 0xe1,
  0xd7, 0x47, 0xa7, 0x27, 0x07, 0xd7, 0x47, 0x33,
  0x01, 0xc2, 0x35, 0x4d, 0x59, 0x6a, 0xf7, 0x84,
  0x96, 0x98, 0xeb, 0xaa, 0xac, 0xf6, 0xa1, 0x45,
  0xc7, 0x15, 0xe2, 0xea, 0xfe, 0x55, 0x67, 0x96,
  0xb6, 0x96, 0x0b, 0x3a, 0xab, 0xe6
};
// clang-format on

/*
 * srtp_test_update() verifies updating/rekeying existing streams.
 * As stated in https://tools.ietf.org/html/rfc3711#section-3.3.1
 * the value of the ROC must not be reset after a rekey, this test
 * attempts to prove that srtp_update does not reset the ROC.
 */

srtp_err_status_t srtp_test_update(void)
{
    srtp_err_status_t status;
    uint32_t ssrc = 0x12121212;
    size_t msg_len_octets = 32;
    size_t protected_msg_len_octets;
    uint8_t *msg;
    srtp_t srtp_snd, srtp_recv;
    srtp_policy_t policy;

    memset(&policy, 0, sizeof(policy));
    srtp_crypto_policy_set_rtp_default(&policy.rtp);
    srtp_crypto_policy_set_rtcp_default(&policy.rtcp);
    policy.window_size = 128;
    policy.allow_repeat_tx = false;
    policy.next = NULL;
    policy.key = test_key;

    /* create a send and recive ctx with defualt profile and test_key */
    policy.ssrc.type = ssrc_any_outbound;
    status = srtp_create(&srtp_snd, &policy);
    if (status) {
        return status;
    }

    policy.ssrc.type = ssrc_any_inbound;
    status = srtp_create(&srtp_recv, &policy);
    if (status) {
        return status;
    }

    /* protect and unprotect two msg's that will cause the ROC to be equal to 1
     */
    msg = create_rtp_test_packet(msg_len_octets, ssrc, 1, 1, false,
                                 &protected_msg_len_octets, NULL);
    if (msg == NULL) {
        return srtp_err_status_alloc_fail;
    }
    ((srtp_hdr_t *)msg)->seq = htons(65535);

    status = call_srtp_protect(srtp_snd, msg, &protected_msg_len_octets, 0);
    if (status) {
        return srtp_err_status_fail;
    }

    status = call_srtp_unprotect(srtp_recv, msg, &protected_msg_len_octets);
    if (status) {
        return status;
    }

    free(msg);

    msg = create_rtp_test_packet(msg_len_octets, ssrc, 1, 1, false,
                                 &protected_msg_len_octets, NULL);
    if (msg == NULL) {
        return srtp_err_status_alloc_fail;
    }
    ((srtp_hdr_t *)msg)->seq = htons(1);

    status = call_srtp_protect(srtp_snd, msg, &protected_msg_len_octets, 0);
    if (status) {
        return srtp_err_status_fail;
    }

    status = call_srtp_unprotect(srtp_recv, msg, &protected_msg_len_octets);
    if (status) {
        return status;
    }

    free(msg);

    /* update send ctx with same test_key t verify update works*/
    policy.ssrc.type = ssrc_any_outbound;
    policy.key = test_key;
    status = srtp_update(srtp_snd, &policy);
    if (status) {
        return status;
    }

    msg = create_rtp_test_packet(msg_len_octets, ssrc, 1, 1, false,
                                 &protected_msg_len_octets, NULL);
    if (msg == NULL) {
        return srtp_err_status_alloc_fail;
    }
    ((srtp_hdr_t *)msg)->seq = htons(2);

    status = call_srtp_protect(srtp_snd, msg, &protected_msg_len_octets, 0);
    if (status) {
        return srtp_err_status_fail;
    }

    status = call_srtp_unprotect(srtp_recv, msg, &protected_msg_len_octets);
    if (status) {
        return status;
    }

    free(msg);

    /* update send ctx to use test_alt_key */
    policy.ssrc.type = ssrc_any_outbound;
    policy.key = test_alt_key;
    status = srtp_update(srtp_snd, &policy);
    if (status) {
        return status;
    }

    /* create and protect msg with new key and ROC still equal to 1 */
    msg = create_rtp_test_packet(msg_len_octets, ssrc, 1, 1, false,
                                 &protected_msg_len_octets, NULL);
    if (msg == NULL) {
        return srtp_err_status_alloc_fail;
    }
    ((srtp_hdr_t *)msg)->seq = htons(3);

    status = call_srtp_protect(srtp_snd, msg, &protected_msg_len_octets, 0);
    if (status) {
        return srtp_err_status_fail;
    }

    /* verify that receive ctx will fail to unprotect as it still uses test_key
     */
    status = call_srtp_unprotect(srtp_recv, msg, &protected_msg_len_octets);
    if (status == srtp_err_status_ok) {
        return srtp_err_status_fail;
    }

    /* create a new receive ctx with test_alt_key but since it is new it will
     * have ROC equal to 1
     * and therefore should fail to unprotected */
    {
        srtp_t srtp_recv_roc_0;

        policy.ssrc.type = ssrc_any_inbound;
        policy.key = test_alt_key;
        status = srtp_create(&srtp_recv_roc_0, &policy);
        if (status) {
            return status;
        }

        status = call_srtp_unprotect(srtp_recv_roc_0, msg,
                                     &protected_msg_len_octets);
        if (status == srtp_err_status_ok) {
            return srtp_err_status_fail;
        }

        status = srtp_dealloc(srtp_recv_roc_0);
        if (status) {
            return status;
        }
    }

    /* update recive ctx to use test_alt_key */
    policy.ssrc.type = ssrc_any_inbound;
    policy.key = test_alt_key;
    status = srtp_update(srtp_recv, &policy);
    if (status) {
        return status;
    }

    /* verify that can still unprotect, therfore key is updated and ROC value is
     * preserved */
    status = call_srtp_unprotect(srtp_recv, msg, &protected_msg_len_octets);
    if (status) {
        return status;
    }

    free(msg);

    status = srtp_dealloc(srtp_snd);
    if (status) {
        return status;
    }

    status = srtp_dealloc(srtp_recv);
    if (status) {
        return status;
    }

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_test_update_mki(void)
{
    srtp_err_status_t status;
    srtp_t srtp;
    srtp_policy_t policy;

    memset(&policy, 0, sizeof(policy));
    srtp_crypto_policy_set_rtp_default(&policy.rtp);
    srtp_crypto_policy_set_rtcp_default(&policy.rtcp);
    policy.ssrc.type = ssrc_any_outbound;
    policy.keys = test_keys;
    policy.num_master_keys = 1;
    policy.use_mki = true;
    policy.mki_size = 1;

    status = srtp_create(&srtp, &policy);
    if (status) {
        return status;
    }

    /* can not turn off mki */
    policy.use_mki = false;
    policy.mki_size = 0;
    status = srtp_update(srtp, &policy);
    if (status == srtp_err_status_ok) {
        return srtp_err_status_fail;
    }

    /* update same values still ok*/
    policy.use_mki = true;
    policy.mki_size = 1;
    status = srtp_update(srtp, &policy);
    if (status) {
        return status;
    }

    /* can not change mki size*/
    policy.use_mki = true;
    policy.mki_size = 2;
    status = srtp_update(srtp, &policy);
    if (status == srtp_err_status_ok) {
        return srtp_err_status_fail;
    }

    status = srtp_dealloc(srtp);
    if (status) {
        return status;
    }

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_test_setup_protect_trailer_streams(
    srtp_t *srtp_send,
    srtp_t *srtp_send_mki,
    srtp_t *srtp_send_aes_gcm,
    srtp_t *srtp_send_aes_gcm_mki)
{
    srtp_err_status_t status;
    srtp_policy_t policy;
    srtp_policy_t policy_mki;

#ifdef GCM
    srtp_policy_t policy_aes_gcm;
    srtp_policy_t policy_aes_gcm_mki;
#else
    (void)srtp_send_aes_gcm;
    (void)srtp_send_aes_gcm_mki;
#endif // GCM

    memset(&policy, 0, sizeof(policy));
    srtp_crypto_policy_set_rtp_default(&policy.rtp);
    srtp_crypto_policy_set_rtcp_default(&policy.rtcp);
    policy.window_size = 128;
    policy.allow_repeat_tx = false;
    policy.next = NULL;
    policy.ssrc.type = ssrc_any_outbound;
    policy.key = test_key;

    memset(&policy_mki, 0, sizeof(policy_mki));
    srtp_crypto_policy_set_rtp_default(&policy_mki.rtp);
    srtp_crypto_policy_set_rtcp_default(&policy_mki.rtcp);
    policy_mki.window_size = 128;
    policy_mki.allow_repeat_tx = false;
    policy_mki.next = NULL;
    policy_mki.ssrc.type = ssrc_any_outbound;
    policy_mki.key = NULL;
    policy_mki.keys = test_keys;
    policy_mki.num_master_keys = 2;
    policy_mki.use_mki = true;
    policy_mki.mki_size = TEST_MKI_ID_SIZE;

#ifdef GCM
    memset(&policy_aes_gcm, 0, sizeof(policy_aes_gcm));
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy_aes_gcm.rtp);
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy_aes_gcm.rtcp);
    policy_aes_gcm.window_size = 128;
    policy_aes_gcm.allow_repeat_tx = false;
    policy_aes_gcm.next = NULL;
    policy_aes_gcm.ssrc.type = ssrc_any_outbound;
    policy_aes_gcm.key = test_key;

    memset(&policy_aes_gcm_mki, 0, sizeof(policy_aes_gcm_mki));
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy_aes_gcm_mki.rtp);
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy_aes_gcm_mki.rtcp);
    policy_aes_gcm_mki.window_size = 128;
    policy_aes_gcm_mki.allow_repeat_tx = false;
    policy_aes_gcm_mki.next = NULL;
    policy_aes_gcm_mki.ssrc.type = ssrc_any_outbound;
    policy_aes_gcm_mki.key = NULL;
    policy_aes_gcm_mki.keys = test_keys;
    policy_aes_gcm_mki.num_master_keys = 2;
    policy_aes_gcm_mki.use_mki = true;
    policy_aes_gcm_mki.mki_size = TEST_MKI_ID_SIZE;
#endif // GCM

    /* create a send ctx with defualt profile and test_key */
    status = srtp_create(srtp_send, &policy);
    if (status) {
        return status;
    }

    status = srtp_create(srtp_send_mki, &policy_mki);
    if (status) {
        return status;
    }

#ifdef GCM
    status = srtp_create(srtp_send_aes_gcm, &policy_aes_gcm);
    if (status) {
        return status;
    }

    status = srtp_create(srtp_send_aes_gcm_mki, &policy_aes_gcm_mki);
    if (status) {
        return status;
    }
#endif // GCM

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_test_protect_trailer_length(void)
{
    srtp_t srtp_send;
    srtp_t srtp_send_mki;
    srtp_t srtp_send_aes_gcm;
    srtp_t srtp_send_aes_gcm_mki;
    size_t length = 0;
    srtp_err_status_t status;

    status = srtp_test_setup_protect_trailer_streams(
        &srtp_send, &srtp_send_mki, &srtp_send_aes_gcm, &srtp_send_aes_gcm_mki);
    if (status) {
        return status;
    }

    status = srtp_get_protect_trailer_length(srtp_send, 0, &length);
    if (status) {
        return status;
    }

    /*  TAG Length: 10 bytes */
    if (length != 10) {
        return srtp_err_status_fail;
    }

    status = srtp_get_protect_trailer_length(srtp_send_mki, 1, &length);
    if (status) {
        return status;
    }

    /*  TAG Length: 10 bytes + MKI length: 4 bytes*/
    if (length != 14) {
        return srtp_err_status_fail;
    }

#ifdef GCM
    status = srtp_get_protect_trailer_length(srtp_send_aes_gcm, 0, &length);
    if (status) {
        return status;
    }

    /*  TAG Length: 16 bytes */
    if (length != 16) {
        return srtp_err_status_fail;
    }

    status = srtp_get_protect_trailer_length(srtp_send_aes_gcm_mki, 1, &length);
    if (status) {
        return status;
    }

    /*  TAG Length: 16 bytes + MKI length: 4 bytes*/
    if (length != 20) {
        return srtp_err_status_fail;
    }
#endif // GCM

    srtp_dealloc(srtp_send);
    srtp_dealloc(srtp_send_mki);
#ifdef GCM
    srtp_dealloc(srtp_send_aes_gcm);
    srtp_dealloc(srtp_send_aes_gcm_mki);
#endif

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_test_protect_rtcp_trailer_length(void)
{
    srtp_t srtp_send;
    srtp_t srtp_send_mki;
    srtp_t srtp_send_aes_gcm;
    srtp_t srtp_send_aes_gcm_mki;
    size_t length = 0;
    srtp_err_status_t status;

    srtp_test_setup_protect_trailer_streams(
        &srtp_send, &srtp_send_mki, &srtp_send_aes_gcm, &srtp_send_aes_gcm_mki);

    status = srtp_get_protect_rtcp_trailer_length(srtp_send, 0, &length);
    if (status) {
        return status;
    }

    /*  TAG Length: 10 bytes + SRTCP Trailer 4 bytes*/
    if (length != 14) {
        return srtp_err_status_fail;
    }

    status = srtp_get_protect_rtcp_trailer_length(srtp_send_mki, 1, &length);
    if (status) {
        return status;
    }

    /*  TAG Length: 10 bytes + SRTCP Trailer 4 bytes + MKI 4 bytes*/
    if (length != 18) {
        return srtp_err_status_fail;
    }

#ifdef GCM
    status =
        srtp_get_protect_rtcp_trailer_length(srtp_send_aes_gcm, 0, &length);
    if (status) {
        return status;
    }

    /*  TAG Length: 16 bytes + SRTCP Trailer 4 bytes*/
    if (length != 20) {
        return srtp_err_status_fail;
    }

    status =
        srtp_get_protect_rtcp_trailer_length(srtp_send_aes_gcm_mki, 1, &length);
    if (status) {
        return status;
    }

    /*  TAG Length: 16 bytes + SRTCP Trailer 4 bytes + MKI 4 bytes*/
    if (length != 24) {
        return srtp_err_status_fail;
    }
#endif // GCM

    srtp_dealloc(srtp_send);
    srtp_dealloc(srtp_send_mki);
#ifdef GCM
    srtp_dealloc(srtp_send_aes_gcm);
    srtp_dealloc(srtp_send_aes_gcm_mki);
#endif

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_test_out_of_order_after_rollover(void)
{
    srtp_err_status_t status;

    srtp_policy_t sender_policy;
    srtp_t sender_session;

    srtp_policy_t receiver_policy;
    srtp_t receiver_session;

    const uint32_t num_pkts = 5;
    uint8_t *pkts[5];
    size_t pkt_len_octets[5];

    uint32_t i;
    uint32_t stream_roc;

    /* Create the sender */
    memset(&sender_policy, 0, sizeof(sender_policy));
#ifdef GCM
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&sender_policy.rtp);
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&sender_policy.rtcp);
    sender_policy.key = test_key_gcm;
#else
    srtp_crypto_policy_set_rtp_default(&sender_policy.rtp);
    srtp_crypto_policy_set_rtcp_default(&sender_policy.rtcp);
    sender_policy.key = test_key;
#endif
    sender_policy.ssrc.type = ssrc_specific;
    sender_policy.ssrc.value = 0xcafebabe;
    sender_policy.window_size = 128;

    status = srtp_create(&sender_session, &sender_policy);
    if (status) {
        return status;
    }

    /* Create the receiver */
    memset(&receiver_policy, 0, sizeof(receiver_policy));
#ifdef GCM
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&receiver_policy.rtp);
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&receiver_policy.rtcp);
    receiver_policy.key = test_key_gcm;
#else
    srtp_crypto_policy_set_rtp_default(&receiver_policy.rtp);
    srtp_crypto_policy_set_rtcp_default(&receiver_policy.rtcp);
    receiver_policy.key = test_key;
#endif
    receiver_policy.ssrc.type = ssrc_specific;
    receiver_policy.ssrc.value = sender_policy.ssrc.value;
    receiver_policy.window_size = 128;

    status = srtp_create(&receiver_session, &receiver_policy);
    if (status) {
        return status;
    }

    /* Create and protect packets to get to get roc == 1 */
    pkts[0] = create_rtp_test_packet(64, sender_policy.ssrc.value, 65534, 0,
                                     false, &pkt_len_octets[0], NULL);
    status = call_srtp_protect(sender_session, pkts[0], &pkt_len_octets[0], 0);
    if (status) {
        return status;
    }
    status = srtp_stream_get_roc(sender_session, sender_policy.ssrc.value,
                                 &stream_roc);
    if (status) {
        return status;
    }
    if (stream_roc != 0) {
        return srtp_err_status_fail;
    }

    pkts[1] = create_rtp_test_packet(64, sender_policy.ssrc.value, 65535, 1,
                                     false, &pkt_len_octets[1], NULL);
    status = call_srtp_protect(sender_session, pkts[1], &pkt_len_octets[1], 0);
    if (status) {
        return status;
    }
    status = srtp_stream_get_roc(sender_session, sender_policy.ssrc.value,
                                 &stream_roc);
    if (status) {
        return status;
    }
    if (stream_roc != 0) {
        return srtp_err_status_fail;
    }

    pkts[2] = create_rtp_test_packet(64, sender_policy.ssrc.value, 0, 2, false,
                                     &pkt_len_octets[2], NULL);
    status = call_srtp_protect(sender_session, pkts[2], &pkt_len_octets[2], 0);
    if (status) {
        return status;
    }
    status = srtp_stream_get_roc(sender_session, sender_policy.ssrc.value,
                                 &stream_roc);
    if (status) {
        return status;
    }
    if (stream_roc != 1) {
        return srtp_err_status_fail;
    }

    pkts[3] = create_rtp_test_packet(64, sender_policy.ssrc.value, 1, 3, false,
                                     &pkt_len_octets[3], NULL);
    status = call_srtp_protect(sender_session, pkts[3], &pkt_len_octets[3], 0);
    if (status) {
        return status;
    }
    status = srtp_stream_get_roc(sender_session, sender_policy.ssrc.value,
                                 &stream_roc);
    if (status) {
        return status;
    }
    if (stream_roc != 1) {
        return srtp_err_status_fail;
    }

    pkts[4] = create_rtp_test_packet(64, sender_policy.ssrc.value, 2, 4, false,
                                     &pkt_len_octets[4], NULL);
    status = call_srtp_protect(sender_session, pkts[4], &pkt_len_octets[4], 0);
    if (status) {
        return status;
    }
    status = srtp_stream_get_roc(sender_session, sender_policy.ssrc.value,
                                 &stream_roc);
    if (status) {
        return status;
    }
    if (stream_roc != 1) {
        return srtp_err_status_fail;
    }

    /* Unprotect packets in this seq order 65534, 0, 2, 1, 65535 which is
     * equivalent to index 0, 2, 4, 3, 1*/
    status = call_srtp_unprotect(receiver_session, pkts[0], &pkt_len_octets[0]);
    if (status) {
        return status;
    }
    status = srtp_stream_get_roc(receiver_session, receiver_policy.ssrc.value,
                                 &stream_roc);
    if (status) {
        return status;
    }
    if (stream_roc != 0) {
        return srtp_err_status_fail;
    }

    status = call_srtp_unprotect(receiver_session, pkts[2], &pkt_len_octets[2]);
    if (status) {
        return status;
    }
    status = srtp_stream_get_roc(receiver_session, receiver_policy.ssrc.value,
                                 &stream_roc);
    if (status) {
        return status;
    }
    if (stream_roc != 1) {
        return srtp_err_status_fail;
    }

    status = call_srtp_unprotect(receiver_session, pkts[4], &pkt_len_octets[4]);
    if (status) {
        return status;
    }
    status = srtp_stream_get_roc(receiver_session, receiver_policy.ssrc.value,
                                 &stream_roc);
    if (status) {
        return status;
    }
    if (stream_roc != 1) {
        return srtp_err_status_fail;
    }

    status = call_srtp_unprotect(receiver_session, pkts[3], &pkt_len_octets[3]);
    if (status) {
        return status;
    }
    status = srtp_stream_get_roc(receiver_session, receiver_policy.ssrc.value,
                                 &stream_roc);
    if (status) {
        return status;
    }
    if (stream_roc != 1) {
        return srtp_err_status_fail;
    }

    status = call_srtp_unprotect(receiver_session, pkts[1], &pkt_len_octets[1]);
    if (status) {
        return status;
    }
    status = srtp_stream_get_roc(receiver_session, receiver_policy.ssrc.value,
                                 &stream_roc);
    if (status) {
        return status;
    }
    if (stream_roc != 1) {
        return srtp_err_status_fail;
    }

    /* Cleanup */
    status = srtp_dealloc(sender_session);
    if (status) {
        return status;
    }

    status = srtp_dealloc(receiver_session);
    if (status) {
        return status;
    }

    for (i = 0; i < num_pkts; i++) {
        free(pkts[i]);
    }

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_test_get_roc(void)
{
    srtp_err_status_t status;
    srtp_policy_t policy;
    srtp_t session;
    uint8_t *pkt;
    uint32_t i;
    uint32_t roc;
    uint32_t ts;
    uint16_t seq;

    size_t msg_len_octets = 32;
    size_t protected_msg_len_octets;

    memset(&policy, 0, sizeof(policy));
    srtp_crypto_policy_set_rtp_default(&policy.rtp);
    srtp_crypto_policy_set_rtcp_default(&policy.rtcp);
    policy.ssrc.type = ssrc_specific;
    policy.ssrc.value = 0xcafebabe;
    policy.key = test_key;
    policy.window_size = 128;

    /* Create a sender session */
    status = srtp_create(&session, &policy);
    if (status) {
        return status;
    }

    /* Set start sequence so we roll over */
    seq = 65535;
    ts = 0;

    for (i = 0; i < 2; i++) {
        pkt = create_rtp_test_packet(msg_len_octets, policy.ssrc.value, seq, ts,
                                     false, &protected_msg_len_octets, NULL);
        status = call_srtp_protect(session, pkt, &protected_msg_len_octets, 0);
        free(pkt);
        if (status) {
            return status;
        }

        status = srtp_stream_get_roc(session, policy.ssrc.value, &roc);
        if (status) {
            return status;
        }

        if (roc != i) {
            return srtp_err_status_fail;
        }

        seq++;
        ts++;
    }

    /* Cleanup */
    status = srtp_dealloc(session);
    if (status) {
        return status;
    }

    return srtp_err_status_ok;
}

static srtp_err_status_t test_set_receiver_roc(uint32_t packets,
                                               uint32_t roc_to_set)
{
    srtp_err_status_t status;

    srtp_policy_t sender_policy;
    srtp_t sender_session;

    srtp_policy_t receiver_policy;
    srtp_t receiver_session;

    uint8_t *pkt_1;
    uint8_t *recv_pkt_1;

    uint8_t *pkt_2;
    uint8_t *recv_pkt_2;

    uint32_t i;
    uint32_t ts;
    uint16_t seq;
    uint16_t stride;

    size_t msg_len_octets = 32;
    size_t protected_msg_len_octets_1;
    size_t protected_msg_len_octets_2;

    /* Create the sender */
    memset(&sender_policy, 0, sizeof(sender_policy));
#ifdef GCM
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&sender_policy.rtp);
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&sender_policy.rtcp);
    sender_policy.key = test_key_gcm;
#else
    srtp_crypto_policy_set_rtp_default(&sender_policy.rtp);
    srtp_crypto_policy_set_rtcp_default(&sender_policy.rtcp);
    sender_policy.key = test_key;
#endif
    sender_policy.ssrc.type = ssrc_specific;
    sender_policy.ssrc.value = 0xcafebabe;
    sender_policy.window_size = 128;

    status = srtp_create(&sender_session, &sender_policy);
    if (status) {
        return status;
    }

    /* Create and protect packets */
    i = 0;
    seq = 0;
    ts = 0;
    stride = 0x4000;
    while (i < packets) {
        uint8_t *tmp_pkt;
        size_t tmp_len;

        tmp_pkt =
            create_rtp_test_packet(msg_len_octets, sender_policy.ssrc.value,
                                   seq, ts, false, &tmp_len, NULL);
        status = call_srtp_protect(sender_session, tmp_pkt, &tmp_len, 0);
        free(tmp_pkt);
        if (status) {
            return status;
        }

        while (stride > (packets - i) && stride > 1) {
            stride >>= 1;
        }

        i += stride;
        seq += stride;
        ts++;
    }

    /* Create the first packet to decrypt and test for ROC change */
    pkt_1 =
        create_rtp_test_packet(msg_len_octets, sender_policy.ssrc.value, seq,
                               ts, false, &protected_msg_len_octets_1, NULL);
    status = call_srtp_protect(sender_session, pkt_1,
                               &protected_msg_len_octets_1, 0);
    if (status) {
        return status;
    }

    /* Create the second packet to decrypt and test for ROC change */
    seq++;
    ts++;
    pkt_2 =
        create_rtp_test_packet(msg_len_octets, sender_policy.ssrc.value, seq,
                               ts, false, &protected_msg_len_octets_2, NULL);
    status = call_srtp_protect(sender_session, pkt_2,
                               &protected_msg_len_octets_2, 0);
    if (status) {
        return status;
    }

    /* Create the receiver */
    memset(&receiver_policy, 0, sizeof(receiver_policy));
#ifdef GCM
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&receiver_policy.rtp);
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&receiver_policy.rtcp);
    receiver_policy.key = test_key_gcm;
#else
    srtp_crypto_policy_set_rtp_default(&receiver_policy.rtp);
    srtp_crypto_policy_set_rtcp_default(&receiver_policy.rtcp);
    receiver_policy.key = test_key;
#endif
    receiver_policy.ssrc.type = ssrc_specific;
    receiver_policy.ssrc.value = sender_policy.ssrc.value;
    receiver_policy.window_size = 128;

    status = srtp_create(&receiver_session, &receiver_policy);
    if (status) {
        return status;
    }

    /* Make a copy of the first sent protected packet */
    recv_pkt_1 = malloc(protected_msg_len_octets_1);
    if (recv_pkt_1 == NULL) {
        return srtp_err_status_fail;
    }
    memcpy(recv_pkt_1, pkt_1, protected_msg_len_octets_1);

    /* Make a copy of the second sent protected packet */
    recv_pkt_2 = malloc(protected_msg_len_octets_2);
    if (recv_pkt_2 == NULL) {
        return srtp_err_status_fail;
    }
    memcpy(recv_pkt_2, pkt_2, protected_msg_len_octets_2);

    /* Set the ROC to the wanted value */
    status = srtp_stream_set_roc(receiver_session, receiver_policy.ssrc.value,
                                 roc_to_set);
    if (status) {
        return status;
    }

    /* Unprotect the first packet */
    status = call_srtp_unprotect(receiver_session, recv_pkt_1,
                                 &protected_msg_len_octets_1);
    if (status) {
        return status;
    }

    /* Unprotect the second packet */
    status = call_srtp_unprotect(receiver_session, recv_pkt_2,
                                 &protected_msg_len_octets_2);
    if (status) {
        return status;
    }

    /* Cleanup */
    status = srtp_dealloc(sender_session);
    if (status) {
        return status;
    }

    status = srtp_dealloc(receiver_session);
    if (status) {
        return status;
    }

    free(pkt_1);
    free(recv_pkt_1);
    free(pkt_2);
    free(recv_pkt_2);

    return srtp_err_status_ok;
}

static srtp_err_status_t test_set_sender_roc(uint16_t seq, uint32_t roc_to_set)
{
    srtp_err_status_t status;

    srtp_policy_t sender_policy;
    srtp_t sender_session;

    srtp_policy_t receiver_policy;
    srtp_t receiver_session;

    uint8_t *pkt;
    uint8_t *recv_pkt;

    uint32_t ts;

    size_t msg_len_octets = 32;
    size_t protected_msg_len_octets;

    /* Create the sender */
    memset(&sender_policy, 0, sizeof(sender_policy));
#ifdef GCM
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&sender_policy.rtp);
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&sender_policy.rtcp);
    sender_policy.key = test_key_gcm;
#else
    srtp_crypto_policy_set_rtp_default(&sender_policy.rtp);
    srtp_crypto_policy_set_rtcp_default(&sender_policy.rtcp);
    sender_policy.key = test_key;
#endif
    sender_policy.ssrc.type = ssrc_specific;
    sender_policy.ssrc.value = 0xcafebabe;
    sender_policy.window_size = 128;

    status = srtp_create(&sender_session, &sender_policy);
    if (status) {
        return status;
    }

    /* Set the ROC before encrypting the first packet */
    status = srtp_stream_set_roc(sender_session, sender_policy.ssrc.value,
                                 roc_to_set);
    if (status != srtp_err_status_ok) {
        return status;
    }

    /* Create the packet to decrypt */
    ts = 0;
    pkt = create_rtp_test_packet(msg_len_octets, sender_policy.ssrc.value, seq,
                                 ts, false, &protected_msg_len_octets, NULL);
    status =
        call_srtp_protect(sender_session, pkt, &protected_msg_len_octets, 0);
    if (status) {
        return status;
    }

    /* Create the receiver */
    memset(&receiver_policy, 0, sizeof(receiver_policy));
#ifdef GCM
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&receiver_policy.rtp);
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&receiver_policy.rtcp);
    receiver_policy.key = test_key_gcm;
#else
    srtp_crypto_policy_set_rtp_default(&receiver_policy.rtp);
    srtp_crypto_policy_set_rtcp_default(&receiver_policy.rtcp);
    receiver_policy.key = test_key;
#endif
    receiver_policy.ssrc.type = ssrc_specific;
    receiver_policy.ssrc.value = sender_policy.ssrc.value;
    receiver_policy.window_size = 128;

    status = srtp_create(&receiver_session, &receiver_policy);
    if (status) {
        return status;
    }

    /* Make a copy of the sent protected packet */
    recv_pkt = malloc(protected_msg_len_octets);
    if (recv_pkt == NULL) {
        return srtp_err_status_fail;
    }
    memcpy(recv_pkt, pkt, protected_msg_len_octets);

    /* Set the ROC to the wanted value */
    status = srtp_stream_set_roc(receiver_session, receiver_policy.ssrc.value,
                                 roc_to_set);
    if (status) {
        return status;
    }

    status = call_srtp_unprotect(receiver_session, recv_pkt,
                                 &protected_msg_len_octets);
    if (status) {
        return status;
    }

    /* Cleanup */
    status = srtp_dealloc(sender_session);
    if (status) {
        return status;
    }

    status = srtp_dealloc(receiver_session);
    if (status) {
        return status;
    }

    free(pkt);
    free(recv_pkt);

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_test_set_receiver_roc(void)
{
    int packets;
    uint32_t roc;
    srtp_err_status_t status;

    /* First test does not rollover */
    packets = 1;
    roc = 0;

    status = test_set_receiver_roc(packets - 1, roc);
    if (status) {
        return status;
    }

    status = test_set_receiver_roc(packets, roc);
    if (status) {
        return status;
    }

    status = test_set_receiver_roc(packets + 1, roc);
    if (status) {
        return status;
    }

    status = test_set_receiver_roc(packets + 60000, roc);
    if (status) {
        return status;
    }

    /* Second test should rollover */
    packets = 65535;
    roc = 0;

    status = test_set_receiver_roc(packets - 1, roc);
    if (status) {
        return status;
    }

    status = test_set_receiver_roc(packets, roc);
    if (status) {
        return status;
    }

    /* Now the rollover counter should be 1 */
    roc = 1;
    status = test_set_receiver_roc(packets + 1, roc);
    if (status) {
        return status;
    }

    status = test_set_receiver_roc(packets + 60000, roc);
    if (status) {
        return status;
    }

    status = test_set_receiver_roc(packets + 65535, roc);
    if (status) {
        return status;
    }

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_test_set_sender_roc(void)
{
    uint32_t roc;
    uint16_t seq;
    srtp_err_status_t status;

    seq = 43210;
    roc = 0;
    status = test_set_sender_roc(seq, roc);
    if (status) {
        return status;
    }

    roc = 65535;
    status = test_set_sender_roc(seq, roc);
    if (status) {
        return status;
    }

    roc = 0xffff;
    status = test_set_sender_roc(seq, roc);
    if (status) {
        return status;
    }

    roc = 0xffff00;
    status = test_set_sender_roc(seq, roc);
    if (status) {
        return status;
    }

    roc = 0xfffffff0;
    status = test_set_sender_roc(seq, roc);
    if (status) {
        return status;
    }

    return srtp_err_status_ok;
}

/* This test illustrates how the ROC can be mismatched between
 * sender and receiver when a packets are lost before the initial
 * sequence number wraparound. In a nutshell:
 * - Sender encrypts sequence numbers 65535, 0, 1, ...
 * - Receiver only receives 1 initially.
 * In that state the receiver will assume the sender used ROC=0 to
 * encrypt the packet with sequence number 0.
 * This is a long-standing problem that is best avoided by a choice
 * of initial sequence number in the lower half of the sequence number
 * space.
 */
srtp_err_status_t srtp_test_roc_mismatch(void)
{
    srtp_policy_t sender_policy;
    srtp_t sender_session;

    srtp_policy_t receiver_policy;
    srtp_t receiver_session;

    const uint32_t num_pkts = 3;
    uint8_t *pkts[3];
    size_t pkt_len_octets[3];

    const uint16_t seq = 0xffff;
    uint32_t i;

    /* Create the sender */
    memset(&sender_policy, 0, sizeof(sender_policy));
#ifdef GCM
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&sender_policy.rtp);
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&sender_policy.rtcp);
    sender_policy.key = test_key_gcm;
#else
    srtp_crypto_policy_set_rtp_default(&sender_policy.rtp);
    srtp_crypto_policy_set_rtcp_default(&sender_policy.rtcp);
    sender_policy.key = test_key;
#endif
    sender_policy.ssrc.type = ssrc_specific;
    sender_policy.ssrc.value = 0xcafebabe;
    sender_policy.window_size = 128;

    CHECK_OK(srtp_create(&sender_session, &sender_policy));

    /* Create the receiver */
    memset(&receiver_policy, 0, sizeof(receiver_policy));
#ifdef GCM
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&receiver_policy.rtp);
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&receiver_policy.rtcp);
    receiver_policy.key = test_key_gcm;
#else
    srtp_crypto_policy_set_rtp_default(&receiver_policy.rtp);
    srtp_crypto_policy_set_rtcp_default(&receiver_policy.rtcp);
    receiver_policy.key = test_key;
#endif
    receiver_policy.ssrc.type = ssrc_specific;
    receiver_policy.ssrc.value = sender_policy.ssrc.value;
    receiver_policy.window_size = 128;

    CHECK_OK(srtp_create(&receiver_session, &receiver_policy));

    /* Create and protect packets to get to get ROC == 1 */
    for (i = 0; i < num_pkts; i++) {
        pkts[i] = create_rtp_test_packet(64, sender_policy.ssrc.value,
                                         (uint16_t)(seq + i), 0, false,
                                         &pkt_len_octets[i], NULL);
        CHECK_OK(
            call_srtp_protect(sender_session, pkts[i], &pkt_len_octets[i], 0));
    }

    /* Decrypt in reverse order (1, 65535) */
    CHECK_RETURN(
        call_srtp_unprotect(receiver_session, pkts[2], &pkt_len_octets[2]),
        srtp_err_status_auth_fail);
    CHECK_OK(
        call_srtp_unprotect(receiver_session, pkts[0], &pkt_len_octets[0]));
    /* After decryption of the previous ROC rollover will work as expected */
    /* Only pkts[1] is checked since that is not modified by the attempt to
     * decryption. */
    CHECK_OK(
        call_srtp_unprotect(receiver_session, pkts[1], &pkt_len_octets[1]));

    for (i = 0; i < num_pkts; i++) {
        free(pkts[i]);
    }
    CHECK_OK(srtp_dealloc(sender_session));
    CHECK_OK(srtp_dealloc(receiver_session));
    return srtp_err_status_ok;
}

/*
 * srtp policy definitions - these definitions are used above
 */

// clang-format off
uint8_t test_key[46] = {
    0xe1, 0xf9, 0x7a, 0x0d, 0x3e, 0x01, 0x8b, 0xe0,
    0xd6, 0x4f, 0xa3, 0x2c, 0x06, 0xde, 0x41, 0x39,
    0x0e, 0xc6, 0x75, 0xad, 0x49, 0x8a, 0xfe, 0xeb,
    0xb6, 0x96, 0x0b, 0x3a, 0xab, 0xe6, 0xc1, 0x73,
    0xc3, 0x17, 0xf2, 0xda, 0xbe, 0x35, 0x77, 0x93,
    0xb6, 0x96, 0x0b, 0x3a, 0xab, 0xe6
};

uint8_t test_key_2[46] = {
    0xf0, 0xf0, 0x49, 0x14, 0xb5, 0x13, 0xf2, 0x76,
    0x3a, 0x1b, 0x1f, 0xa1, 0x30, 0xf1, 0x0e, 0x29,
    0x98, 0xf6, 0xf6, 0xe4, 0x3e, 0x43, 0x09, 0xd1,
    0xe6, 0x22, 0xa0, 0xe3, 0x32, 0xb9, 0xf1, 0xb6,
    0xc3, 0x17, 0xf2, 0xda, 0xbe, 0x35, 0x77, 0x93,
    0xb6, 0x96, 0x0b, 0x3a, 0xab, 0xe6
};

uint8_t test_key_gcm[28] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
    0xa8, 0xa9, 0xaa, 0xab
};

uint8_t test_mki_id[TEST_MKI_ID_SIZE] = {
    0xe1, 0xf9, 0x7a, 0x0d
};

uint8_t test_mki_id_2[TEST_MKI_ID_SIZE] = {
    0xf3, 0xa1, 0x46, 0x71
};
// clang-format on

const srtp_policy_t default_policy = {
    { ssrc_any_outbound, 0 }, /* SSRC */
    {
        /* SRTP policy */
        SRTP_AES_ICM_128,               /* cipher type                 */
        SRTP_AES_ICM_128_KEY_LEN_WSALT, /* cipher key length in octets */
        SRTP_HMAC_SHA1,                 /* authentication func type    */
        16,                             /* auth key length in octets   */
        10,                             /* auth tag length in octets   */
        sec_serv_conf_and_auth          /* security services flag      */
    },
    {
        /* SRTCP policy */
        SRTP_AES_ICM_128,               /* cipher type                 */
        SRTP_AES_ICM_128_KEY_LEN_WSALT, /* cipher key length in octets */
        SRTP_HMAC_SHA1,                 /* authentication func type    */
        16,                             /* auth key length in octets   */
        10,                             /* auth tag length in octets   */
        sec_serv_conf_and_auth          /* security services flag      */
    },
    NULL,
    (srtp_master_key_t **)test_keys,
    2,                /* indicates the number of Master keys          */
    true,             /* no mki */
    TEST_MKI_ID_SIZE, /* mki size */
    128,              /* replay window size                           */
    0,                /* retransmission not allowed                   */
    NULL,             /* no encrypted extension headers               */
    0,                /* list of encrypted extension headers is empty */
    false,            /* cryptex                                      */
    NULL
};

const srtp_policy_t aes_only_policy = {
    { ssrc_any_outbound, 0 }, /* SSRC                        */
    {
        SRTP_AES_ICM_128,               /* cipher type                 */
        SRTP_AES_ICM_128_KEY_LEN_WSALT, /* cipher key length in octets */
        SRTP_NULL_AUTH,                 /* authentication func type    */
        0,                              /* auth key length in octets   */
        0,                              /* auth tag length in octets   */
        sec_serv_conf                   /* security services flag      */
    },
    {
        SRTP_AES_ICM_128,               /* cipher type                 */
        SRTP_AES_ICM_128_KEY_LEN_WSALT, /* cipher key length in octets */
        SRTP_NULL_AUTH,                 /* authentication func type    */
        0,                              /* auth key length in octets   */
        0,                              /* auth tag length in octets   */
        sec_serv_conf                   /* security services flag      */
    },
    NULL,
    (srtp_master_key_t **)test_keys,
    2,                /* indicates the number of Master keys          */
    true,             /* no mki */
    TEST_MKI_ID_SIZE, /* mki size */
    128,              /* replay window size                           */
    0,                /* retransmission not allowed                   */
    NULL,             /* no encrypted extension headers               */
    0,                /* list of encrypted extension headers is empty */
    false,            /* cryptex                                      */
    NULL
};

const srtp_policy_t hmac_only_policy = {
    { ssrc_any_outbound, 0 }, /* SSRC */
    {
        SRTP_NULL_CIPHER,               /* cipher type                 */
        SRTP_AES_ICM_128_KEY_LEN_WSALT, /* cipher key length in octets */
        SRTP_HMAC_SHA1,                 /* authentication func type    */
        20,                             /* auth key length in octets   */
        4,                              /* auth tag length in octets   */
        sec_serv_auth                   /* security services flag      */
    },
    {
        SRTP_NULL_CIPHER,               /* cipher type                 */
        SRTP_AES_ICM_128_KEY_LEN_WSALT, /* cipher key length in octets */
        SRTP_HMAC_SHA1,                 /* authentication func type    */
        20,                             /* auth key length in octets   */
        4,                              /* auth tag length in octets   */
        sec_serv_auth                   /* security services flag      */
    },
    NULL,
    (srtp_master_key_t **)test_keys,
    2,                /* Number of Master keys associated with the policy */
    true,             /* no mki */
    TEST_MKI_ID_SIZE, /* mki size */
    128,              /* replay window size                               */
    0,                /* retransmission not allowed                       */
    NULL,             /* no encrypted extension headers                   */
    0,                /* list of encrypted extension headers is empty     */
    false,            /* cryptex                                          */
    NULL
};

#ifdef GCM
const srtp_policy_t aes128_gcm_8_policy = {
    { ssrc_any_outbound, 0 }, /* SSRC */
    {
        /* SRTP policy */
        SRTP_AES_GCM_128,               /* cipher type                 */
        SRTP_AES_GCM_128_KEY_LEN_WSALT, /* cipher key length in octets */
        SRTP_NULL_AUTH,                 /* authentication func type    */
        0,                              /* auth key length in octets   */
        8,                              /* auth tag length in octets   */
        sec_serv_conf_and_auth          /* security services flag      */
    },
    {
        /* SRTCP policy */
        SRTP_AES_GCM_128,               /* cipher type                 */
        SRTP_AES_GCM_128_KEY_LEN_WSALT, /* cipher key length in octets */
        SRTP_NULL_AUTH,                 /* authentication func type    */
        0,                              /* auth key length in octets   */
        8,                              /* auth tag length in octets   */
        sec_serv_conf_and_auth          /* security services flag      */
    },
    NULL,
    (srtp_master_key_t **)test_keys,
    2,                /* indicates the number of Master keys          */
    true,             /* no mki */
    TEST_MKI_ID_SIZE, /* mki size */
    128,              /* replay window size                           */
    0,                /* retransmission not allowed                   */
    NULL,             /* no encrypted extension headers               */
    0,                /* list of encrypted extension headers is empty */
    false,            /* cryptex                                      */
    NULL
};

const srtp_policy_t aes128_gcm_8_cauth_policy = {
    { ssrc_any_outbound, 0 }, /* SSRC */
    {
        /* SRTP policy */
        SRTP_AES_GCM_128,               /* cipher type                 */
        SRTP_AES_GCM_128_KEY_LEN_WSALT, /* cipher key length in octets */
        SRTP_NULL_AUTH,                 /* authentication func type    */
        0,                              /* auth key length in octets   */
        8,                              /* auth tag length in octets   */
        sec_serv_conf_and_auth          /* security services flag      */
    },
    {
        /* SRTCP policy */
        SRTP_AES_GCM_128,               /* cipher type                 */
        SRTP_AES_GCM_128_KEY_LEN_WSALT, /* cipher key length in octets */
        SRTP_NULL_AUTH,                 /* authentication func type    */
        0,                              /* auth key length in octets   */
        8,                              /* auth tag length in octets   */
        sec_serv_auth                   /* security services flag      */
    },
    NULL,
    (srtp_master_key_t **)test_keys,
    2,                /* indicates the number of Master keys          */
    true,             /* no mki */
    TEST_MKI_ID_SIZE, /* mki size */
    128,              /* replay window size                           */
    0,                /* retransmission not allowed                   */
    NULL,             /* no encrypted extension headers               */
    0,                /* list of encrypted extension headers is empty */
    false,            /* cryptex                                      */
    NULL
};

const srtp_policy_t aes256_gcm_8_policy = {
    { ssrc_any_outbound, 0 }, /* SSRC */
    {
        /* SRTP policy */
        SRTP_AES_GCM_256,               /* cipher type                 */
        SRTP_AES_GCM_256_KEY_LEN_WSALT, /* cipher key length in octets */
        SRTP_NULL_AUTH,                 /* authentication func type    */
        0,                              /* auth key length in octets   */
        8,                              /* auth tag length in octets   */
        sec_serv_conf_and_auth          /* security services flag      */
    },
    {
        /* SRTCP policy */
        SRTP_AES_GCM_256,               /* cipher type                 */
        SRTP_AES_GCM_256_KEY_LEN_WSALT, /* cipher key length in octets */
        SRTP_NULL_AUTH,                 /* authentication func type    */
        0,                              /* auth key length in octets   */
        8,                              /* auth tag length in octets   */
        sec_serv_conf_and_auth          /* security services flag      */
    },
    NULL,
    (srtp_master_key_t **)test_keys,
    2,                /* indicates the number of Master keys          */
    true,             /* no mki */
    TEST_MKI_ID_SIZE, /* mki size */
    128,              /* replay window size                           */
    0,                /* retransmission not allowed                   */
    NULL,             /* no encrypted extension headers               */
    0,                /* list of encrypted extension headers is empty */
    false,            /* cryptex                                      */
    NULL
};

const srtp_policy_t aes256_gcm_8_cauth_policy = {
    { ssrc_any_outbound, 0 }, /* SSRC */
    {
        /* SRTP policy */
        SRTP_AES_GCM_256,               /* cipher type                 */
        SRTP_AES_GCM_256_KEY_LEN_WSALT, /* cipher key length in octets */
        SRTP_NULL_AUTH,                 /* authentication func type    */
        0,                              /* auth key length in octets   */
        8,                              /* auth tag length in octets   */
        sec_serv_conf_and_auth          /* security services flag      */
    },
    {
        /* SRTCP policy */
        SRTP_AES_GCM_256,               /* cipher type                 */
        SRTP_AES_GCM_256_KEY_LEN_WSALT, /* cipher key length in octets */
        SRTP_NULL_AUTH,                 /* authentication func type    */
        0,                              /* auth key length in octets   */
        8,                              /* auth tag length in octets   */
        sec_serv_auth                   /* security services flag      */
    },
    NULL,
    (srtp_master_key_t **)test_keys,
    2,                /* indicates the number of Master keys          */
    true,             /* no mki */
    TEST_MKI_ID_SIZE, /* mki size */
    128,              /* replay window size                           */
    0,                /* retransmission not allowed                   */
    NULL,             /* no encrypted extension headers               */
    0,                /* list of encrypted extension headers is empty */
    false,            /* cryptex                                      */
    NULL
};
#endif

const srtp_policy_t null_policy = {
    { ssrc_any_outbound, 0 }, /* SSRC */
    {
        SRTP_NULL_CIPHER,               /* cipher type                 */
        SRTP_AES_GCM_256_KEY_LEN_WSALT, /* cipher key length in octets */
        SRTP_NULL_AUTH,                 /* authentication func type    */
        0,                              /* auth key length in octets   */
        0,                              /* auth tag length in octets   */
        sec_serv_none                   /* security services flag      */
    },
    {
        SRTP_NULL_CIPHER,               /* cipher type                 */
        SRTP_AES_GCM_256_KEY_LEN_WSALT, /* cipher key length in octets */
        SRTP_NULL_AUTH,                 /* authentication func type    */
        0,                              /* auth key length in octets   */
        0,                              /* auth tag length in octets   */
        sec_serv_none                   /* security services flag      */
    },
    NULL,
    (srtp_master_key_t **)test_keys,
    2,                /* indicates the number of Master keys          */
    true,             /* no mki */
    TEST_MKI_ID_SIZE, /* mki size */
    128,              /* replay window size                           */
    0,                /* retransmission not allowed                   */
    NULL,             /* no encrypted extension headers               */
    0,                /* list of encrypted extension headers is empty */
    false,            /* cryptex                                      */
    NULL
};

// clang-format off
uint8_t test_256_key[46] = {
    0xf0, 0xf0, 0x49, 0x14, 0xb5, 0x13, 0xf2, 0x76,
    0x3a, 0x1b, 0x1f, 0xa1, 0x30, 0xf1, 0x0e, 0x29,
    0x98, 0xf6, 0xf6, 0xe4, 0x3e, 0x43, 0x09, 0xd1,
    0xe6, 0x22, 0xa0, 0xe3, 0x32, 0xb9, 0xf1, 0xb6,

    0x3b, 0x04, 0x80, 0x3d, 0xe5, 0x1e, 0xe7, 0xc9,
    0x64, 0x23, 0xab, 0x5b, 0x78, 0xd2
};

uint8_t test_256_key_2[46] = {
    0xe1, 0xf9, 0x7a, 0x0d, 0x3e, 0x01, 0x8b, 0xe0,
    0xd6, 0x4f, 0xa3, 0x2c, 0x06, 0xde, 0x41, 0x39,
    0x0e, 0xc6, 0x75, 0xad, 0x49, 0x8a, 0xfe, 0xeb,
    0xb6, 0x96, 0x0b, 0x3a, 0xab, 0xe6, 0xc1, 0x73,
    0x3b, 0x04, 0x80, 0x3d, 0xe5, 0x1e, 0xe7, 0xc9,
    0x64, 0x23, 0xab, 0x5b, 0x78, 0xd2
};

srtp_master_key_t master_256_key_1 = {
    test_256_key,
    test_mki_id
};

srtp_master_key_t master_256_key_2 = {
    test_256_key_2,
    test_mki_id_2
};

srtp_master_key_t *test_256_keys[2] = {
    &master_key_1,
    &master_key_2
};
// clang-format on

const srtp_policy_t aes_256_hmac_policy = {
    { ssrc_any_outbound, 0 }, /* SSRC */
    {
        /* SRTP policy */
        SRTP_AES_ICM_256,               /* cipher type                 */
        SRTP_AES_ICM_256_KEY_LEN_WSALT, /* cipher key length in octets */
        SRTP_HMAC_SHA1,                 /* authentication func type    */
        20,                             /* auth key length in octets   */
        10,                             /* auth tag length in octets   */
        sec_serv_conf_and_auth          /* security services flag      */
    },
    {
        /* SRTCP policy */
        SRTP_AES_ICM_256,               /* cipher type                 */
        SRTP_AES_ICM_256_KEY_LEN_WSALT, /* cipher key length in octets */
        SRTP_HMAC_SHA1,                 /* authentication func type    */
        20,                             /* auth key length in octets   */
        10,                             /* auth tag length in octets   */
        sec_serv_conf_and_auth          /* security services flag      */
    },
    NULL,
    (srtp_master_key_t **)test_256_keys,
    2,                /* indicates the number of Master keys          */
    true,             /* no mki */
    TEST_MKI_ID_SIZE, /* mki size */
    128,              /* replay window size                           */
    false,            /* retransmission not allowed                   */
    NULL,             /* no encrypted extension headers               */
    0,                /* list of encrypted extension headers is empty */
    false,            /* cryptex                                      */
    NULL
};

const srtp_policy_t aes_256_hmac_32_policy = {
    { ssrc_any_outbound, 0 }, /* SSRC */
    {
        /* SRTP policy */
        SRTP_AES_ICM_256,               /* cipher type                 */
        SRTP_AES_ICM_256_KEY_LEN_WSALT, /* cipher key length in octets */
        SRTP_HMAC_SHA1,                 /* authentication func type    */
        20,                             /* auth key length in octets   */
        4,                              /* auth tag length in octets   */
        sec_serv_conf_and_auth          /* security services flag      */
    },
    {
        /* SRTCP policy */
        SRTP_AES_ICM_256,               /* cipher type                 */
        SRTP_AES_ICM_256_KEY_LEN_WSALT, /* cipher key length in octets */
        SRTP_HMAC_SHA1,                 /* authentication func type    */
        20,                             /* auth key length in octets   */
        10,                             /* auth tag length in octets.
                                           80 bits per RFC 3711. */
        sec_serv_conf_and_auth          /* security services flag      */
    },
    NULL,
    (srtp_master_key_t **)test_256_keys,
    2,                /* indicates the number of Master keys          */
    true,             /* no mki */
    TEST_MKI_ID_SIZE, /* mki size */
    128,              /* replay window size                           */
    false,            /* retransmission not allowed                   */
    NULL,             /* no encrypted extension headers               */
    0,                /* list of encrypted extension headers is empty */
    false,            /* cryptex                                      */
    NULL
};

const srtp_policy_t hmac_only_with_no_master_key = {
    { ssrc_any_outbound, 0 }, /* SSRC */
    {
        SRTP_NULL_CIPHER, /* cipher type                 */
        0,                /* cipher key length in octets */
        SRTP_HMAC_SHA1,   /* authentication func type    */
        20,               /* auth key length in octets   */
        4,                /* auth tag length in octets   */
        sec_serv_auth     /* security services flag      */
    },
    {
        SRTP_NULL_CIPHER, /* cipher type                 */
        0,                /* cipher key length in octets */
        SRTP_HMAC_SHA1,   /* authentication func type    */
        20,               /* auth key length in octets   */
        4,                /* auth tag length in octets   */
        sec_serv_auth     /* security services flag      */
    },
    NULL,
    NULL,  /* no master keys*/
    0,     /* indicates the number of Master keys          */
    false, /* no mki */
    0,     /* mki size */
    128,   /* replay window size                           */
    false, /* retransmission not allowed                   */
    NULL,  /* no encrypted extension headers               */
    0,     /* list of encrypted extension headers is empty */
    false, /* cryptex                                      */
    NULL
};

/*
 * an array of pointers to the policies listed above
 *
 * This array is used to test various aspects of libSRTP for
 * different cryptographic policies.  The order of the elements
 * matters - the timing test generates output that can be used
 * in a plot (see the gnuplot script file 'timing').  If you
 * add to this list, you should do it at the end.
 */

// clang-format off
const srtp_policy_t *policy_array[] = {
    &hmac_only_policy,
    &aes_only_policy,
    &default_policy,
#ifdef GCM
    &aes128_gcm_8_policy,
    &aes128_gcm_8_cauth_policy,
    &aes256_gcm_8_policy,
    &aes256_gcm_8_cauth_policy,
#endif
    &null_policy,
    &aes_256_hmac_policy,
    &aes_256_hmac_32_policy,
    NULL
};
// clang-format on

// clang-format off
const srtp_policy_t *invalid_policy_array[] = {
    &hmac_only_with_no_master_key,
    NULL
};
// clang-format on

const srtp_policy_t wildcard_policy = {
    { ssrc_any_outbound, 0 }, /* SSRC */
    {
        /* SRTP policy */
        SRTP_AES_ICM_128,               /* cipher type                 */
        SRTP_AES_ICM_128_KEY_LEN_WSALT, /* cipher key length in octets */
        SRTP_HMAC_SHA1,                 /* authentication func type    */
        16,                             /* auth key length in octets   */
        10,                             /* auth tag length in octets   */
        sec_serv_conf_and_auth          /* security services flag      */
    },
    {
        /* SRTCP policy */
        SRTP_AES_ICM_128,               /* cipher type                 */
        SRTP_AES_ICM_128_KEY_LEN_WSALT, /* cipher key length in octets */
        SRTP_HMAC_SHA1,                 /* authentication func type    */
        16,                             /* auth key length in octets   */
        10,                             /* auth tag length in octets   */
        sec_serv_conf_and_auth          /* security services flag      */
    },
    test_key,
    NULL,
    0,
    false, /* no mki */
    0,     /* mki size */
    128,   /* replay window size                           */
    0,     /* retransmission not allowed                   */
    NULL,  /* no encrypted extension headers               */
    0,     /* list of encrypted extension headers is empty */
    false, /* cryptex                                      */
    NULL
};

static srtp_stream_t stream_list_test_create_stream(uint32_t ssrc)
{
    srtp_stream_t stream = malloc(sizeof(srtp_stream_ctx_t));
    stream->ssrc = ssrc;
    return stream;
}

static void stream_list_test_free_stream(srtp_stream_t stream)
{
    free(stream);
}

bool stream_list_test_count_cb(srtp_stream_t stream, void *data)
{
    size_t *count = (size_t *)data;
    (*count)++;
    (void)stream;
    return true;
}

struct remove_one_data {
    uint32_t ssrc;
    srtp_stream_list_t list;
};

bool stream_list_test_remove_one_cb(srtp_stream_t stream, void *data)
{
    struct remove_one_data *d = (struct remove_one_data *)data;
    if (stream->ssrc == d->ssrc) {
        srtp_stream_list_remove(d->list, stream);
        stream_list_test_free_stream(stream);
        return false;
    }
    return true;
}

bool stream_list_test_remove_all_cb(srtp_stream_t stream, void *data)
{
    srtp_stream_list_t *list = (srtp_stream_list_t *)data;
    srtp_stream_list_remove(*list, stream);
    stream_list_test_free_stream(stream);
    return true;
}

srtp_err_status_t srtp_stream_list_test(void)
{
    srtp_stream_list_t list;

    if (srtp_stream_list_alloc(&list)) {
        return srtp_err_status_fail;
    }

    /* add 4 streams */
    if (srtp_stream_list_insert(list, stream_list_test_create_stream(1))) {
        return srtp_err_status_fail;
    }
    if (srtp_stream_list_insert(list, stream_list_test_create_stream(2))) {
        return srtp_err_status_fail;
    }
    if (srtp_stream_list_insert(list, stream_list_test_create_stream(3))) {
        return srtp_err_status_fail;
    }
    if (srtp_stream_list_insert(list, stream_list_test_create_stream(4))) {
        return srtp_err_status_fail;
    }

    /* find */
    if (srtp_stream_list_get(list, 3) == NULL) {
        return srtp_err_status_fail;
    }
    if (srtp_stream_list_get(list, 1) == NULL) {
        return srtp_err_status_fail;
    }
    if (srtp_stream_list_get(list, 2) == NULL) {
        return srtp_err_status_fail;
    }
    if (srtp_stream_list_get(list, 4) == NULL) {
        return srtp_err_status_fail;
    }

    /* find not in list */
    if (srtp_stream_list_get(list, 5)) {
        return srtp_err_status_fail;
    }

    /* for each */
    size_t count = 0;
    srtp_stream_list_for_each(list, stream_list_test_count_cb, &count);
    if (count != 4) {
        return srtp_err_status_fail;
    }

    /* remove */
    srtp_stream_t stream = srtp_stream_list_get(list, 3);
    if (stream == NULL) {
        return srtp_err_status_fail;
    }
    srtp_stream_list_remove(list, stream);
    stream_list_test_free_stream(stream);

    /* find after remove */
    if (srtp_stream_list_get(list, 3)) {
        return srtp_err_status_fail;
    }

    /* recount */
    count = 0;
    srtp_stream_list_for_each(list, stream_list_test_count_cb, &count);
    if (count != 3) {
        return srtp_err_status_fail;
    }

    /* remove one in for each */
    struct remove_one_data data = { 2, list };
    srtp_stream_list_for_each(list, stream_list_test_remove_one_cb, &data);

    /* find after remove */
    if (srtp_stream_list_get(list, 2)) {
        return srtp_err_status_fail;
    }

    /* recount */
    count = 0;
    srtp_stream_list_for_each(list, stream_list_test_count_cb, &count);
    if (count != 2) {
        return srtp_err_status_fail;
    }

    /* destroy non empty list */
    if (srtp_stream_list_dealloc(list) == srtp_err_status_ok) {
        return srtp_err_status_fail;
    }

    /* remove all in for each */
    srtp_stream_list_for_each(list, stream_list_test_remove_all_cb, &list);

    /* recount */
    count = 0;
    srtp_stream_list_for_each(list, stream_list_test_count_cb, &count);
    if (count != 0) {
        return srtp_err_status_fail;
    }

    /* destroy empty list */
    if (srtp_stream_list_dealloc(list)) {
        return srtp_err_status_fail;
    }

    return srtp_err_status_ok;
}

#ifdef SRTP_USE_TEST_STREAM_LIST

/*
 * A srtp_stream_list_ctx_t implementation using a single linked list
 * that does not use the internal next / prev fields.
 */

struct test_list_node {
    srtp_stream_t stream;
    struct test_list_node *next;
};
struct srtp_stream_list_ctx_t_ {
    struct test_list_node *head;
};

srtp_err_status_t srtp_stream_list_alloc(srtp_stream_list_t *list_ptr)
{
    struct srtp_stream_list_ctx_t_ *l =
        malloc(sizeof(struct srtp_stream_list_ctx_t_));
    l->head = NULL;
    *list_ptr = l;
    return srtp_err_status_ok;
}

srtp_err_status_t srtp_stream_list_dealloc(srtp_stream_list_t list)
{
    struct test_list_node *node = list->head;
    if (node) {
        return srtp_err_status_fail;
    }
    free(list);

    return srtp_err_status_ok;
}

srtp_err_status_t srtp_stream_list_insert(srtp_stream_list_t list,
                                          srtp_stream_t stream)
{
    struct test_list_node *node = malloc(sizeof(struct test_list_node));
    node->stream = stream;
    node->next = list->head;
    list->head = node;

    return srtp_err_status_ok;
}

srtp_stream_t srtp_stream_list_get(srtp_stream_list_t list, uint32_t ssrc)
{
    struct test_list_node *node = list->head;
    while (node != NULL) {
        if (node->stream->ssrc == ssrc) {
            return node->stream;
        }
        node = node->next;
    }
    return NULL;
}

void srtp_stream_list_remove(srtp_stream_list_t list, srtp_stream_t stream)
{
    struct test_list_node **node = &(list->head);
    while ((*node) != NULL) {
        if ((*node)->stream->ssrc == stream->ssrc) {
            struct test_list_node *tmp = (*node);
            (*node) = tmp->next;
            free(tmp);
            return;
        }
        node = &(*node)->next;
    }
}

void srtp_stream_list_for_each(srtp_stream_list_t list,
                               bool (*callback)(srtp_stream_t, void *),
                               void *data)
{
    struct test_list_node *node = list->head;
    while (node != NULL) {
        struct test_list_node *tmp = node;
        node = node->next;
        if (!callback(tmp->stream, data)) {
            break;
        }
    }
}

#endif
