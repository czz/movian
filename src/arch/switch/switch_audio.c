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
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <switch.h>

#include "audio2/audio.h"
#include "media/media.h"

#ifndef U64_MAX
#define U64_MAX ((uint64_t)-1)
#endif

#define BUFFER_COUNT 4
#define SAMPLE_RATE 48000
#define NUM_CHANNELS 2
#define BUFFER_SIZE (SAMPLE_RATE * NUM_CHANNELS * sizeof(int16_t) / 60) // ~16ms buffer

typedef struct decoder {
  audio_decoder_t ad;
  
  AudioOutBuffer wavebufs[BUFFER_COUNT];
  int current_buffer;
  int buffer_count;
  
  uint8_t *mempool;
  size_t mempool_size;
  
  int initialized;
  int playing;
  int paused;
  
  float volume;
  
} decoder_t;

static float master_volume = 1.0f;
static int master_mute = 0;

/**
 *
 */
static int
switch_audio_init(audio_decoder_t *ad)
{
  decoder_t *d = (decoder_t *)ad;
  
  d->initialized = 0;
  d->playing = 0;
  d->paused = 0;
  d->current_buffer = 0;
  d->buffer_count = 0;
  d->volume = 1.0f;
  
  // Allocate memory pool for audio buffers
  d->mempool_size = BUFFER_SIZE * BUFFER_COUNT;
  if (posix_memalign((void **)&d->mempool, 0x1000, d->mempool_size) != 0) {
    d->mempool = NULL;
    return -1;
  }
  
  memset(d->mempool, 0, d->mempool_size);
  
  // Initialize wavebufs
  for (int i = 0; i < BUFFER_COUNT; i++) {
    d->wavebufs[i].buffer = d->mempool + (i * BUFFER_SIZE);
    d->wavebufs[i].buffer_size = BUFFER_SIZE;
    d->wavebufs[i].next = NULL;
    d->wavebufs[i].data_size = 0;
  }
  
  d->initialized = 1;
  return 0;
}

/**
 *
 */
static void
switch_audio_fini(audio_decoder_t *ad)
{
  decoder_t *d = (decoder_t *)ad;
  
  if (!d->initialized)
    return;
  
  if (d->playing) {
    audoutExit();
    d->playing = 0;
  }
  
  if (d->mempool) {
    free(d->mempool);
    d->mempool = NULL;
  }
  
  d->initialized = 0;
}

/**
 *
 */
static int
switch_audio_reconfig(audio_decoder_t *ad)
{
  decoder_t *d = (decoder_t *)ad;
  
  // Switch audio output is fixed at 48kHz stereo 16-bit
  ad->ad_out_sample_rate = SAMPLE_RATE;
  ad->ad_out_sample_format = AV_SAMPLE_FMT_S16;
  ad->ad_out_channel_layout = AV_CH_LAYOUT_STEREO;
  ad->ad_tile_size = SAMPLE_RATE / 60; // ~800 samples per buffer
  
  // Initialize audout if not already done
  if (!d->playing) {
    Result rc = audoutInitialize();
    if (R_FAILED(rc)) {
      return -1;
    }
    
    d->playing = 1;
  }
  
  return 0;
}

/**
 *
 */
static int
switch_audio_deliver(audio_decoder_t *ad, int samples, int64_t pts, int epoch)
{
  decoder_t *d = (decoder_t *)ad;
  
  if (!d->initialized || !d->playing || d->paused)
    return -1;
  
  // Get next available buffer
  AudioOutBuffer *buf = &d->wavebufs[d->current_buffer];
  
  // Check if buffer is available
  AudioOutBuffer *released;
  u32 released_count;
  Result rc = audoutWaitPlayFinish(&released, &released_count, U64_MAX);
  if (R_FAILED(rc)) {
    return -1;
  }
  
  // Convert samples to 16-bit stereo if needed
  uint8_t *data[8] = {0};
  data[0] = buf->buffer;
  
  int samples_to_copy = samples;
  if (samples_to_copy > (int)(BUFFER_SIZE / (NUM_CHANNELS * sizeof(int16_t)))) {
    samples_to_copy = BUFFER_SIZE / (NUM_CHANNELS * sizeof(int16_t));
  }
  
  int r = ad->ad_avr != NULL ? avresample_read(ad->ad_avr, data, samples_to_copy) : 0;
  
  if (r <= 0) {
    return -1;
  }
  
  // Apply volume
  if (master_mute || d->volume == 0.0f) {
    memset(buf->buffer, 0, r * NUM_CHANNELS * sizeof(int16_t));
  } else {
    float vol = master_volume * d->volume;
    int16_t *samples = (int16_t *)buf->buffer;
    for (int i = 0; i < r * NUM_CHANNELS; i++) {
      samples[i] = (int16_t)(samples[i] * vol);
    }
  }
  
  buf->data_size = r * NUM_CHANNELS * sizeof(int16_t);
  buf->next = NULL;
  
  // Play the buffer
  AudioOutBuffer *released_out;
  rc = audoutPlayBuffer(buf, &released_out);
  if (R_FAILED(rc)) {
    return -1;
  }
  
  // Move to next buffer
  d->current_buffer = (d->current_buffer + 1) % BUFFER_COUNT;
  
  return 0;
}

/**
 *
 */
static void
switch_audio_flush(audio_decoder_t *ad)
{
  decoder_t *d = (decoder_t *)ad;
  
  if (!d->initialized || !d->playing)
    return;
  
  // Wait for all buffers to finish
  AudioOutBuffer *released;
  u32 released_count;
  audoutWaitPlayFinish(&released, &released_count, U64_MAX);
  
  d->current_buffer = 0;
}

/**
 *
 */
static void
switch_audio_pause(audio_decoder_t *ad)
{
  decoder_t *d = (decoder_t *)ad;
  
  if (!d->initialized || !d->playing)
    return;
  
  d->paused = 1;
  audoutExit();
  d->playing = 0;
}

/**
 *
 */
static void
switch_audio_play(audio_decoder_t *ad)
{
  decoder_t *d = (decoder_t *)ad;
  
  if (!d->initialized || d->playing)
    return;
  
  d->paused = 0;
  
  Result rc = audoutInitialize();
  if (R_SUCCEEDED(rc)) {
    d->playing = 1;
  }
}

/**
 *
 */
static void
switch_set_volume(audio_decoder_t *ad, float scale)
{
  decoder_t *d = (decoder_t *)ad;
  d->volume = scale;
}

/**
 *
 */
static int
switch_get_mode(audio_decoder_t *ad, int codec,
		const void *extradata, size_t extradata_size)
{
  // Switch only supports PCM output via audout
  return AUDIO_MODE_PCM;
}

/**
 *
 */
static audio_class_t switch_audio_class = {
  .ac_alloc_size     = sizeof(decoder_t),
  .ac_init           = switch_audio_init,
  .ac_fini           = switch_audio_fini,
  .ac_reconfig       = switch_audio_reconfig,
  .ac_deliver_unlocked = switch_audio_deliver,
  .ac_flush          = switch_audio_flush,
  .ac_pause          = switch_audio_pause,
  .ac_play           = switch_audio_play,
  .ac_set_volume     = switch_set_volume,
  .ac_get_mode       = switch_get_mode,
};

/**
 *
 */
static void
set_mastervol(void *opaque, float value)
{
  master_volume = pow(10, (value / 20));
}

/**
 *
 */
static void
set_mastermute(void *opaque, int value)
{
  master_mute = value;
}

/**
 *
 */
audio_class_t *
audio_driver_init(struct prop *asettings)
{
  // Subscribe to master volume changes
  prop_subscribe(0,
		 PROP_TAG_CALLBACK_FLOAT, set_mastervol, NULL,
		 PROP_TAG_NAME("global", "audio", "mastervolume"),
		 NULL);

  prop_subscribe(0,
		 PROP_TAG_CALLBACK_INT, set_mastermute, NULL,
		 PROP_TAG_NAME("global", "audio", "mastermute"),
		 NULL);

  return &switch_audio_class;
}
