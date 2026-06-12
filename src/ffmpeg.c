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
#include <ctype.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/pixdesc.h>
#include <libavutil/channel_layout.h>

#include "main.h"
#include "media/media.h"
#include "ffmpeg.h"
#include "fileaccess/fa_ffmpeg.h"
#include "video/video_decoder.h"
#include "video/video_settings.h"

#if ENABLE_VDPAU
#include "video/vdpau.h"
#endif




static const int ffmpeg_colorspace_tbl[] = {
  [AVCOL_SPC_BT709]     = COLOR_SPACE_BT_709,
  [AVCOL_SPC_BT470BG]   = COLOR_SPACE_BT_601,
  [AVCOL_SPC_SMPTE170M] = COLOR_SPACE_BT_601,
  [AVCOL_SPC_SMPTE240M] = COLOR_SPACE_SMPTE_240M,
};


#define vd_valid_duration(t) ((t) > 10000ULL && (t) < 1000000ULL)


/**
 *
 */
static void
ffmpeg_deliver_frame(video_decoder_t *vd,
                    media_pipe_t *mp, media_queue_t *mq,
                    AVCodecContext *ctx, AVFrame *frame,
                    const media_buf_meta_t *mbm, int decode_time,
                    const media_codec_t *mc)
{
  frame_info_t fi;

  /* Compute aspect ratio */
  switch(mbm->mbm_aspect_override) {
  case 0:

    fi.fi_dar_num = frame->width;
    fi.fi_dar_den = frame->height;

    if(frame->sample_aspect_ratio.num) {
      fi.fi_dar_num *= frame->sample_aspect_ratio.num;
      fi.fi_dar_den *= frame->sample_aspect_ratio.den;
    } else if(mc->sar_num) {
      fi.fi_dar_num *= mc->sar_num;
      fi.fi_dar_den *= mc->sar_den;
    }

    break;
  case 1:
    fi.fi_dar_num = 4;
    fi.fi_dar_den = 3;
    break;
  case 2:
    fi.fi_dar_num = 16;
    fi.fi_dar_den = 9;
    break;
  }

  int64_t pts = video_decoder_infer_pts(mbm, vd,
					frame->pict_type == AV_PICTURE_TYPE_B);

  int duration = mbm->mbm_duration;

  if(!vd_valid_duration(duration)) {
    /* duration is zero or very invalid, use duration from last output */
    duration = vd->vd_estimated_duration;
  }

  if(pts == AV_NOPTS_VALUE && vd->vd_nextpts != AV_NOPTS_VALUE)
    pts = vd->vd_nextpts; /* no pts set, use estimated pts */

  if(pts != AV_NOPTS_VALUE && vd->vd_prevpts != AV_NOPTS_VALUE) {
    /* we know PTS of a prior frame */
    int64_t t = (pts - vd->vd_prevpts) / vd->vd_prevpts_cnt;

    if(vd_valid_duration(t)) {
      /* inter frame duration seems valid, store it */
      vd->vd_estimated_duration = t;
      if(duration == 0)
	duration = t;

    }
  }
  
  duration += frame->repeat_pict * duration / 2;
 
  if(pts != AV_NOPTS_VALUE) {
    vd->vd_prevpts = pts;
    vd->vd_prevpts_cnt = 0;
  }
  vd->vd_prevpts_cnt++;

  if(duration == 0) {
    TRACE(TRACE_DEBUG, "Video", "Dropping frame with duration = 0");
    return;
  }

  prop_set_int(mq->mq_prop_too_slow, decode_time > duration);

  if(pts != AV_NOPTS_VALUE) {
    vd->vd_nextpts = pts + duration;
  } else {
    vd->vd_nextpts = AV_NOPTS_VALUE;
  }
#if 0
  static int64_t lastpts = AV_NOPTS_VALUE;
  if(lastpts != AV_NOPTS_VALUE) {
    printf(" VDEC: %20"PRId64" : %-20"PRId64" %d %"PRId64" %6d %d epoch=%d\n", pts, pts - lastpts, mbm->mbm_drive_clock,
           mbm->mbm_user_time, duration, mbm->mbm_sequence, mbm->mbm_epoch);
#if 0
    if(pts - lastpts > 1000000) {
      abort();
    }
    #endif
  }
  lastpts = pts;
#endif


  media_discontinuity_debug(&vd->vd_debug_discont_out,
                            mbm->mbm_dts,
                            mbm->mbm_pts,
                            mbm->mbm_epoch,
                            mbm->mbm_skip,
                            "VOUT");

  vd->vd_interlaced |=
    (frame->flags & AV_FRAME_FLAG_INTERLACED) && !mbm->mbm_disable_deinterlacer;

  fi.fi_width = frame->width;
  fi.fi_height = frame->height;
  fi.fi_pts = pts;
  fi.fi_epoch = mbm->mbm_epoch;
  fi.fi_user_time = mbm->mbm_user_time;
  fi.fi_duration = duration;
  fi.fi_drive_clock = mbm->mbm_drive_clock;

  fi.fi_interlaced = !!vd->vd_interlaced;
  fi.fi_tff = !!(frame->flags & AV_FRAME_FLAG_TOP_FIELD_FIRST);
  fi.fi_prescaled = 0;

  fi.fi_color_space = 
    ctx->colorspace < ARRAYSIZE(ffmpeg_colorspace_tbl) ? 
    ffmpeg_colorspace_tbl[ctx->colorspace] : 0;

  fi.fi_type = 'LAVC';

  // Check if we should skip directly to convert code
  if(vd->vd_convert_width  != frame->width ||
     vd->vd_convert_height != frame->height ||
     vd->vd_convert_pixfmt != frame->format) {

    // Nope, go ahead and deliver frame as-is

    fi.fi_data[0] = frame->data[0];
    fi.fi_data[1] = frame->data[1];
    fi.fi_data[2] = frame->data[2];

    fi.fi_pitch[0] = frame->linesize[0];
    fi.fi_pitch[1] = frame->linesize[1];
    fi.fi_pitch[2] = frame->linesize[2];

    fi.fi_pix_fmt = frame->format;
    fi.fi_avframe = frame;

    int r = video_deliver_frame(vd, &fi);

    /* return value
     * 0  = OK
     * 1  = Need convert to YUV420P
     * -1 = Fail
     */

    if(r != 1)
      return;
  }

  // Need to convert frame

  vd->vd_sws =
    sws_getCachedContext(vd->vd_sws,
                         frame->width, frame->height, frame->format,
                         frame->width, frame->height, AV_PIX_FMT_YUV420P,
                         0, NULL, NULL, NULL);

  if(vd->vd_sws == NULL) {
    TRACE(TRACE_ERROR, "Video", "Unable to convert from %s to %s",
	  av_get_pix_fmt_name(frame->format),
	  av_get_pix_fmt_name(AV_PIX_FMT_YUV420P));
    return;
  }

  if(vd->vd_convert_width  != frame->width  ||
     vd->vd_convert_height != frame->height ||
     vd->vd_convert_pixfmt != frame->format) {
    if(vd->vd_convert != NULL) {
      av_frame_free(&vd->vd_convert);
    }

    vd->vd_convert_width  = frame->width;
    vd->vd_convert_height = frame->height;
    vd->vd_convert_pixfmt = frame->format;

    vd->vd_convert = av_frame_alloc();
    vd->vd_convert->format = AV_PIX_FMT_YUV420P;
    vd->vd_convert->width = frame->width;
    vd->vd_convert->height = frame->height;
    av_frame_get_buffer(vd->vd_convert, 0);

    TRACE(TRACE_DEBUG, "Video", "Converting from %s to %s",
	  av_get_pix_fmt_name(frame->format),
	  av_get_pix_fmt_name(AV_PIX_FMT_YUV420P));
  }

  sws_scale(vd->vd_sws, (void *)frame->data, frame->linesize, 0,
            frame->height, vd->vd_convert->data, vd->vd_convert->linesize);

  fi.fi_data[0] = vd->vd_convert->data[0];
  fi.fi_data[1] = vd->vd_convert->data[1];
  fi.fi_data[2] = vd->vd_convert->data[2];

  fi.fi_pitch[0] = vd->vd_convert->linesize[0];
  fi.fi_pitch[1] = vd->vd_convert->linesize[1];
  fi.fi_pitch[2] = vd->vd_convert->linesize[2];

  fi.fi_type = 'LAVC';
  fi.fi_pix_fmt = AV_PIX_FMT_YUV420P;
  fi.fi_avframe = NULL;
  video_deliver_frame(vd, &fi);
}



/**
 *
 */
static void
ffmpeg_video_flush(media_codec_t *mc, video_decoder_t *vd)
{
  AVCodecContext *ctx = mc->ctx;
  AVFrame *frame = vd->vd_frame;
  AVPacket avpkt;

  av_init_packet(&avpkt);
  avpkt.data = NULL;
  avpkt.size = 0;

  // Send NULL packet to flush decoder
  avcodec_send_packet(ctx, &avpkt);
  
  while(1) {
    int ret = avcodec_receive_frame(ctx, frame);
    if(ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
      break;
    if(ret < 0)
      break;
    av_frame_unref(frame);
  };
  avcodec_flush_buffers(ctx);
}


/**
 *
 */
static void
ffmpeg_video_eof(media_codec_t *mc, video_decoder_t *vd,
                struct media_queue *mq)
{
  media_pipe_t *mp = vd->vd_mp;
  AVCodecContext *ctx = mc->ctx;
  AVFrame *frame = vd->vd_frame;
  AVPacket avpkt;
  int t;

  av_init_packet(&avpkt);
  avpkt.data = NULL;
  avpkt.size = 0;

  // Send NULL packet to flush decoder
  avcodec_send_packet(ctx, &avpkt);
  
  while(1) {
    avgtime_start(&vd->vd_decode_time);

    int ret = avcodec_receive_frame(ctx, frame);

    t = avgtime_stop(&vd->vd_decode_time, mq->mq_prop_decode_avg,
                     mq->mq_prop_decode_peak);

    if(ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
      break;
    if(ret < 0)
      break;
    
    // reordered_opaque was removed in FFmpeg 5.0+
    // Use vd_reorder_ptr directly as fallback
    const media_buf_meta_t *mbm = &vd->vd_reorder[vd->vd_reorder_ptr];
    if(!mbm->mbm_skip)
      ffmpeg_deliver_frame(vd, mp, mq, ctx, frame, mbm, t, mc);
    av_frame_unref(frame);
  };
  avcodec_flush_buffers(ctx);
}

#include "misc/minmax.h"

/**
 *
 */
static void
ffmpeg_decode_video(struct media_codec *mc, struct video_decoder *vd,
                   struct media_queue *mq, struct media_buf *mb, int reqsize)
{
  media_pipe_t *mp = vd->vd_mp;
  AVCodecContext *ctx = mc->ctx;
  AVFrame *frame = vd->vd_frame;
  int t;

  if(mb->mb_flush)
    ffmpeg_video_eof(mc, vd, mq);

  copy_mbm_from_mb(&vd->vd_reorder[vd->vd_reorder_ptr], mb);
  // reordered_opaque was removed in FFmpeg 5.0+
  // Store the reorder_ptr in a local variable instead
  int current_reorder_ptr = vd->vd_reorder_ptr;
  vd->vd_reorder_ptr = (vd->vd_reorder_ptr + 1) & VIDEO_DECODER_REORDER_MASK;

  /*
   * If we are seeking, drop any non-reference frames
   */
  ctx->skip_frame = mb->mb_skip == 1 ? AVDISCARD_NONREF : AVDISCARD_DEFAULT;
  avgtime_start(&vd->vd_decode_time);

    int ret = avcodec_send_packet(ctx, &mb->mb_pkt);
  if(ret < 0 && ret != AVERROR(EAGAIN)) {
    return;
  }

  ret = avcodec_receive_frame(ctx, frame);

  t = avgtime_stop(&vd->vd_decode_time, mq->mq_prop_decode_avg,
		   mq->mq_prop_decode_peak);

  mp_set_mq_meta(mq, ctx->codec, ctx);

  if(ret < 0)
    return;

  // FFmpeg 6.0: Transfer hardware frames to CPU if needed
  AVFrame *sw_frame = frame;
  AVFrame *hw_frame = NULL;
  
  if(frame->format == AV_PIX_FMT_VDPAU) {
    hw_frame = frame;
    sw_frame = av_frame_alloc();
    if(!sw_frame) {
      av_frame_unref(frame);
      return;
    }
    
    // Transfer data from GPU to CPU
    int transfer_ret = av_hwframe_transfer_data(sw_frame, hw_frame, 0);
    if(transfer_ret < 0) {
      char errbuf[128];
      av_strerror(transfer_ret, errbuf, sizeof(errbuf));
      TRACE(TRACE_ERROR, "ffmpeg", "Failed to transfer frame: %s (%d)", errbuf, transfer_ret);
      av_frame_free(&sw_frame);
      av_frame_unref(frame);
      return;
    }
    
    // Copy properties from hardware frame to software frame
    sw_frame->pts = hw_frame->pts;
    sw_frame->width = hw_frame->width;
    sw_frame->height = hw_frame->height;
  }

  const media_buf_meta_t *mbm = &vd->vd_reorder[current_reorder_ptr];
  if(!mbm->mbm_skip)
    ffmpeg_deliver_frame(vd, mp, mq, ctx, sw_frame, mbm, t, mc);
  
  if(hw_frame) {
    av_frame_free(&sw_frame);
    av_frame_unref(hw_frame);
  } else {
    av_frame_unref(frame);
  }
}


/**
 *
 */
static enum AVPixelFormat
ffmpeg_get_format(struct AVCodecContext *ctx, const enum AVPixelFormat *fmt)
{
  media_codec_t *mc = ctx->opaque;
  
  if(mc->close != NULL) {
    mc->close(mc);
    mc->close = NULL;
  }

#if ENABLE_VDPAU
  TRACE(TRACE_INFO, "ffmpeg", "Attempting VDPAU initialization");
  if(!vdpau_init_ffmpeg_decode(mc, ctx)) {
    TRACE(TRACE_INFO, "ffmpeg", "VDPAU initialized successfully, returning AV_PIX_FMT_VDPAU");
    return AV_PIX_FMT_VDPAU;
  }
  TRACE(TRACE_INFO, "ffmpeg", "VDPAU initialization failed, falling back to software");
#endif

  // Prefer YUV420P over other formats for better compatibility
  for(const enum AVPixelFormat *p = fmt; *p != AV_PIX_FMT_NONE; p++) {
    if(*p == AV_PIX_FMT_YUV420P) {
      mc->get_buffer2 = &avcodec_default_get_buffer2;
      TRACE(TRACE_INFO, "ffmpeg", "Returning AV_PIX_FMT_YUV420P (software decoding)");
      return AV_PIX_FMT_YUV420P;
    }
  }

  mc->get_buffer2 = &avcodec_default_get_buffer2;
  return avcodec_default_get_format(ctx, fmt);
}


/**
 *
 */
static int
get_buffer2_wrapper(struct AVCodecContext *s, AVFrame *frame, int flags)
{
  media_codec_t *mc = s->opaque;
  return mc->get_buffer2(s, frame, flags);
}

/**
 *
 */
static int
media_codec_create_lavc(media_codec_t *cw, const media_codec_params_t *mcp,
                        media_pipe_t *mp)
{
  // Ensure codec list is initialized in this thread
  void *opaque = NULL;
  av_codec_iterate(&opaque);

  const AVCodec *codec = avcodec_find_decoder(cw->codec_id);

  TRACE(TRACE_INFO, "ffmpeg", "Looking for decoder for codec_id %d (AV_CODEC_ID_H264=%d, AV_CODEC_ID_VORBIS=%d)", 
        cw->codec_id, AV_CODEC_ID_H264, AV_CODEC_ID_VORBIS);

  if(codec == NULL) {
    TRACE(TRACE_INFO, "ffmpeg", "avcodec_find_decoder failed, giving up");
    return -1;
  }

  TRACE(TRACE_INFO, "ffmpeg", "Codec found: %s, attempting avcodec_open2", codec->name);

  cw->ctx = avcodec_alloc_context3(codec);
  if(cw->codec_par != NULL) {
    // Use avcodec_parameters_to_context with codec_par
    int ret = avcodec_parameters_to_context(cw->ctx, cw->codec_par);
    if(ret < 0) {
      char errbuf[128];
      av_strerror(ret, errbuf, sizeof(errbuf));
      TRACE(TRACE_INFO, "ffmpeg", "avcodec_parameters_to_context failed: %s (error %d)", errbuf, ret);
    } else {
      TRACE(TRACE_INFO, "ffmpeg", "Copied parameters from codec_par successfully");
    }
  } else {
    TRACE(TRACE_INFO, "ffmpeg", "codec_par is NULL, using default codec context");
  }

  // cw->ctx->debug = FF_DEBUG_PICT_INFO | FF_DEBUG_BUGS;

  if(mcp != NULL && mcp->extradata != NULL && !cw->ctx->extradata) {
    cw->ctx->extradata = calloc(1, mcp->extradata_size +
				AV_INPUT_BUFFER_PADDING_SIZE);
    memcpy(cw->ctx->extradata, mcp->extradata, mcp->extradata_size);
    cw->ctx->extradata_size = mcp->extradata_size;
  }

  if(mcp && mcp->cheat_for_speed)
    cw->ctx->flags2 |= AV_CODEC_FLAG2_FAST;

  if(codec->type == AVMEDIA_TYPE_VIDEO) {

    cw->get_buffer2 = &avcodec_default_get_buffer2;

    // If we run with vdpau and h264 libav will crash when going
    // back and forth between accelerated and non-accelerated mode
    // Skip thread_count setting for FFmpeg 6.0 compatibility
    // if(!(video_settings.vdpau && cw->codec_id == AV_CODEC_ID_H264))
    //   cw->ctx->thread_count = gconf.concurrency;

    cw->ctx->opaque = cw;
    // refcounted_frames was removed in FFmpeg 5.0+
    cw->ctx->get_format = &ffmpeg_get_format;
    cw->ctx->get_buffer2 = &get_buffer2_wrapper;

    cw->decode = &ffmpeg_decode_video;
    cw->flush  = &ffmpeg_video_flush;
  }

  int ret = avcodec_open2(cw->ctx, codec, NULL);
  if(ret < 0) {
    char errbuf[128];
    av_strerror(ret, errbuf, sizeof(errbuf));
    TRACE(TRACE_INFO, "ffmpeg", "Unable to open codec %s: %s (error code %d)",
	  codec ? codec->name : "<noname>", errbuf, ret);

    av_freep(&cw->ctx);

    return -1;
  }

  return 0;
}


REGISTER_CODEC(NULL, media_codec_create_lavc, 1000);

/**
 *
 */
media_format_t *
media_format_create(AVFormatContext *fctx)
{
  media_format_t *fw = malloc(sizeof(media_format_t));
  atomic_set(&fw->refcount, 1);
  fw->fctx = fctx;
  return fw;
}


/**
 *
 */
void
media_format_deref(media_format_t *fw)
{
  if(atomic_dec(&fw->refcount))
    return;
  fa_ffmpeg_close_format(fw->fctx, 0);
  free(fw);
}


/**
 *
 */
void
metadata_from_ffmpeg(char *dst, size_t dstlen,
		    const AVCodec *codec, const AVCodecParameters *par)
{
  const char *name = codec->name;
  const char *profile = av_get_profile_name(codec, par->profile);

  if(codec->id == AV_CODEC_ID_DTS && profile != NULL)
    name = NULL;

  int off = 0;

  if(name) {
    off = snprintf(dst, dstlen, "%s", codec->name);
    char *n = dst;
    while(*n) {
      *n = toupper((int)*n);
      n++;
    }
  }

  if(profile != NULL)
    off += snprintf(dst + off, dstlen - off,
                    "%s%s", off ? " " : "", profile);

  if(codec->id == AV_CODEC_ID_H264 && par->level != AV_LEVEL_UNKNOWN)
    off += snprintf(dst + off, dstlen - off,
                    " (Level %d.%d)",
                    par->level / 10, par->level % 10);

  if(par->codec_type == AVMEDIA_TYPE_AUDIO) {
    char buf[64];

    av_channel_layout_describe(&par->ch_layout, buf, sizeof(buf));

    off += snprintf(dst + off, dstlen - off, ", %d Hz, %s",
		    par->sample_rate, buf);
  }

  if(par->width)
    off += snprintf(dst + off, dstlen - off,
		    ", %dx%d", par->width, par->height);
}

/**
 *
 */
void
mp_set_mq_meta(media_queue_t *mq, const AVCodec *codec,
	       const AVCodecContext *avctx)
{
  uint64_t channel_layout = avctx->ch_layout.u.mask;
  int channels = avctx->ch_layout.nb_channels;

  if(mq->mq_meta_codec_id       == codec->id &&
     mq->mq_meta_profile        == avctx->profile &&
     mq->mq_meta_channels       == channels &&
     mq->mq_meta_channel_layout == channel_layout &&
     mq->mq_meta_width          == avctx->width &&
     mq->mq_meta_height         == avctx->height)
    return;

  mq->mq_meta_codec_id       = codec->id;
  mq->mq_meta_profile        = avctx->profile;
  mq->mq_meta_channels       = channels;
  mq->mq_meta_channel_layout = channel_layout;
  mq->mq_meta_width          = avctx->width;
  mq->mq_meta_height         = avctx->height;

  char buf[128];
  AVCodecParameters *par = avcodec_parameters_alloc();
  avcodec_parameters_from_context(par, avctx);
  metadata_from_ffmpeg(buf, sizeof(buf), codec, par);
  avcodec_parameters_free(&par);
  prop_set_string(mq->mq_prop_codec, buf);
}


