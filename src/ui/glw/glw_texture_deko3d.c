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
#include <deko3d.h>
#include <switch.h>

#include "glw.h"
#include "glw_deko3d.h"
#include "glw_texture.h"
#include "main.h"

/**
 * Free render resources for a texture
 */
void
glw_tex_backend_free_render_resources(glw_root_t *gr, 
                                      glw_loadable_texture_t *glt)
{
  glw_backend_texture_t *tex = &glt->glt_texture;
  
  if(tex->d3_memblock != NULL) {
    dkMemBlockDestroy(tex->d3_memblock);
    tex->d3_memblock = NULL;
  }
  
  memset(tex, 0, sizeof(glw_backend_texture_t));
}

/**
 * Free loader resources for a texture
 */
void
glw_tex_backend_free_loader_resources(glw_loadable_texture_t *glt)
{
  // No loader resources to free for deko3d
}

/**
 * Layout a texture (set texture parameters)
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
  glw_backend_root_t *be = &gr->gr_be;
  glw_backend_texture_t *tex = &glt->glt_texture;
  
  if(pm == NULL || pm->pm_data == NULL)
    return 0;
  
  // Calculate image layout
  DkImageLayoutMaker imageLayoutMaker;
  dkImageLayoutMakerDefaults(&imageLayoutMaker, be->d3_device);
  imageLayoutMaker.type = DkImageType_2D;
  imageLayoutMaker.format = DkImageFormat_RGBA8_Unorm;
  imageLayoutMaker.dimensions[0] = pm->pm_width;
  imageLayoutMaker.dimensions[1] = pm->pm_height;
  imageLayoutMaker.dimensions[2] = 1;
  imageLayoutMaker.flags = DkImageFlags_BlockLinear;
  
  DkImageLayout imageLayout;
  dkImageLayoutInitialize(&imageLayout, &imageLayoutMaker);
  
  uint32_t imageSize = dkImageLayoutGetSize(&imageLayout);
  uint32_t imageAlign = dkImageLayoutGetAlignment(&imageLayout);
  imageSize = (imageSize + imageAlign - 1) &~ (imageAlign - 1);
  
  // Allocate memory for texture
  DkMemBlockMaker memMaker;
  dkMemBlockMakerDefaults(&memMaker, be->d3_device, imageSize);
  memMaker.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image;
  
  tex->d3_memblock = dkMemBlockCreate(&memMaker);
  if(tex->d3_memblock == NULL) {
    TRACE(TRACE_ERROR, "GLW", "Failed to allocate texture memory");
    return 0;
  }
  
  // Initialize image
  dkImageInitialize(&tex->d3_image, &imageLayout, tex->d3_memblock, 0);
  
  // Initialize image view
  dkImageViewDefaults(&tex->d3_view, &tex->d3_image);
  
  tex->width = pm->pm_width;
  tex->height = pm->pm_height;
  tex->opaque = 1;
  
  // Copy pixel data to texture memory
  uint8_t *cpuAddr = (uint8_t*)dkMemBlockGetCpuAddr(tex->d3_memblock);
  memcpy(cpuAddr, pm->pm_data, pm->pm_width * pm->pm_height * 4);
  
  // Flush CPU cache
  dkMemBlockFlushCpuCache(tex->d3_memblock, 0, imageSize);
  
  return imageSize;
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

  // If texture already exists, update it
  if(tex->d3_memblock != NULL) {
    uint8_t *cpuAddr = (uint8_t*)dkMemBlockGetCpuAddr(tex->d3_memblock);
    uint32_t size = pm->pm_width * pm->pm_height * 4;
    memcpy(cpuAddr, pm->pm_data, size);
    dkMemBlockFlushCpuCache(tex->d3_memblock, 0, size);
  }
}

/**
 * Destroy a texture
 */
void
glw_tex_destroy(glw_root_t *gr, glw_backend_texture_t *tex)
{
  if(tex->d3_memblock != NULL) {
    dkMemBlockDestroy(tex->d3_memblock);
    tex->d3_memblock = NULL;
  }
  
  memset(tex, 0, sizeof(glw_backend_texture_t));
}
