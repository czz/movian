/*
 *  Copyright (C) 2007-2015 Lonelycoder AB
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  This program is also available under a commercial proprietary license.
 *  For more information, contact andreas@lonelycoder.com
 */
#pragma once
#include "config.h"

#if ENABLE_COMMONCRYPTO

#include <CommonCrypto/CommonDigest.h>

#define sha1_decl(ctx) CC_SHA1_CTX ctx

#define sha1_init(ctx) CC_SHA1_Init(&ctx)

#define sha1_update(ctx, data, len) CC_SHA1_Update(&ctx, data, len)

#define sha1_final(ctx, output) CC_SHA1_Final(output, &ctx)

#elif ENABLE_MBEDTLS

#include <mbedtls/sha1.h>

#define sha1_decl(ctx) mbedtls_sha1_context ctx

#define sha1_init(ctx) \
    do { \
        mbedtls_sha1_init(&ctx); \
        mbedtls_sha1_starts_ret(&ctx); \
    } while(0)

#define sha1_update(ctx, data, len) \
    mbedtls_sha1_update_ret(&ctx, data, len)

#define sha1_final(ctx, output) \
    do { \
        mbedtls_sha1_finish_ret(&ctx, output); \
        mbedtls_sha1_free(&ctx); \
    } while(0)

#elif ENABLE_FFMPEG

#include <libavutil/sha.h>
#include <libavutil/mem.h>

#define sha1_decl(ctx) struct AVSHA *ctx = NULL;

#define sha1_init(ctx) do {                     \
  ctx = av_sha_alloc();                         \
  av_sha_init(ctx, 160);                        \
  } while(0)

#define sha1_update(ctx, data, len) av_sha_update(ctx, data, len)

#define sha1_final(ctx, output) do {            \
  av_sha_final(ctx, output);                    \
  av_freep(&ctx);                               \
  } while(0)
#elif ENABLE_OPENSSL

#include <openssl/sha.h>

#define sha1_decl(ctx) SHA_CTX ctx

#define sha1_init(ctx) SHA1_Init(&ctx)

#define sha1_update(ctx, data, len) SHA1_Update(&ctx, data, len)

#define sha1_final(ctx, output) SHA1_Final(output, &ctx)

#else
#error no sha1
#endif
