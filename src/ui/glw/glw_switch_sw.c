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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "glw_switch_sw.h"
#include "glw.h"
#include "glw_texture.h"
#include "main.h"

/**
 * Software rendering backend for Switch
 */

static uint32_t *framebuffer = NULL;
static int fb_width = 1280;
static int fb_height = 720;

/**
 * Free render resources for a texture
 */
void
glw_tex_backend_free_render_resources(glw_root_t *gr,
                                      glw_loadable_texture_t *glt)
{
  glw_backend_texture_t *tex = &glt->glt_texture;

  if(tex->pixels != NULL) {
    free(tex->pixels);
    tex->pixels = NULL;
  }

  memset(tex, 0, sizeof(glw_backend_texture_t));
}

/**
 * Free loader resources for a texture
 */
void
glw_tex_backend_free_loader_resources(glw_loadable_texture_t *glt)
{
  // No loader resources to free
}

/**
 * Layout a texture
 */
void
glw_tex_backend_layout(glw_root_t *gr, glw_loadable_texture_t *glt)
{
  // Texture layout is handled during upload
}

/**
 * Load a texture from a pixmap
 */
int
glw_tex_backend_load(glw_root_t *gr, glw_loadable_texture_t *glt, pixmap_t *pm)
{
  glw_backend_texture_t *tex = &glt->glt_texture;

  if(pm == NULL || pm->pm_data == NULL)
    return 0;

  tex->width = pm->pm_width;
  tex->height = pm->pm_height;
  tex->opaque = 1;

  tex->pixels = malloc(pm->pm_width * pm->pm_height * 4);
  if(tex->pixels == NULL) {
    TRACE(TRACE_ERROR, "GLW", "Failed to allocate texture memory");
    return 0;
  }

  memcpy(tex->pixels, pm->pm_data, pm->pm_width * pm->pm_height * 4);

  return pm->pm_width * pm->pm_height * 4;
}

/**
 * Upload texture data
 */
void
glw_tex_upload(glw_root_t *gr, glw_backend_texture_t *tex,
               const pixmap_t *pm, int flags)
{
  if(pm == NULL || pm->pm_data == NULL)
    return;

  if(tex->pixels != NULL) {
    free(tex->pixels);
  }

  tex->width = pm->pm_width;
  tex->height = pm->pm_height;
  tex->pixels = malloc(pm->pm_width * pm->pm_height * 4);

  if(tex->pixels != NULL) {
    memcpy(tex->pixels, pm->pm_data, pm->pm_width * pm->pm_height * 4);
  }
}

/**
 * Destroy a texture
 */
void
glw_tex_destroy(glw_root_t *gr, glw_backend_texture_t *tex)
{
  if(tex->pixels != NULL) {
    free(tex->pixels);
    tex->pixels = NULL;
  }

  memset(tex, 0, sizeof(glw_backend_texture_t));
}

/**
 * Initialize software backend
 */
static int
glw_sw_init(glw_root_t *gr)
{
  framebuffer = malloc(fb_width * fb_height * 4);
  if(framebuffer == NULL) {
    TRACE(TRACE_ERROR, "GLW", "Failed to allocate framebuffer");
    return -1;
  }
  
  memset(framebuffer, 0, fb_width * fb_height * 4);
  
  TRACE(TRACE_INFO, "GLW", "Software backend initialized");
  return 0;
}

/**
 * Cleanup software backend
 */
static void
glw_sw_fini(glw_root_t *gr)
{
  if(framebuffer != NULL) {
    free(framebuffer);
    framebuffer = NULL;
  }
  
  TRACE(TRACE_INFO, "GLW", "Software backend destroyed");
}

/**
 * Render frame
 */
static void
glw_sw_render(glw_root_t *gr)
{
  // Clear framebuffer to black
  if(framebuffer != NULL) {
    memset(framebuffer, 0, fb_width * fb_height * 4);
  }
  
  // TODO: Implement actual rendering
}

/**
 * Update size
 */
static void
glw_sw_update_size(glw_root_t *gr)
{
  // Keep 1280x720
}

/**
 * Make a shader program (stub for software rendering)
 */
struct glw_program *
glw_make_program(struct glw_root *gr, const char *title, const char *fs)
{
  // Return NULL for software rendering - no GPU shaders
  return NULL;
}

/**
 * Destroy a shader program
 */
void
glw_destroy_program(struct glw_root *gr, struct glw_program *gp)
{
  // No-op for software rendering
}
