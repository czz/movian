#
#  Showtime mediacenter
#  Copyright (C) 2007-2009 Andreas Öman
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

include ${CURDIR}/config.default

OPTFLAGS ?= -O2

BUILDDIR = build.${PLATFORM}

include ${BUILDDIR}/config.mak

CFLAGS  = -Wall -Werror -Wwrite-strings -Wno-deprecated-declarations 
CFLAGS += -Wmissing-prototypes -Iext/dvd


#
# Core
#
SRCS += src/main.c \
	src/runcontrol.c \
	src/version.c \
	src/navigator.c \
	src/backend/backend.c \
	src/backend/search.c \
	src/media.c \
	src/event.c \
	src/keyring.c \
	src/settings.c \
	src/prop.c \
	src/bookmarks.c \
	src/service.c \
	src/notifications.c \
	src/playqueue.c \
	src/arch/arch_${OSENV}.c \
	src/ui/ui.c \
	src/keymapper.c \

ifeq ($(PLATFORM), linux)
SRCS += src/arch/linux.c
endif

ifeq ($(PLATFORM), osx)
SRCS += src/arch/darwin.c
endif

#
# Misc support
#
SRCS +=	src/misc/ptrvec.c \
	src/misc/callout.c \
	src/misc/rstr.c \
	src/misc/pixmap.c \
	src/misc/jpeg.c \
	src/misc/gz.c \
	src/misc/string.c \

#
# HTSMSG
#
SRCS +=	src/htsmsg/htsbuf.c \
	src/htsmsg/htsmsg.c \
	src/htsmsg/htsmsg_json.c \
	src/htsmsg/htsmsg_xml.c \
	src/htsmsg/htsmsg_binary.c \
	src/htsmsg/htsmsg_store_${OSENV}.c \

#
# Virtual FS system
#
SRCS += src/fileaccess/fileaccess.c \
	src/fileaccess/fa_probe.c \
	src/fileaccess/fa_imageloader.c \
	src/fileaccess/fa_backend.c \
	src/fileaccess/fa_scanner.c \
	src/fileaccess/fa_video.c \
	src/fileaccess/fa_audio.c \
	src/fileaccess/fa_fs.c \
	src/fileaccess/fa_rar.c \
	src/fileaccess/fa_http.c \
	src/fileaccess/fa_zip.c \
	src/fileaccess/fa_zlib.c \
	src/fileaccess/fa_bundle.c \
	src/fileaccess/fa_sidfile.c \

SRCS-$(CONFIG_TINYSMB)  += src/fileaccess/fa_tinysmb.c

SRCS-$(CONFIG_LIBSMBCLIENT) += src/fileaccess/fa_smb.c

SRCS-$(CONFIG_LOCATEDB) += src/fileaccess/fa_locatedb.c

SRCS-$(CONFIG_SPOTLIGHT) += src/fileaccess/fa_spotlight.c

SRCS += ext/audio/sid.c

#
# Service Discovery
#

SRCS 			+= src/sd/sd.c \

SRCS-$(CONFIG_AVAHI) 	+= src/sd/avahi.c \

${BUILDDIR}/src/sd/avahi.o : CFLAGS = $(CFLAGS_AVAHI) -Wall -Werror

BUNDLES += resources/tvheadend
BUNDLES += resources/fileaccess


SRCS-$(CONFIG_BONJOUR) 	+= src/sd/bonjour.c

#
# APIs
#
SRCS += 		src/api/api.c \
			src/api/xmlrpc.c \
			src/api/opensubtitles.c \
			src/api/lastfm.c \

#
# Networking
#
SRCS += src/networking/net_common.c \

SRCS-$(CONFIG_POSIX_NETWORKING) += src/networking/net_posix.c
SRCS-$(CONFIG_LIBOGC) += src/networking/net_libogc.c

#
# Video support
#
SRCS += src/video/video_playback.c \
	src/video/video_decoder.c \
	src/video/video_subtitles.c \
	src/video/subtitles.c \

SRCS-$(CONFIG_DVD) += src/video/video_dvdspu.c

SRCS += ext/video/yadif.c

# Temporary fix for http://gcc.gnu.org/bugzilla/show_bug.cgi?id=11203
# -OO will result in compiler error
ifeq ($(PLATFORM), osx)
${BUILDDIR}/ext/video/yadif.o : CFLAGS = ${OPTFLAGS}
endif

SRCS-$(CONFIG_VDPAU) += src/video/vdpau.c

#
# Audio subsys
#
SRCS += src/audio/audio.c \
	src/audio/audio_decoder.c \
	src/audio/audio_fifo.c \
	src/audio/audio_iec958.c \

SRCS-$(CONFIG_LIBASOUND)  += src/audio/alsa/alsa_audio.c
SRCS-$(CONFIG_LIBPULSE)   += src/audio/pulseaudio/pulseaudio.c
SRCS-$(CONFIG_LIBOGC)     += src/audio/wii/wii_audio.c
SRCS-$(CONFIG_COREAUDIO)  += src/audio/coreaudio/coreaudio.c
SRCS                      += src/audio/dummy/dummy_audio.c

#
# DVD
#
SRCS-$(CONFIG_DVD)       += src/backend/dvd/dvd.c
SRCS-$(CONFIG_DVD_LINUX) += src/backend/dvd/linux_dvd.c
SRCS-$(CONFIG_DVD_WII)   += src/backend/dvd/wii_dvd.c
SRCS-$(CONFIG_CDDA)      += src/backend/dvd/cdda.c

#
# TV
#
SRCS  += src/backend/htsp/htsp.c \

#
# Spotify
#
SRCS-${CONFIG_SPOTIFY} += src/backend/spotify/spotify.c
BUNDLES-$(CONFIG_SPOTIFY) += resources/spotify

#
# Headweb
#
SRCS-$(CONFIG_LIBRTMP)    += src/backend/headweb/headweb.c
BUNDLES-$(CONFIG_LIBRTMP) += resources/headweb

#
# GLW user interface
#
SRCS-$(CONFIG_GLW)   += src/ui/glw/glw.c \
			src/ui/glw/glw_event.c \
			src/ui/glw/glw_view.c \
		     	src/ui/glw/glw_view_lexer.c \
		     	src/ui/glw/glw_view_parser.c \
			src/ui/glw/glw_view_eval.c \
			src/ui/glw/glw_view_preproc.c \
			src/ui/glw/glw_view_support.c \
			src/ui/glw/glw_view_attrib.c \
			src/ui/glw/glw_view_loader.c \
			src/ui/glw/glw_dummy.c \
			src/ui/glw/glw_container.c \
			src/ui/glw/glw_list.c \
			src/ui/glw/glw_array.c \
			src/ui/glw/glw_deck.c \
			src/ui/glw/glw_layer.c \
			src/ui/glw/glw_expander.c \
			src/ui/glw/glw_slider.c \
			src/ui/glw/glw_rotator.c  \
			src/ui/glw/glw_throbber3d.c  \
			src/ui/glw/glw_slideshow.c \
			src/ui/glw/glw_freefloat.c \
			src/ui/glw/glw_transitions.c \
			src/ui/glw/glw_navigation.c \
			src/ui/glw/glw_texture_loader.c \
			src/ui/glw/glw_image.c \
			src/ui/glw/glw_text_bitmap.c \
			src/ui/glw/glw_unicode.c \
			src/ui/glw/glw_fx_texrot.c \
			src/ui/glw/glw_bloom.c \
			src/ui/glw/glw_cube.c \
			src/ui/glw/glw_displacement.c \
			src/ui/glw/glw_coverflow.c \
			src/ui/glw/glw_mirror.c \
			src/ui/glw/glw_video_common.c \
			src/ui/glw/glw_video_overlay.c \
			src/ui/glw/glw_gradient.c \
			src/ui/glw/glw_bar.c \
			src/ui/glw/glw_flicker.c \

SRCS-$(CONFIG_GLW_FRONTEND_X11)	  += src/ui/glw/glw_x11.c \
				     src/ui/linux/x11_common.c

SRCS-$(CONFIG_GLW_FRONTEND_COCOA) += src/ui/glw/glw_cocoa.m
SRCS-$(CONFIG_GLW_BACKEND_OPENGL) += src/ui/glw/glw_opengl.c
SRCS-$(CONFIG_GLW_BACKEND_OPENGL) += src/ui/glw/glw_texture_opengl.c
SRCS-$(CONFIG_GLW_BACKEND_OPENGL) += src/ui/glw/glw_video_opengl.c
SRCS-$(CONFIG_GLW_BACKEND_OPENGL) += src/ui/glw/glw_video_vdpau.c
SRCS-$(CONFIG_GLW_FRONTEND_WII)	  += src/ui/glw/glw_wii.c
SRCS-$(CONFIG_GLW_BACKEND_GX)     += src/ui/glw/glw_texture_gx.c
SRCS-$(CONFIG_GLW_BACKEND_GX)     += src/ui/glw/glw_gx.c
SRCS-$(CONFIG_GLW_BACKEND_GX)     += src/ui/glw/glw_video_gx.c
SRCS-$(CONFIG_GLW_BACKEND_GX)     += src/ui/glw/glw_gxasm.S

SRCS-$(CONFIG_NVCTRL)             += src/ui/linux/nvidia.c

#
# GTK based interface
#
SRCS-$(CONFIG_GU) +=    src/ui/gu/gu.c \
			src/ui/gu/gu_helpers.c \
			src/ui/gu/gu_pixbuf.c \
			src/ui/gu/gu_popup.c \
			src/ui/gu/gu_menu.c \
			src/ui/gu/gu_menubar.c \
			src/ui/gu/gu_toolbar.c \
			src/ui/gu/gu_statusbar.c \
			src/ui/gu/gu_playdeck.c \
			src/ui/gu/gu_pages.c \
			src/ui/gu/gu_home.c \
			src/ui/gu/gu_directory.c \
			src/ui/gu/gu_directory_store.c \
			src/ui/gu/gu_directory_list.c \
			src/ui/gu/gu_directory_album.c \
			src/ui/gu/gu_directory_albumcollection.c \
			src/ui/gu/gu_cell_bar.c \
			src/ui/gu/gu_video.c \
			src/ui/linux/x11_common.c \

${BUILDDIR}/src/ui/gu/%.o : CFLAGS = $(CFLAGS_GTK) \
-Wall -Werror -Wmissing-prototypes -Wno-cast-qual -Wno-deprecated-declarations 

#
# IPC
#
SRCS                +=  src/ipc/ipc.c

SRCS-$(CONFIG_DBUS) +=  src/ipc/dbus/dbus.c \
			src/ipc/dbus/mpris.c \
			src/ipc/dbus/mpkeys.c

${BUILDDIR}/src/ipc/dbus/%.o : CFLAGS = $(CFLAGS_DBUS) \
-Wall -Werror -Wmissing-prototypes -Wno-cast-qual

SRCS-$(CONFIG_LIRC) +=  src/ipc/lirc.c

SRCS-$(CONFIG_SERDEV) +=	src/ipc/serdev/serdev.c \
				src/ipc/serdev/lgtv.c \


#
# Apple remote and keyspan front row remote
#
SRCS-$(CONFIG_APPLEREMOTE) += \
			src/ui/appleremote/AppleRemote.m \
			src/ui/appleremote/GlobalKeyboardDevice.m \
			src/ui/appleremote/HIDRemoteControlDevice.m \
			src/ui/appleremote/KeyspanFrontRowControl.m \
			src/ui/appleremote/MultiClickRemoteBehavior.m \
			src/ui/appleremote/RemoteControl.m \
			src/ui/appleremote/RemoteControlContainer.m \
			src/ui/appleremote/ShowtimeMainController.m

#
# librtmp
#

SRCS-$(CONFIG_LIBRTMP) +=	ext/rtmpdump/librtmp/amf.c \
				ext/rtmpdump/librtmp/hashswf.c \
				ext/rtmpdump/librtmp/log.c \
				ext/rtmpdump/librtmp/rtmp.c \
				ext/rtmpdump/librtmp/parseurl.c \

${BUILDDIR}/ext/rtmpdump/%.o : CFLAGS = ${OPTFLAGS}
SRCS-$(CONFIG_LIBRTMP)  +=      src/backend/rtmp/rtmp.c
${BUILDDIR}/src/backend/rtmp/rtmp.o : CFLAGS = -Wall -Werror -Iext/rtmpdump


#
# dvdcss
#
SRCS-$(CONFIG_DVD) += 	ext/dvd/dvdcss/css.c \
			ext/dvd/dvdcss/device.c \
			ext/dvd/dvdcss/error.c \
			ext/dvd/dvdcss/ioctl.c \
			ext/dvd/dvdcss/libdvdcss.c

ifeq ($(PLATFORM), osx)
DVDCSS_CFLAGS = -DDARWIN_DVD_IOCTL -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
else
ifeq ($(PLATFORM), linux)
DVDCSS_CFLAGS = -DHAVE_LINUX_DVD_STRUCT -DDVD_STRUCT_IN_LINUX_CDROM_H -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
endif
endif


${BUILDDIR}/ext/dvd/dvdcss/%.o : CFLAGS = \
 -DHAVE_LIMITS_H -DHAVE_UNISTD_H -DHAVE_ERRNO_H -DVERSION="0" $(DVDCSS_CFLAGS)

#
# libdvdread
#
SRCS-$(CONFIG_DVD) += 	ext/dvd/libdvdread/dvd_input.c \
			ext/dvd/libdvdread/dvd_reader.c \
			ext/dvd/libdvdread/dvd_udf.c \
			ext/dvd/libdvdread/ifo_read.c \
			ext/dvd/libdvdread/md5.c \
			ext/dvd/libdvdread/nav_read.c \
			ext/dvd/libdvdread/bitreader.c

${BUILDDIR}/ext/dvd/libdvdread/%.o : CFLAGS = \
 -DHAVE_DVDCSS_DVDCSS_H -DDVDNAV_COMPILE -Wno-strict-aliasing  -Iext/dvd 

#
# libdvdread
#
SRCS-$(CONFIG_DVD) += 	ext/dvd/dvdnav/dvdnav.c \
			ext/dvd/dvdnav/highlight.c \
			ext/dvd/dvdnav/navigation.c \
			ext/dvd/dvdnav/read_cache.c \
			ext/dvd/dvdnav/remap.c \
			ext/dvd/dvdnav/settings.c \
			ext/dvd/dvdnav/vm/vm.c \
			ext/dvd/dvdnav/vm/decoder.c \
			ext/dvd/dvdnav/vm/vmcmd.c \
			ext/dvd/dvdnav/searching.c

${BUILDDIR}/ext/dvd/dvdnav/%.o : CFLAGS = \
 -DVERSION=\"showtime\" -DDVDNAV_COMPILE -Wno-strict-aliasing -Iext/dvd \
 -Iext/dvd/dvdnav

#
# Spidermonkey
#
SRCS-$(CONFIG_SPIDERMONKEY) += ext/spidermonkey/jsapi.c	\
			ext/spidermonkey/jsarena.c	\
			ext/spidermonkey/jsarray.c	\
			ext/spidermonkey/jsatom.c	\
			ext/spidermonkey/jsbool.c	\
			ext/spidermonkey/jscntxt.c	\
			ext/spidermonkey/jsdate.c	\
			ext/spidermonkey/jsdbgapi.c	\
			ext/spidermonkey/jsdhash.c	\
			ext/spidermonkey/jsdtoa.c	\
			ext/spidermonkey/jsemit.c	\
			ext/spidermonkey/jsexn.c	\
			ext/spidermonkey/jsfun.c	\
			ext/spidermonkey/jsgc.c		\
			ext/spidermonkey/jshash.c	\
			ext/spidermonkey/jsinterp.c	\
			ext/spidermonkey/jsinvoke.c	\
			ext/spidermonkey/jsiter.c	\
			ext/spidermonkey/jslock.c	\
			ext/spidermonkey/jslog2.c	\
			ext/spidermonkey/jslong.c	\
			ext/spidermonkey/jsmath.c	\
			ext/spidermonkey/jsnum.c	\
			ext/spidermonkey/jsobj.c	\
			ext/spidermonkey/jsopcode.c     \
			ext/spidermonkey/jsparse.c	\
			ext/spidermonkey/jsprf.c	\
			ext/spidermonkey/jsregexp.c	\
			ext/spidermonkey/jsscan.c	\
			ext/spidermonkey/jsscope.c	\
			ext/spidermonkey/jsscript.c	\
			ext/spidermonkey/jsstr.c	\
			ext/spidermonkey/jsutil.c       \
			ext/spidermonkey/jsxdrapi.c	\
			ext/spidermonkey/jsxml.c	\
			ext/spidermonkey/prmjtime.c	\
                        src/arch/nspr/nspr.c            \
                        src/js/js.c                     \
                        src/js/js_prop.c                \

${BUILDDIR}/ext/spidermonkey/%.o : CFLAGS = \
	-Iext/spidermonkey -Isrc/arch/nspr -DDEBUG=1

CFLAGS_com += -DXP_UNIX -DJS_HAS_XML_SUPPORT -DJS_THREADSAFE -DJS_GC_ZEAL


# Various transformations
SRCS  += $(SRCS-yes)
DLIBS += $(DLIBS-yes)
SLIBS += $(SLIBS-yes)
SSRCS  = $(sort $(SRCS))
OBJS3=   $(SSRCS:%.S=$(BUILDDIR)/%.o)
OBJS2=   $(OBJS3:%.c=$(BUILDDIR)/%.o)
OBJS=    $(OBJS2:%.m=$(BUILDDIR)/%.o)
DEPS=    ${OBJS:%.o=%.d}
OBJDIRS= $(sort $(dir $(OBJS)))

# File bundles
BUNDLES += $(BUNDLES-yes)
BUNDLE_SRCS=$(BUNDLES:%=$(BUILDDIR)/bundles/%.c)
BUNDLE_DEPS=$(BUNDLE_SRCS:%.c=%.d)
BUNDLE_OBJS=$(BUNDLE_SRCS:%.c=%.o)
OBJDIRS+= $(sort $(dir $(BUNDLE_OBJS)))
.PRECIOUS: ${BUNDLE_SRCS}

# Common CFLAGS for all files
CFLAGS_com += -g -funsigned-char ${OPTFLAGS}
CFLAGS_com += -D_FILE_OFFSET_BITS=64
CFLAGS_com += -I${BUILDDIR} -I${CURDIR}/src -I${CURDIR}

# Tools

MKBUNDLE = $(CURDIR)/support/mkbundle

ifndef V
ECHO   = printf "$(1)\t%s\n" $(2)
BRIEF  = CC MKBUNDLE
MSG    = $@
$(foreach VAR,$(BRIEF), \
    $(eval $(VAR) = @$$(call ECHO,$(VAR),$$(MSG)); $($(VAR))))
endif

all:	makever ${PROG}

.PHONY:	clean distclean ffmpeg makever

${PROG}: ${FFBUILDDEP} $(OBJDIRS) $(OBJS) $(BUNDLE_OBJS) Makefile src/version.c
	$(CC) -o $@ $(OBJS) $(BUNDLE_OBJS) $(LDFLAGS) ${LDFLAGS_cfg}

$(OBJDIRS):
	@mkdir -p $@

${BUILDDIR}/%.o: %.[cm] ${FFBUILDDEP}
	$(CC) -MD -MP $(CFLAGS_com) $(CFLAGS) $(CFLAGS_cfg) -c -o $@ $(CURDIR)/$<

ffmpeg ${FFBUILDDEP}:
	cd ${BUILDDIR}/ffmpeg/build && ${MAKE} all
	cd ${BUILDDIR}/ffmpeg/build && ${MAKE} install

clean:
	rm -rf ${BUILDDIR}/src ${BUILDDIR}/ext ${BUILDDIR}/bundles
	find . -name "*~" | xargs rm -f

distclean: clean
	rm -rf build.*

reconfigure:
	$(CURDIR)/configure.${PLATFORM} $(CONFIGURE_ARGS)

# Create showtimeversion.h
src/version.c: $(BUILDDIR)/showtimeversion.h

makever:
	@$(CURDIR)/support/version.sh $(CURDIR) $(BUILDDIR)/showtimeversion.h


# Include dependency files if they exist.
-include $(DEPS) $(BUNDLE_DEPS)

# Include Platform specific targets
include support/${PLATFORM}.mk

# Bundle files
$(BUILDDIR)/bundles/%.o: $(BUILDDIR)/bundles/%.c
	$(CC) -I${CURDIR}/src/fileaccess -c -o $@ $<

$(BUILDDIR)/bundles/%.c: % $(CURDIR)/support/mkbundle
	$(MKBUNDLE) -o $@ -s $< -d ${BUILDDIR}/bundles/$<.d -p $<
