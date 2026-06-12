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
#include <deko3d.h>

struct glw_rgb;
struct glw_rctx;
struct glw_root;
struct glw_backend_root;
struct glw_renderer;
struct glw_backend_texture;


#define GLW_DRAW_TRIANGLES DkPrimitive_Triangles
#define GLW_DRAW_LINE_LOOP DkPrimitive_LineLoop
#define GLW_DRAW_LINES     DkPrimitive_Lines


/**
 *
 */
typedef struct deko3d_shader {
  DkShader *d3_shader;
  uint32_t d3_code_size;
  uint32_t d3_binding_base;

} deko3d_shader_t;


/**
 *
 */
struct glw_program {

  deko3d_shader_t *gp_vertex_shader;
  deko3d_shader_t *gp_fragment_shader;

  int gp_u_modelview;
  int gp_u_color;
  int gp_u_color_offset;
  int gp_u_blur;
  int gp_u_blend;
  int gp_u_color_matrix;

  int gp_a_position;
  int gp_a_color;
  int gp_a_texcoord;

  int gp_texunit[6];

};

/**
 *
 */
typedef struct glw_backend_root {
  DkDevice *d3_device;
  DkQueue *d3_queue;
  DkSwapchain *d3_swapchain;
  DkCmdBuf *d3_cmdbuf;

  int d3_width;
  int d3_height;

  struct glw_program *d3_current_program;

  struct glw_program d3_yuv2rgb_1f;
  struct glw_program d3_yuv2rgb_2f;
  struct glw_program d3_rgb2rgb_1f;
  struct glw_program d3_rgb2rgb_2f;
  struct glw_program d3_yc2rgb_1f;
  struct glw_program d3_yc2rgb_2f;

  struct glw_program d3_renderer_tex;
  struct glw_program d3_renderer_tex_stencil;
  struct glw_program d3_renderer_tex_blur;
  struct glw_program d3_renderer_tex_stencil_blur;
  struct glw_program d3_renderer_flat;
  struct glw_program d3_renderer_flat_stencil;

  DkMemBlock *d3_image_mem;
  DkMemBlock *d3_code_mem;
  DkMemBlock *d3_vertex_mem;
  DkMemBlock *d3_index_mem;
  DkMemBlock *d3_uniform_mem;

  DkShader d3_vertex_shader;
  DkShader d3_fragment_shader;
  DkImage d3_framebuffers[2];

} glw_backend_root_t;


/**
 *
 */
typedef struct glw_backend_texture {
  DkImage d3_image;
  DkMemBlock *d3_memblock;
  DkImageView d3_view;
  uint16_t width;
  uint16_t height;
  uint8_t opaque;
} glw_backend_texture_t;

#define glw_tex_width(gbt) ((gbt)->width)
#define glw_tex_height(gbt) ((gbt)->height)
#define glw_is_tex_inited(n) ((n)->d3_memblock != NULL)


#define glw_can_tnpo2(gr) 1

int glw_deko3d_init_context(struct glw_root *gr);
void glw_deko3d_fini(struct glw_root *gr);
