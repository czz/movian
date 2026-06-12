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

#include <assert.h>
#include <sys/time.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <malloc.h>

#include "settings.h"

#include "glw.h"
#include "glw_settings.h"
#include "glw_video_common.h"

#if CONFIG_GLW_BACKEND_DEKO3D
#include "glw_deko3d.h"
#endif

#include "main.h"
#include "settings.h"
#include "misc/extents.h"
#include "navigator.h"
#include "arch/arch.h"
#include "event.h"

#include <switch.h>

#if !CONFIG_GLW_BACKEND_DEKO3D
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#endif

// Avoid conflict with libnx's utf16_to_utf8
#define utf16_to_utf8 movian_utf16_to_utf8
#include "misc/str.h"
#undef utf16_to_utf8

// Switch controller button mapping - using libnx HidNpadButton constants
#define AVEC(x...) (const action_type_t []){x, ACTION_NONE}

static void
send_action_from_button(u64 button_mask, u64 kDown, u64 kHeld)
{
  const action_type_t *actions = NULL;

  switch(button_mask) {
    case HidNpadButton_A:
      actions = AVEC(ACTION_ACTIVATE);
      break;
    case HidNpadButton_B:
      actions = AVEC(ACTION_NAV_BACK);
      break;
    case HidNpadButton_X:
      actions = AVEC(ACTION_PLAY);
      break;
    case HidNpadButton_Y:
      actions = AVEC(ACTION_MENU);
      break;
    case HidNpadButton_Up:
      actions = AVEC(ACTION_UP);
      break;
    case HidNpadButton_Down:
      actions = AVEC(ACTION_DOWN);
      break;
    case HidNpadButton_Left:
      actions = AVEC(ACTION_LEFT);
      break;
    case HidNpadButton_Right:
      actions = AVEC(ACTION_RIGHT);
      break;
    case HidNpadButton_Plus:
      actions = AVEC(ACTION_ENTER);
      break;
    case HidNpadButton_Minus:
      actions = AVEC(ACTION_CANCEL);
      break;
    case HidNpadButton_L:
      actions = AVEC(ACTION_VOLUME_DOWN);
      break;
    case HidNpadButton_R:
      actions = AVEC(ACTION_VOLUME_UP);
      break;
    case HidNpadButton_ZL:
      actions = AVEC(ACTION_SKIP_BACKWARD);
      break;
    case HidNpadButton_ZR:
      actions = AVEC(ACTION_SKIP_FORWARD);
      break;
    default:
      return;
  }

  if (actions) {
    int i = 0;
    while (actions[i] != ACTION_NONE) {
      event_t *e = event_create_action(actions[i]);
      if (e) {
        e->e_flags |= EVENT_KEYPRESS;
        event_to_ui(e);
      }
      i++;
    }
  }
}

typedef struct glw_switch {
  glw_root_t gr;

  float gp_browser_alpha;
  int gp_stop;
  int gp_seekmode;

#if !CONFIG_GLW_BACKEND_DEKO3D
  EGLDisplay egl_display;
  EGLSurface egl_surface;
  EGLContext egl_context;
  EGLConfig egl_config;
#endif

  int screen_width;
  int screen_height;

  // Controller input using libnx pad API
  PadState pad;
  u64 kDownOld;

  float scale;

} glw_switch_t;

static glw_switch_t *glwswitch;

#if !CONFIG_GLW_BACKEND_DEKO3D
/**
 * Initialize OpenGL ES via EGL
 */
static int
glw_switch_init_egl(glw_switch_t *gp)
{
  EGLint num_configs;
  EGLint attrib_list[] = {
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 8,
    EGL_DEPTH_SIZE, 24,
    EGL_STENCIL_SIZE, 8,
    EGL_NONE
  };

  EGLint context_attribs[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
  };

  // Get display
  gp->egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (gp->egl_display == EGL_NO_DISPLAY) {
    TRACE(TRACE_ERROR, "GLW", "Failed to get EGL display");
    return -1;
  }

  // Initialize EGL
  if (eglInitialize(gp->egl_display, NULL, NULL) != EGL_TRUE) {
    TRACE(TRACE_ERROR, "GLW", "Failed to initialize EGL");
    return -1;
  }

  // Choose config
  if (eglChooseConfig(gp->egl_display, attrib_list, &gp->egl_config, 1, &num_configs) != EGL_TRUE ||
      num_configs == 0) {
    TRACE(TRACE_ERROR, "GLW", "Failed to choose EGL config");
    return -1;
  }

  // Create window surface
  gp->egl_surface = eglCreateWindowSurface(gp->egl_display, gp->egl_config, nwindowGetDefault(), NULL);
  if (gp->egl_surface == EGL_NO_SURFACE) {
    TRACE(TRACE_ERROR, "GLW", "Failed to create EGL surface");
    return -1;
  }

  // Create context
  gp->egl_context = eglCreateContext(gp->egl_display, gp->egl_config, EGL_NO_CONTEXT, context_attribs);
  if (gp->egl_context == EGL_NO_CONTEXT) {
    TRACE(TRACE_ERROR, "GLW", "Failed to create EGL context");
    return -1;
  }

  // Make context current
  if (eglMakeCurrent(gp->egl_display, gp->egl_surface, gp->egl_surface, gp->egl_context) != EGL_TRUE) {
    TRACE(TRACE_ERROR, "GLW", "Failed to make EGL context current");
    return -1;
  }

  // Get screen dimensions
  eglQuerySurface(gp->egl_display, gp->egl_surface, EGL_WIDTH, &gp->screen_width);
  eglQuerySurface(gp->egl_display, gp->egl_surface, EGL_HEIGHT, &gp->screen_height);

  TRACE(TRACE_DEBUG, "GLW", "EGL initialized: %dx%d", gp->screen_width, gp->screen_height);

  return 0;
}

/**
 * Cleanup EGL
 */
static void
glw_switch_fini_egl(glw_switch_t *gp)
{
  if (gp->egl_display != EGL_NO_DISPLAY) {
    eglMakeCurrent(gp->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    if (gp->egl_context != EGL_NO_CONTEXT) {
      eglDestroyContext(gp->egl_display, gp->egl_context);
    }

    if (gp->egl_surface != EGL_NO_SURFACE) {
      eglDestroySurface(gp->egl_display, gp->egl_surface);
    }

    eglTerminate(gp->egl_display);
  }

  gp->egl_display = EGL_NO_DISPLAY;
  gp->egl_surface = EGL_NO_SURFACE;
  gp->egl_context = EGL_NO_CONTEXT;
}
#endif

/**
 * Process controller input using libnx pad API
 */
static void
glw_switch_process_input(glw_switch_t *gp)
{
  // Scan the gamepad. This should be done once for each frame
  padUpdate(&gp->pad);

  // Get buttons that are newly pressed in this frame
  u64 kDown = padGetButtonsDown(&gp->pad);

  // Get buttons that are currently held
  u64 kHeld = padGetButtons(&gp->pad);

  // Process each button
  if (kDown & HidNpadButton_A) send_action_from_button(HidNpadButton_A, kDown, kHeld);
  if (kDown & HidNpadButton_B) send_action_from_button(HidNpadButton_B, kDown, kHeld);
  if (kDown & HidNpadButton_X) send_action_from_button(HidNpadButton_X, kDown, kHeld);
  if (kDown & HidNpadButton_Y) send_action_from_button(HidNpadButton_Y, kDown, kHeld);
  if (kDown & HidNpadButton_Up) send_action_from_button(HidNpadButton_Up, kDown, kHeld);
  if (kDown & HidNpadButton_Down) send_action_from_button(HidNpadButton_Down, kDown, kHeld);
  if (kDown & HidNpadButton_Left) send_action_from_button(HidNpadButton_Left, kDown, kHeld);
  if (kDown & HidNpadButton_Right) send_action_from_button(HidNpadButton_Right, kDown, kHeld);
  if (kDown & HidNpadButton_Plus) send_action_from_button(HidNpadButton_Plus, kDown, kHeld);
  if (kDown & HidNpadButton_Minus) send_action_from_button(HidNpadButton_Minus, kDown, kHeld);
  if (kDown & HidNpadButton_L) send_action_from_button(HidNpadButton_L, kDown, kHeld);
  if (kDown & HidNpadButton_R) send_action_from_button(HidNpadButton_R, kDown, kHeld);
  if (kDown & HidNpadButton_ZL) send_action_from_button(HidNpadButton_ZL, kDown, kHeld);
  if (kDown & HidNpadButton_ZR) send_action_from_button(HidNpadButton_ZR, kDown, kHeld);

  // Handle analog stick for continuous navigation
  HidAnalogStickState stick_l = padGetStickPos(&gp->pad, 0);
  const int stick_threshold = 0x2800; // Threshold for stick movement

  if (stick_l.x > stick_threshold) {
    // Right
    if (kHeld & HidNpadButton_StickL) send_action_from_button(HidNpadButton_Right, kDown, kHeld);
  } else if (stick_l.x < -stick_threshold) {
    // Left
    if (kHeld & HidNpadButton_StickL) send_action_from_button(HidNpadButton_Left, kDown, kHeld);
  }

  if (stick_l.y > stick_threshold) {
    // Up
    if (kHeld & HidNpadButton_StickL) send_action_from_button(HidNpadButton_Up, kDown, kHeld);
  } else if (stick_l.y < -stick_threshold) {
    // Down
    if (kHeld & HidNpadButton_StickL) send_action_from_button(HidNpadButton_Down, kDown, kHeld);
  }

  gp->kDownOld = kDown;
}

/**
 * Main loop
 */
static void
glw_switch_mainloop(glw_switch_t *gp)
{
  TRACE(TRACE_INFO, "GLW", "Starting main loop");
  int frame_count = 0;

  while (!gp->gp_stop && appletMainLoop()) {
    frame_count++;

    // Process input
    glw_switch_process_input(gp);

    // Update GLW
    glw_prepare_frame(&gp->gr, 0);
    glw_idle(&gp->gr);

    // Render
    glw_render0(&gp->gr, NULL);

    if (frame_count % 60 == 0) {
      TRACE(TRACE_DEBUG, "GLW", "Frame %d rendered", frame_count);
    }

#if CONFIG_GLW_BACKEND_DEKO3D
    // Swap buffers using Deko3d
    glw_backend_root_t *be = &gp->gr.gr_be;
    dkQueueSubmitCommands(be->d3_queue, dkCmdBufFinishList(be->d3_cmdbuf));
    dkQueuePresentImage(be->d3_queue, be->d3_swapchain, 0);
    dkQueueWaitIdle(be->d3_queue);
#else
    // Swap buffers using EGL
    eglSwapBuffers(gp->egl_display, gp->egl_surface);
#endif

    // Small delay to prevent CPU hogging
    svcSleepThread(1e7); // 10ms
  }

  TRACE(TRACE_INFO, "GLW", "Main loop ended after %d frames", frame_count);
}

/**
 * Initialize GLW for Switch
 */
static int
glw_switch_init(glw_switch_t *gp)
{
  TRACE(TRACE_INFO, "GLW", "Initializing Switch services");

  // Initialize applet services
  appletInitialize();
  TRACE(TRACE_INFO, "GLW", "Applet initialized");

  // Initialize filesystem
  fsInitialize();
  TRACE(TRACE_INFO, "GLW", "Filesystem initialized");

  // Configure pad input
  padConfigureInput(1, HidNpadStyleSet_NpadStandard);
  TRACE(TRACE_INFO, "GLW", "Pad input configured");

  // Initialize the default gamepad
  padInitializeDefault(&gp->pad);
  gp->kDownOld = 0;
  TRACE(TRACE_INFO, "GLW", "Pad initialized");

#if !CONFIG_GLW_BACKEND_DEKO3D
  // Initialize EGL/OpenGL ES for software backend
  if (glw_switch_init_egl(gp) != 0) {
    return -1;
  }

  // Set viewport
  glViewport(0, 0, gp->screen_width, gp->screen_height);

  // Enable blending
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
#endif

  return 0;
}

/**
 * GLW entry point for Switch
 */
int
glw_switch_main(void)
{
  TRACE(TRACE_INFO, "GLW", "Starting GLW Switch main");

  glw_switch_t *gp = calloc(1, sizeof(glw_switch_t));
  if (gp == NULL) {
    TRACE(TRACE_ERROR, "GLW", "Failed to allocate glw_switch_t");
    return 1;
  }
  TRACE(TRACE_INFO, "GLW", "glw_switch_t allocated");

  glwswitch = gp;
  prop_t *root = gp->gr.gr_prop_ui = prop_create(prop_get_global(), "ui");
  gp->gr.gr_prop_nav = nav_spawn();
  TRACE(TRACE_INFO, "GLW", "Properties created");

  prop_set_int(prop_create(root, "fullscreen"), 1);

  if (glw_switch_init(gp)) {
    TRACE(TRACE_ERROR, "GLW", "Failed to initialize Switch services");
    return 1;
  }
  TRACE(TRACE_INFO, "GLW", "Switch services initialized");

  gp->gr.gr_prop_maxtime = 10000;

  glw_root_t *gr = &gp->gr;

#if CONFIG_GLW_BACKEND_DEKO3D
  TRACE(TRACE_INFO, "GLW", "Initializing Deko3d backend");
  if (glw_deko3d_init_context(gr)) {
    TRACE(TRACE_ERROR, "GLW", "Failed to initialize Deko3d context");
    return 1;
  }
  TRACE(TRACE_INFO, "GLW", "Deko3d backend initialized");
#endif

  TRACE(TRACE_INFO, "GLW", "Initializing GLW core");
  if (glw_init2(gr,
               GLW_INIT_KEYBOARD_MODE |
               GLW_INIT_OVERSCAN |
               GLW_INIT_IN_FULLSCREEN)) {
    TRACE(TRACE_ERROR, "GLW", "Failed to initialize GLW core");
    return 1;
  }
  TRACE(TRACE_INFO, "GLW", "GLW core initialized");

  TRACE(TRACE_DEBUG, "GLW", "loading universe");

  glw_load_universe(gr);
  TRACE(TRACE_INFO, "GLW", "Universe loaded");

  glw_switch_mainloop(gp);
  TRACE(TRACE_INFO, "GLW", "Mainloop exited");

  glw_unload_universe(gr);
  glw_reap(gr);
  glw_reap(gr);

  // Cleanup
#if CONFIG_GLW_BACKEND_DEKO3D
  glw_deko3d_fini(gr);
#else
  glw_switch_fini_egl(gp);
#endif
  // hidExit(); // Not initialized yet
  fsExit();
  appletExit();

  free(gp);
  return 0;
}
