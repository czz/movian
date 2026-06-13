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

#define md5_decl(ctx) CC_MD5_CTX ctx

#define md5_init(ctx) CC_MD5_Init(&ctx)

#define md5_update(ctx, data, len) CC_MD5_Update(&ctx, data, len)

#define md5_final(ctx, output) CC_MD5_Final(output, &ctx)

#elif ENABLE_FFMPEG

#include <libavutil/md5.h>
#include <libavutil/mem.h>

#define md5_decl(ctx) struct AVMD5 *ctx = NULL;

#define md5_init(ctx) do {                      \
  ctx = av_md5_alloc();                         \
  av_md5_init(ctx);                             \
  } while(0)

#define md5_update(ctx, data, len) av_md5_update(ctx, data, len)

#define md5_final(ctx, output) do {             \
  av_md5_final(ctx, output);                    \
  av_freep(&ctx);                               \
  } while(0)

#elif ENABLE_MBEDTLS

#include <mbedtls/md5.h>

/* stato MD5 */
#define md5_decl(ctx) mbedtls_md5_context ctx

#define md5_init(ctx) \
    do { \
        mbedtls_md5_init(&ctx); \
        mbedtls_md5_starts_ret(&ctx); \
    } while(0)

#define md5_update(ctx, data, len) \
    mbedtls_md5_update_ret(&ctx, data, len)

#define md5_final(ctx, output) \
    do { \
        mbedtls_md5_finish_ret(&ctx, output); \
        mbedtls_md5_free(&ctx); \
    } while(0)

#elif ENABLE_OPENSSL

#include <openssl/md5.h>

#define md5_decl(ctx) MD5_CTX ctx

#define md5_init(ctx) MD5_Init(&ctx)

#define md5_update(ctx, data, len) MD5_Update(&ctx, data, len)

#define md5_final(ctx, output) MD5_Final(output, &ctx)

#else

#error No md5 crypto

#endif
