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

struct glw_rgb;
struct glw_rctx;
struct glw_root;
struct glw_backend_root;
struct glw_backend_texture;

/**
 * Software rendering backend for Switch
 */

#define GLW_DRAW_TRIANGLES 0
#define GLW_DRAW_LINE_LOOP 1
#define GLW_DRAW_LINES     2

typedef struct glw_backend_texture {
  uint32_t *pixels;
  uint16_t width;
  uint16_t height;
  uint8_t opaque;
} glw_backend_texture_t;

#define glw_tex_width(gbt) ((gbt)->width)
#define glw_tex_height(gbt) ((gbt)->height)
#define glw_is_tex_inited(n) ((n)->pixels != NULL)

typedef struct glw_backend_root {
  int sw_width;
  int sw_height;
} glw_backend_root_t;
