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
#include <math.h>

#include <deko3d.h>
#include <switch.h>
#include <switch/nvidia/map.h>
#include <switch/nvidia/address_space.h>
#include <switch/applets/error.h>

#include "glw.h"
#include "glw_deko3d.h"
#include "glw_video_common.h"
#include "glw_texture.h"
#include "main.h"

// Forward declaration
static void glw_deko3d_render_unlocked(glw_root_t *gr);

// Function to load shader from file
static void loadShaderFromFile(DkShader* pShader, const char* path,
                               DkMemBlock codeMemBlock, uint32_t codeOffset)
{
  TRACE(TRACE_INFO, "GLW", "Loading shader from: %s", path);
  FILE* f = fopen(path, "rb");
  if(f == NULL) {
    TRACE(TRACE_ERROR, "GLW", "Failed to open shader file: %s", path);
    return;
  }

  fseek(f, 0, SEEK_END);
  uint32_t size = ftell(f);
  rewind(f);
  TRACE(TRACE_INFO, "GLW", "Shader file size: %u bytes", size);

  // Copy shader data to code memory
  uint8_t* codeAddr = (uint8_t*)dkMemBlockGetCpuAddr(codeMemBlock) + codeOffset;
  fread(codeAddr, 1, size, f);
  fclose(f);

  // Initialize shader
  DkShaderMaker shaderMaker;
  dkShaderMakerDefaults(&shaderMaker, codeMemBlock, codeOffset);
  dkShaderInitialize(pShader, &shaderMaker);
  TRACE(TRACE_INFO, "GLW", "Shader initialized: %s", path);
}

/**
 *
 */
int
glw_deko3d_init_context(glw_root_t *gr)
{
  glw_backend_root_t *be = &gr->gr_be;

  TRACE(TRACE_INFO, "GLW", "Initializing Deko3d context");

  // Set render unlocked function
  gr->gr_be_render_unlocked = glw_deko3d_render_unlocked;

  // Initialize NVN first
  TRACE(TRACE_INFO, "GLW", "Initializing NVN map");
  Result rc = nvMapInit();
  if(R_FAILED(rc)) {
    TRACE(TRACE_ERROR, "GLW", "Failed to initialize NVN map: %x", rc);
    return -1;
  }
  TRACE(TRACE_INFO, "GLW", "NVN map initialized");

  // Initialize deko3d device
  TRACE(TRACE_INFO, "GLW", "Creating deko3d device");
  DkDeviceMaker deviceMaker;
  dkDeviceMakerDefaults(&deviceMaker);
  DkDevice device = dkDeviceCreate(&deviceMaker);
  if(device == NULL) {
    TRACE(TRACE_ERROR, "GLW", "Failed to create deko3d device");
    nvMapExit();
    return -1;
  }
  TRACE(TRACE_INFO, "GLW", "Deko3d device created");

  // Initialize backend structure
  memset(be, 0, sizeof(glw_backend_root_t));
  be->d3_device = device;

  // Create command buffer
  TRACE(TRACE_INFO, "GLW", "Creating command buffer");
  DkCmdBufMaker cmdBufMaker;
  dkCmdBufMakerDefaults(&cmdBufMaker, device);
  be->d3_cmdbuf = dkCmdBufCreate(&cmdBufMaker);
  if(be->d3_cmdbuf == NULL) {
    TRACE(TRACE_ERROR, "GLW", "Failed to create deko3d command buffer");
    dkDeviceDestroy(device);
    return -1;
  }
  TRACE(TRACE_INFO, "GLW", "Command buffer created");

  // Create queue
  TRACE(TRACE_INFO, "GLW", "Creating queue");
  DkQueueMaker queueMaker;
  dkQueueMakerDefaults(&queueMaker, device);
  be->d3_queue = dkQueueCreate(&queueMaker);
  if(be->d3_queue == NULL) {
    TRACE(TRACE_ERROR, "GLW", "Failed to create deko3d queue");
    dkCmdBufDestroy(be->d3_cmdbuf);
    dkDeviceDestroy(device);
    return -1;
  }
  TRACE(TRACE_INFO, "GLW", "Queue created");

  // Get window dimensions
  be->d3_width = 1280;
  be->d3_height = 720;

  // Calculate framebuffer layout
  TRACE(TRACE_INFO, "GLW", "Creating framebuffer layout");
  DkImageLayoutMaker imageLayoutMaker;
  dkImageLayoutMakerDefaults(&imageLayoutMaker, device);
  imageLayoutMaker.flags = DkImageFlags_UsageRender | DkImageFlags_UsagePresent | DkImageFlags_HwCompression;
  imageLayoutMaker.format = DkImageFormat_RGBA8_Unorm;
  imageLayoutMaker.dimensions[0] = be->d3_width;
  imageLayoutMaker.dimensions[1] = be->d3_height;
  imageLayoutMaker.dimensions[2] = 1;

  DkImageLayout framebufferLayout;
  dkImageLayoutInitialize(&framebufferLayout, &imageLayoutMaker);

  uint32_t framebufferSize = dkImageLayoutGetSize(&framebufferLayout);
  uint32_t framebufferAlign = dkImageLayoutGetAlignment(&framebufferLayout);
  framebufferSize = (framebufferSize + framebufferAlign - 1) &~ (framebufferAlign - 1);

  // Create framebuffer memory
  TRACE(TRACE_INFO, "GLW", "Allocating framebuffer memory (%u bytes)", framebufferSize * 2);
  DkMemBlockMaker memMaker;
  dkMemBlockMakerDefaults(&memMaker, device, 2 * framebufferSize);
  memMaker.flags = DkMemBlockFlags_GpuCached | DkMemBlockFlags_Image;
  be->d3_image_mem = dkMemBlockCreate(&memMaker);
  if(be->d3_image_mem == NULL) {
    TRACE(TRACE_ERROR, "GLW", "Failed to allocate deko3d image memory");
    dkQueueDestroy(be->d3_queue);
    dkCmdBufDestroy(be->d3_cmdbuf);
    dkDeviceDestroy(device);
    return -1;
  }
  TRACE(TRACE_INFO, "GLW", "Framebuffer memory allocated");

  // Initialize framebuffers
  DkImage const* swapchainImages[2];
  for(unsigned i = 0; i < 2; i++) {
    swapchainImages[i] = &be->d3_framebuffers[i];
    dkImageInitialize(&be->d3_framebuffers[i], &framebufferLayout, be->d3_image_mem, i * framebufferSize);
  }

  // Create swapchain
  TRACE(TRACE_INFO, "GLW", "Creating swapchain");
  DkSwapchainMaker scMaker;
  dkSwapchainMakerDefaults(&scMaker, device, nwindowGetDefault(), swapchainImages, 2);
  be->d3_swapchain = dkSwapchainCreate(&scMaker);
  if(be->d3_swapchain == NULL) {
    TRACE(TRACE_ERROR, "GLW", "Failed to create deko3d swapchain");
    dkMemBlockDestroy(be->d3_image_mem);
    dkQueueDestroy(be->d3_queue);
    dkCmdBufDestroy(be->d3_cmdbuf);
    dkDeviceDestroy(device);
    return -1;
  }
  TRACE(TRACE_INFO, "GLW", "Swapchain created");

  // Allocate memory for shader code
  TRACE(TRACE_INFO, "GLW", "Allocating shader code memory");
  dkMemBlockMakerDefaults(&memMaker, device, 0x100000);
  memMaker.flags = DkMemBlockFlags_Code;
  be->d3_code_mem = dkMemBlockCreate(&memMaker);
  if(be->d3_code_mem == NULL) {
    TRACE(TRACE_ERROR, "GLW", "Failed to allocate deko3d code memory");
    dkMemBlockDestroy(be->d3_image_mem);
    dkSwapchainDestroy(be->d3_swapchain);
    dkQueueDestroy(be->d3_queue);
    dkCmdBufDestroy(be->d3_cmdbuf);
    dkDeviceDestroy(device);
    return -1;
  }
  TRACE(TRACE_INFO, "GLW", "Shader code memory allocated");

  // Load shaders from files
  TRACE(TRACE_INFO, "GLW", "Loading shaders");
  uint32_t codeOffset = 0;
  loadShaderFromFile(&be->d3_vertex_shader, "bundles/shaders/deko3d_vertex.dksh",
                     be->d3_code_mem, codeOffset);
  codeOffset += 0x1000; // Align to 4KB
  loadShaderFromFile(&be->d3_fragment_shader, "bundles/shaders/deko3d_fragment.dksh",
                     be->d3_code_mem, codeOffset);
  TRACE(TRACE_INFO, "GLW", "Shaders loaded");

  // Allocate memory for vertex buffers
  TRACE(TRACE_INFO, "GLW", "Allocating vertex buffer memory");
  dkMemBlockMakerDefaults(&memMaker, device, 0x100000);
  memMaker.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
  be->d3_vertex_mem = dkMemBlockCreate(&memMaker);
  if(be->d3_vertex_mem == NULL) {
    TRACE(TRACE_ERROR, "GLW", "Failed to allocate deko3d vertex memory");
    dkMemBlockDestroy(be->d3_code_mem);
    dkMemBlockDestroy(be->d3_image_mem);
    dkSwapchainDestroy(be->d3_swapchain);
    dkQueueDestroy(be->d3_queue);
    dkCmdBufDestroy(be->d3_cmdbuf);
    dkDeviceDestroy(device);
    return -1;
  }
  TRACE(TRACE_INFO, "GLW", "Vertex buffer memory allocated");

  // Allocate memory for index buffers
  TRACE(TRACE_INFO, "GLW", "Allocating index buffer memory");
  dkMemBlockMakerDefaults(&memMaker, device, 0x100000);
  memMaker.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
  be->d3_index_mem = dkMemBlockCreate(&memMaker);
  if(be->d3_index_mem == NULL) {
    TRACE(TRACE_ERROR, "GLW", "Failed to allocate deko3d index memory");
    dkMemBlockDestroy(be->d3_vertex_mem);
    dkMemBlockDestroy(be->d3_code_mem);
    dkMemBlockDestroy(be->d3_image_mem);
    dkSwapchainDestroy(be->d3_swapchain);
    dkQueueDestroy(be->d3_queue);
    dkCmdBufDestroy(be->d3_cmdbuf);
    dkDeviceDestroy(device);
    return -1;
  }
  TRACE(TRACE_INFO, "GLW", "Index buffer memory allocated");

  // Allocate memory for uniform buffers
  TRACE(TRACE_INFO, "GLW", "Allocating uniform buffer memory");
  dkMemBlockMakerDefaults(&memMaker, device, 0x10000);
  memMaker.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached;
  be->d3_uniform_mem = dkMemBlockCreate(&memMaker);
  if(be->d3_uniform_mem == NULL) {
    TRACE(TRACE_ERROR, "GLW", "Failed to allocate deko3d uniform memory");
    dkMemBlockDestroy(be->d3_index_mem);
    dkMemBlockDestroy(be->d3_vertex_mem);
    dkMemBlockDestroy(be->d3_code_mem);
    dkMemBlockDestroy(be->d3_image_mem);
    dkSwapchainDestroy(be->d3_swapchain);
    dkQueueDestroy(be->d3_queue);
    dkCmdBufDestroy(be->d3_cmdbuf);
    dkDeviceDestroy(device);
    return -1;
  }
  TRACE(TRACE_INFO, "GLW", "Uniform buffer memory allocated");

  TRACE(TRACE_INFO, "GLW", "deko3d backend initialized successfully");
  return 0;
}


/**
 *
 */
static void
glw_deko3d_render_unlocked(glw_root_t *gr)
{
  glw_backend_root_t *be = &gr->gr_be;
  DkCmdBuf *cmdbuf = be->d3_cmdbuf;

  // Set viewport
  DkViewport viewport = {
    .x = 0.0f,
    .y = 0.0f,
    .width = (float)be->d3_width,
    .height = (float)be->d3_height,
    .near = 0.0f,
    .far = 1.0f,
  };
  dkCmdBufSetViewports(cmdbuf, 0, &viewport, 1);

  // Set scissor
  DkScissor scissor = {
    .x = 0,
    .y = 0,
    .width = be->d3_width,
    .height = be->d3_height,
  };
  dkCmdBufSetScissors(cmdbuf, 0, &scissor, 1);

  // Track current blend mode
  int current_blendmode = -1;

  // Upload vertex buffer data to GPU
  if(gr->gr_vertex_buffer != NULL) {
    void *vtx_cpu_addr = dkMemBlockGetCpuAddr(be->d3_vertex_mem);
    if(vtx_cpu_addr != NULL) {
      // TODO: Need to know buffer size - use a reasonable default for now
      memcpy(vtx_cpu_addr, gr->gr_vertex_buffer, 0x100000);
      armDCacheFlush(vtx_cpu_addr, 0x100000);
    }
  }

  // Upload index buffer data to GPU
  if(gr->gr_index_buffer != NULL) {
    void *idx_cpu_addr = dkMemBlockGetCpuAddr(be->d3_index_mem);
    if(idx_cpu_addr != NULL) {
      // TODO: Need to know buffer size - use a reasonable default for now
      memcpy(idx_cpu_addr, gr->gr_index_buffer, 0x100000);
      armDCacheFlush(idx_cpu_addr, 0x100000);
    }
  }

  // Process render jobs
  for(int j = 0; j < gr->gr_num_render_jobs; j++) {
    const glw_render_order_t *ro = gr->gr_render_order + j;
    const glw_render_job_t *rj = ro->job;
    const struct glw_backend_texture *t0 = rj->t0;
    const struct glw_backend_texture *t1 = rj->t1;

    // Bind shaders
    dkCmdBufBindShaders(cmdbuf, DkStageFlag_GraphicsMask, &be->d3_vertex_shader, &be->d3_fragment_shader);

    // Setup vertex attributes manually (position, color, texcoord)
    DkVtxAttribState attribState[3] = {0};
    attribState[0].bufferId = 0;
    attribState[0].offset = 0;
    attribState[0].type = DkVtxAttribType_Float;
    attribState[0].size = 3;
    attribState[1].bufferId = 0;
    attribState[1].offset = sizeof(float) * 3;
    attribState[1].type = DkVtxAttribType_Float;
    attribState[1].size = 4;
    attribState[2].bufferId = 0;
    attribState[2].offset = sizeof(float) * 7;
    attribState[2].type = DkVtxAttribType_Float;
    attribState[2].size = 2;
    dkCmdBufBindVtxAttribState(cmdbuf, attribState, 3);

    // Setup vertex buffer state manually
    DkVtxBufferState vtxBufferState = {0};
    vtxBufferState.stride = sizeof(float) * 8;
    dkCmdBufBindVtxBufferState(cmdbuf, &vtxBufferState, 1);

    // Bind texture 0 if present
    if(t0 != NULL && glw_is_tex_inited(t0)) {
      dkCmdBufBindImage(cmdbuf, 0, &t0->d3_view, 0);
    }

    // Bind texture 1 if present
    if(t1 != NULL && glw_is_tex_inited(t1)) {
      dkCmdBufBindImage(cmdbuf, 1, &t1->d3_view, 0);
    }

    // Set uniforms (matrix, color, alpha)
    float rgba[4];
    const float alpha = rj->alpha;

    if(rj->blendmode == GLW_BLEND_NORMAL) {
      rgba[0] = rj->rgb_mul.r;
      rgba[1] = rj->rgb_mul.g;
      rgba[2] = rj->rgb_mul.b;
      rgba[3] = alpha;
    } else {
      rgba[0] = rj->rgb_mul.r * alpha;
      rgba[1] = rj->rgb_mul.g * alpha;
      rgba[2] = rj->rgb_mul.b * alpha;
      rgba[3] = 1.0f;
    }

    // Upload uniforms to GPU memory
    void *uniform_cpu_addr = dkMemBlockGetCpuAddr(be->d3_uniform_mem);
    DkGpuAddr uniform_gpu_addr = dkMemBlockGetGpuAddr(be->d3_uniform_mem);
    if(uniform_cpu_addr != NULL) {
      // Copy color uniform (offset 0)
      memcpy((char*)uniform_cpu_addr + 0, rgba, sizeof(rgba));
      // Copy modelview matrix uniform (offset 16 bytes)
      memcpy((char*)uniform_cpu_addr + 16, (const float *)&rj->m, sizeof(Mtx));
      armDCacheFlush(uniform_cpu_addr, 16 + sizeof(Mtx));

      // Push color uniform to fragment shader
      dkCmdBufPushConstants(cmdbuf, uniform_gpu_addr, 0x10000, 0, sizeof(rgba), rgba);
      // Push modelview matrix uniform to vertex shader
      dkCmdBufPushConstants(cmdbuf, uniform_gpu_addr, 0x10000, 16, sizeof(Mtx), (const float *)&rj->m);
    }

    // Set blend mode
    if(current_blendmode != rj->blendmode) {
      current_blendmode = rj->blendmode;
      DkBlendState blendState;
      dkBlendStateDefaults(&blendState);

      switch(rj->blendmode) {
        case GLW_BLEND_ADDITIVE:
          blendState.srcColorBlendFactor = DkBlendFactor_SrcColor;
          blendState.dstColorBlendFactor = DkBlendFactor_One;
          blendState.srcAlphaBlendFactor = DkBlendFactor_SrcAlpha;
          blendState.dstAlphaBlendFactor = DkBlendFactor_One;
          break;
        case GLW_BLEND_NORMAL:
        default:
          blendState.srcColorBlendFactor = DkBlendFactor_SrcAlpha;
          blendState.dstColorBlendFactor = DkBlendFactor_InvSrcAlpha;
          blendState.srcAlphaBlendFactor = DkBlendFactor_SrcAlpha;
          blendState.dstAlphaBlendFactor = DkBlendFactor_Zero;
          break;
      }

      dkCmdBufBindBlendState(cmdbuf, 0, &blendState);
    }

    // Set vertex buffers
    if(gr->gr_vertex_buffer != NULL && rj->num_vertices > 0) {
      // Bind vertex buffer
      dkCmdBufBindVtxBuffer(cmdbuf, be->d3_vertex_mem, rj->vertex_offset * sizeof(float), sizeof(float) * 8);
    }

    // Bind index buffer if needed
    if(rj->num_indices > 0 && gr->gr_index_buffer != NULL) {
      DkGpuAddr idx_gpu_addr = dkMemBlockGetGpuAddr(be->d3_index_mem);
      dkCmdBufBindIdxBuffer(cmdbuf, DkIdxFormat_Uint16, idx_gpu_addr);
    }

    // Draw primitives based on rj->primitive_type
    DkPrimitive primitive;
    switch(rj->primitive_type) {
      case GLW_DRAW_TRIANGLES:
        primitive = DkPrimitive_Triangles;
        break;
      case GLW_DRAW_LINE_LOOP:
        primitive = DkPrimitive_LineLoop;
        break;
      case GLW_DRAW_LINES:
        primitive = DkPrimitive_Lines;
        break;
      default:
        primitive = DkPrimitive_Triangles;
        break;
    }

    if(rj->num_indices > 0 && gr->gr_index_buffer != NULL) {
      // Indexed drawing
      dkCmdBufDrawIndexed(cmdbuf, primitive, rj->num_indices, 1, rj->index_offset, 0, 0);
    } else if(rj->num_vertices > 0) {
      // Direct drawing
      dkCmdBufDraw(cmdbuf, primitive, rj->num_vertices, 1, rj->vertex_offset, 0);
    }
  }
}

/**
 *
 */
void
glw_deko3d_fini(glw_root_t *gr)
{
  glw_backend_root_t *be = &gr->gr_be;
  if(be->d3_device == NULL)
    return;

  if(be->d3_uniform_mem)
    dkMemBlockDestroy(be->d3_uniform_mem);
  if(be->d3_index_mem)
    dkMemBlockDestroy(be->d3_index_mem);
  if(be->d3_vertex_mem)
    dkMemBlockDestroy(be->d3_vertex_mem);
  if(be->d3_code_mem)
    dkMemBlockDestroy(be->d3_code_mem);
  if(be->d3_image_mem)
    dkMemBlockDestroy(be->d3_image_mem);
  if(be->d3_swapchain)
    dkSwapchainDestroy(be->d3_swapchain);
  if(be->d3_queue)
    dkQueueDestroy(be->d3_queue);
  if(be->d3_cmdbuf)
    dkCmdBufDestroy(be->d3_cmdbuf);
  if(be->d3_device)
    dkDeviceDestroy(be->d3_device);

  memset(be, 0, sizeof(glw_backend_root_t));

  TRACE(TRACE_INFO, "GLW", "deko3d backend destroyed");
}


/**
 *
 */
static void
glw_deko3d_render(glw_root_t *gr)
{
  glw_backend_root_t *be = &gr->gr_be;
  if(be->d3_device == NULL)
    return;

  // Acquire framebuffer from swapchain
  int slot = dkQueueAcquireImage(be->d3_queue, be->d3_swapchain);
  if(slot < 0)
    return;

  // Bind framebuffer as render target
  DkImageView imageView;
  dkImageViewDefaults(&imageView, &be->d3_framebuffers[slot]);
  dkCmdBufBindRenderTargets(be->d3_cmdbuf, &imageView, 1, NULL);

  // Set viewport and scissor
  DkViewport viewport = { 0.0f, 0.0f, (float)be->d3_width, (float)be->d3_height, 0.0f, 1.0f };
  DkScissor scissor = { 0, 0, be->d3_width, be->d3_height };
  dkCmdBufSetViewports(be->d3_cmdbuf, 0, &viewport, 1);
  dkCmdBufSetScissors(be->d3_cmdbuf, 0, &scissor, 1);

  // Clear screen
  float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
  dkCmdBufClearColor(be->d3_cmdbuf, 0, DkColorMask_RGBA, clearColor);

  // Bind shaders
  DkShader const* shaders[] = { &be->d3_vertex_shader, &be->d3_fragment_shader };
  dkCmdBufBindShaders(be->d3_cmdbuf, DkStageFlag_GraphicsMask, shaders, 2);

  // Set default rasterizer and color state
  DkRasterizerState rasterizerState;
  DkColorState colorState;
  DkColorWriteState colorWriteState;
  dkRasterizerStateDefaults(&rasterizerState);
  dkColorStateDefaults(&colorState);
  dkColorWriteStateDefaults(&colorWriteState);
  dkCmdBufBindRasterizerState(be->d3_cmdbuf, &rasterizerState);
  dkCmdBufBindColorState(be->d3_cmdbuf, &colorState);
  dkCmdBufBindColorWriteState(be->d3_cmdbuf, &colorWriteState);

  // Submit commands and present
  dkQueueSubmitCommands(be->d3_queue, dkCmdBufFinishList(be->d3_cmdbuf));
  dkQueuePresentImage(be->d3_queue, be->d3_swapchain, slot);
}


/**
 *
 */
static void
glw_deko3d_update_size(glw_root_t *gr)
{
  glw_backend_root_t *be = &gr->gr_be;
  if(be->d3_device == NULL)
    return;

  // Update dimensions from window
  // For now, keep 1280x720
  be->d3_width = 1280;
  be->d3_height = 720;
}

/**
 * Create a shader program for Deko3d
 */
struct glw_program *
glw_make_program(struct glw_root *gr, const char *title, const char *fs)
{
  // For Deko3d, we need to load shaders from files
  // This is a simplified implementation
  glw_program_t *gp = calloc(1, sizeof(glw_program_t));
  if(gp == NULL)
    return NULL;

  return gp;
}

/**
 * Destroy a shader program
 */
void
glw_destroy_program(struct glw_root *gr, struct glw_program *gp)
{
  if(gp == NULL)
    return;

  free(gp);
}

/**
 * Video overlay functions for Deko3d
 */
void
glw_video_overlay_set_pts(glw_video_t *gv, int64_t pts)
{
  // Stub for Deko3d
}

void
glw_video_overlay_render(glw_video_t *gv, const glw_rctx_t *rc)
{
  // Stub for Deko3d
}

void
glw_video_overlay_layout(glw_video_t *gv)
{
  // Stub for Deko3d
}

void
glw_video_overlay_deinit(glw_video_t *gv)
{
  // Stub for Deko3d
}

void
glw_video_overlay_pointer_event(glw_video_t *gv, event_t *e)
{
  // Stub for Deko3d
}
