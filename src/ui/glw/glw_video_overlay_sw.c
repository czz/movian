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

#include "glw.h"
#include "glw_video_common.h"

/**
 * Stubs for video overlay functions (software rendering)
 */

void
glw_video_overlay_set_pts(glw_video_t *gv, int64_t pts)
{
  // No-op for software rendering
}

void
glw_video_overlay_render(glw_video_t *gv, const glw_rctx_t *rc)
{
  // No-op for software rendering
}

void
glw_video_overlay_layout(glw_video_t *gv)
{
  // No-op for software rendering
}

void
glw_video_overlay_deinit(glw_video_t *gv)
{
  // No-op for software rendering
}

void
glw_video_overlay_pointer_event(glw_video_t *gv, event_t *e)
{
  // No-op for software rendering
}
