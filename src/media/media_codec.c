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
#include "media.h"
#include <libavcodec/codec.h>

static LIST_HEAD(, codec_def) registeredcodecs;

/**
 *
 */
media_codec_t *
media_codec_ref(media_codec_t *cw)
{
  atomic_inc(&cw->refcount);
  return cw;
}

/**
 *
 */
void
media_codec_deref(media_codec_t *cw)
{
  if(atomic_dec(&cw->refcount))
    return;
#if ENABLE_FFMPEG
  if(cw->ctx != NULL && cw->ctx->codec != NULL)
    avcodec_free_context(&cw->ctx);
  else if(cw->ctx != NULL)
    free(cw->ctx);  // Only free if avcodec_free_context wasn't called

  if(cw->codec_par != NULL && cw->fw == NULL)
    avcodec_parameters_free(&cw->codec_par);
#endif

  if(cw->close != NULL)
    cw->close(cw);

#if ENABLE_FFMPEG
  if(cw->parser_ctx != NULL)
    av_parser_close(cw->parser_ctx);

  if(cw->fw != NULL)
    media_format_deref(cw->fw);
#endif

  free(cw);
}


/**
 *
 */
media_codec_t *
media_codec_create(int codec_id, int parser,
		   struct media_format *fw, struct AVCodecParameters *par,
		   const media_codec_params_t *mcp, media_pipe_t *mp)
{
  media_codec_t *mc = calloc(1, sizeof(media_codec_t));
  codec_def_t *cd;

  mc->mp = mp;
  mc->codec_par = par;
  mc->codec_id = codec_id;

#if ENABLE_FFMPEG
  if(par != NULL && mcp != NULL) {
    assert(par->extradata      == mcp->extradata);
    assert(par->extradata_size == mcp->extradata_size);
  }
#endif

  if(mcp != NULL) {
    mc->sar_num = mcp->sar_num;
    mc->sar_den = mcp->sar_den;
  }

  LIST_FOREACH(cd, &registeredcodecs, link)
    if(!cd->open(mc, mcp, mp))
      break;

  if(cd == NULL) {
    free(mc);
    return NULL;
  }

#if ENABLE_FFMPEG
  if(parser) {
    assert(fw == NULL);

    mc->codec_par = avcodec_parameters_alloc();
    mc->parser_ctx = av_parser_init(codec_id);
  }
#endif

  atomic_set(&mc->refcount, 1);
  mc->fw = fw;

  if(fw != NULL) {
    assert(!parser);
    atomic_inc(&fw->refcount);
  }

  return mc;
}


/**
 *
 */
void
media_codec_init(void)
{
  codec_def_t *cd;
  LIST_FOREACH(cd, &registeredcodecs, link)
    if(cd->init)
      cd->init();

  // Debug: check if decoders can be found
  const AVCodec *h264 = avcodec_find_decoder(AV_CODEC_ID_H264);
  const AVCodec *vorbis = avcodec_find_decoder(AV_CODEC_ID_VORBIS);
  TRACE(TRACE_INFO, "media", "FFmpeg decoders check: h264=%p, vorbis=%p", h264, vorbis);
}





/**
 *
 */
static int
codec_def_cmp(const codec_def_t *a, const codec_def_t *b)
{
  return a->prio - b->prio;
}

/**
 *
 */
void
media_register_codec(codec_def_t *cd)
{
  LIST_INSERT_SORTED(&registeredcodecs, cd, link, codec_def_cmp, codec_def_t);
}

