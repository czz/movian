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

#define md4_decl(ctx) CC_MD4_CTX ctx

#define md4_init(ctx) CC_MD4_Init(&ctx)

#define md4_update(ctx, data, len) CC_MD4_Update(&ctx, data, len)

#define md4_final(ctx, output) CC_MD4_Final(output, &ctx)

#elif ENABLE_MBEDTLS

#include <mbedtls/md4.h>

/* stato interno mbedTLS */
#define md4_decl(ctx) mbedtls_md4_context ctx

#define md4_init(ctx) \
    do { \
        mbedtls_md4_init(&ctx); \
        mbedtls_md4_starts_ret(&ctx); \
    } while(0)

#define md4_update(ctx, data, len) \
    mbedtls_md4_update_ret(&ctx, data, len)

#define md4_final(ctx, output) \
    do { \
        mbedtls_md4_finish_ret(&ctx, output); \
        mbedtls_md4_free(&ctx); \
    } while(0)

#elif ENABLE_OPENSSL

#include <openssl/md4.h>

#define md4_decl(ctx) MD4_CTX ctx

#define md4_init(ctx) MD4_Init(&ctx)

#define md4_update(ctx, data, len) MD4_Update(&ctx, data, len)

#define md4_final(ctx, output) MD4_Final(output, &ctx)

#else
#error no md4 backend
#endif