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

#include "main.h"
#include <errno.h>
#include "net_i.h"

#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/ssl.h>
#include <mbedtls/error.h>


/* =========================
 * BIO callbacks (mbedTLS)
 * ========================= */

static int raw_recv(void *ctx, unsigned char *buf, size_t len)
{
  tcpcon_t *tc = ctx;

  int ret = tc->raw_read(tc, buf, len, 0, NULL, NULL);
  if(ret <= 0)
    return MBEDTLS_ERR_SSL_CONN_EOF;

  return ret;
}

static int raw_send(void *ctx, const unsigned char *buf, size_t len)
{
  tcpcon_t *tc = ctx;

  int ret = tc->raw_write(tc, buf, len);
  if(ret < 0)
    return MBEDTLS_ERR_SSL_CONN_EOF;

  return len;
}


/* =========================
 * READ / WRITE wrappers
 * ========================= */

static int
mbedtls_read(tcpcon_t *tc, void *buf, size_t len, int all,
              net_read_cb_t *cb, void *opaque)
{
  int ret, tot = 0;

  if(!all) {
    ret = mbedtls_ssl_read(&tc->ssl, buf, len);
    return ret > 0 ? ret : -1;
  }

  while(tot < len) {
    ret = mbedtls_ssl_read(&tc->ssl, (unsigned char*)buf + tot, len - tot);

    if(ret <= 0)
      return -1;

    tot += ret;

    if(cb)
      cb(opaque, tot);
  }

  return tot;
}

static int
mbedtls_write(tcpcon_t *tc, const void *data, size_t len)
{
  int ret = mbedtls_ssl_write(&tc->ssl, data, len);
  return (ret == (int)len) ? 0 : ECONNRESET;
}


/* =========================
 * DEBUG
 * ========================= */

static void printdbg(void *ctx, int level, const char *file, int line, const char *str)
{
  (void)ctx;
  (void)level;
  (void)file;
  (void)line;
  fprintf(stdout, "%s", str);
  fflush(stdout);
}


/* =========================
 * MAIN TLS OPEN
 * ========================= */

int
tcp_ssl_open(tcpcon_t *tc, char *errbuf, size_t errlen,
             const char *hostname, int verify)
{
  int ret;

  /* init structures */
  mbedtls_ssl_init(&tc->ssl);
  mbedtls_ssl_config_init(&tc->conf);
  mbedtls_ctr_drbg_init(&tc->ctr_drbg);
  mbedtls_entropy_init(&tc->entropy);

  /* seed RNG */
  ret = mbedtls_ctr_drbg_seed(
      &tc->ctr_drbg,
      mbedtls_entropy_func,
      &tc->entropy,
      (const unsigned char *)gconf.device_id,
      sizeof(gconf.device_id)
  );

  if(ret != 0) {
    mbedtls_strerror(ret, errbuf, errlen);
    return -1;
  }

  /* config */
  ret = mbedtls_ssl_config_defaults(
      &tc->conf,
      MBEDTLS_SSL_IS_CLIENT,
      MBEDTLS_SSL_TRANSPORT_STREAM,
      MBEDTLS_SSL_PRESET_DEFAULT
  );

  if(ret != 0) {
    mbedtls_strerror(ret, errbuf, errlen);
    return -1;
  }

  mbedtls_ssl_conf_authmode(
      &tc->conf,
      verify ? MBEDTLS_SSL_VERIFY_REQUIRED : MBEDTLS_SSL_VERIFY_NONE
  );

  mbedtls_ssl_conf_rng(
      &tc->conf,
      mbedtls_ctr_drbg_random,
      &tc->ctr_drbg
  );

  mbedtls_ssl_conf_dbg(&tc->conf, printdbg, NULL);

  /* setup SSL context */
  ret = mbedtls_ssl_setup(&tc->ssl, &tc->conf);
  if(ret != 0) {
    mbedtls_strerror(ret, errbuf, errlen);
    return -1;
  }

  /* hostname */
  if(hostname) {
    mbedtls_ssl_set_hostname(&tc->ssl, hostname);
  }

  /* bind socket */
  mbedtls_ssl_set_bio(
      &tc->ssl,
      tc,
      raw_send,
      raw_recv,
      NULL
  );

  /* handshake */
  while((ret = mbedtls_ssl_handshake(&tc->ssl)) != 0) {
    if(ret != MBEDTLS_ERR_SSL_WANT_READ &&
       ret != MBEDTLS_ERR_SSL_WANT_WRITE) {

      mbedtls_strerror(ret, errbuf, errlen);
      return -1;
    }
  }

  /* switch to TLS I/O */
  tc->raw_read  = tc->read;
  tc->raw_write = tc->write;

  tc->read  = mbedtls_read;
  tc->write = mbedtls_write;

  return 0;
}


/* =========================
 * CLOSE
 * ========================= */

void
tcp_ssl_close(tcpcon_t *tc)
{
  mbedtls_ssl_close_notify(&tc->ssl);

  mbedtls_ssl_free(&tc->ssl);
  mbedtls_ssl_config_free(&tc->conf);
  mbedtls_ctr_drbg_free(&tc->ctr_drbg);
  mbedtls_entropy_free(&tc->entropy);
}