/*
 * Copyright (C) 2025 Infineon Technologies
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <bootutil/sign_key.h>

extern const unsigned char ecdsa_pub_key[];
extern const unsigned int ecdsa_pub_key_len;
const struct bootutil_key bootutil_keys[] = {
    {
        .key = ecdsa_pub_key,
        .len = &ecdsa_pub_key_len,
    }
};
const int bootutil_key_cnt = 1;

#if (ENC_IMG==1)
extern const unsigned char enc_priv_key[];
extern const unsigned int enc_priv_key_len;
#else
const uint8_t * const enc_priv_key = 0;
const unsigned int enc_priv_key_len;
#endif
const struct bootutil_key bootutil_enc_key = {
    .key = enc_priv_key,
    .len = &enc_priv_key_len,
};
