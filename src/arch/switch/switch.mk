# Add libnx and mbedTLS include paths
CFLAGS += -I${DEVKITPRO}/libnx/include
CFLAGS += -I${DEVKITPRO}/portlibs/switch/include
CFLAGS += -I${CURDIR}/ext/sqlite-amalgamation-3450000
CFLAGS += -I${CURDIR}/src/arch/switch

# SQLite compilation flags for Switch
CFLAGS += -DSQLITE_THREADSAFE=0
CFLAGS += -DSQLITE_OMIT_LOAD_EXTENSION
CFLAGS += -DSQLITE_ENABLE_FTS3
CFLAGS += -DSQLITE_ENABLE_FTS4
CFLAGS += -DSQLITE_ENABLE_FTS5
CFLAGS += -DSQLITE_ENABLE_RTREE
CFLAGS += -DSQLITE_ENABLE_JSON1
CFLAGS += -DSQLITE_DISABLE_FTS3_REGEX
CFLAGS += -DSQLITE_DISABLE_FTS4_DEFERRED_SIZE_CHECK
CFLAGS += -DSQLITE_NO_SYNC
CFLAGS += -DSQLITE_DEFAULT_MMAP_SIZE=0

# Add Switch-specific sources to the main SRCS list
SRCS += src/arch/switch/switch_main.c \
	src/arch/switch/switch_audio.c \
	src/arch/linux/linux_misc.c \
	src/arch/linux/linux_trap.c \
	src/fileaccess/fa_opencookie.c \
	src/arch/switch/asyncio_switch.c \
	src/arch/posix/posix_threads.c \
	src/networking/net_posix.c \
	src/networking/net_ifaddr.c \
	src/networking/net_mbedtls.c \
	src/networking/websocket.c \
	src/ipc/devevent.c \
	ext/tlsf/tlsf.c \
	src/htsmsg/persistent_file.c \
	src/metadata/metadata.c \
	src/db/db_support.c \
	ext/sqlite-amalgamation-3450000/sqlite3.c \
	src/arch/switch/sqlite_vfs_switch.c \
	src/arch/switch/switch_stubs.c

# Exclude files that require SQLite (disabled for Switch)
# SRCS := $(filter-out src/db/kvstore.c, $(SRCS))

# Exclude files that require metadata functions (not fully ported)
# SRCS := $(filter-out src/upnp/upnp_browse.c, $(SRCS))
# SRCS := $(filter-out src/fileaccess/fa_filepicker.c, $(SRCS))
# SRCS := $(filter-out src/image/image.c, $(SRCS))

# Exclude files that require FreeType (now enabled with brotli)
# SRCS := $(filter-out src/text/freetype.c, $(SRCS))
# SRCS := $(filter-out src/text/fontconfig.c, $(SRCS))
# SRCS := $(filter-out src/image/rasterizer_ft.c, $(SRCS))
# SRCS := $(filter-out src/subtitles/sub_ass.c, $(SRCS))
# SRCS := $(filter-out src/subtitles/video_overlay.c, $(SRCS))
# SRCS := $(filter-out src/image/jpeg.c, $(SRCS))

# Exclude files that require kvstore (SQLite-based key-value store)
# SRCS := $(filter-out src/metadata/playinfo.c, $(SRCS))
# SRCS := $(filter-out src/media/media_track.c, $(SRCS))
# SRCS := $(filter-out src/prop/prop_proxy.c, $(SRCS))

# Exclude files that require image functions
# SRCS := $(filter-out src/image/image_decoder_ffmpeg.c, $(SRCS))
# SRCS := $(filter-out src/image/nanosvg.c, $(SRCS))
# SRCS := $(filter-out src/image/svg.c, $(SRCS))

# Exclude files that require media_track functions
# SRCS := $(filter-out src/subtitles/subtitles.c, $(SRCS))
# SRCS := $(filter-out src/subtitles/vobsub.c, $(SRCS))
# SRCS := $(filter-out src/video/video_playback.c, $(SRCS))

# Exclude files that require vobsub or subtitle_settings
# SRCS := $(filter-out src/subtitles/ext_subtitles.c, $(SRCS))
# SRCS := $(filter-out src/subtitles/sub_ass.c, $(SRCS))

# Exclude files that require subtitle_settings or subtitles functions
# SRCS := $(filter-out src/media/media_settings.c, $(SRCS))
# SRCS := $(filter-out src/subtitles/video_overlay.c, $(SRCS))
# SRCS := $(filter-out src/video/video_decoder.c, $(SRCS))

# Exclude files that require media_track or mp_track_mgr functions
# SRCS := $(filter-out src/media/media.c, $(SRCS))
# SRCS := $(filter-out src/media/media_event.c, $(SRCS))
# SRCS := $(filter-out src/media/media_buf.c, $(SRCS))
# SRCS := $(filter-out src/media/media_codec.c, $(SRCS))
# SRCS := $(filter-out src/media/media_queue.c, $(SRCS))

# Exclude files that require image functions
# SRCS := $(filter-out src/fileaccess/fa_imageloader.c, $(SRCS))
# SRCS := $(filter-out src/fileaccess/fa_video.c, $(SRCS))

# Exclude files that require mp_* (media player) functions
# SRCS := $(filter-out src/playqueue.c, $(SRCS))
# SRCS := $(filter-out src/audio2/audio.c, $(SRCS))
SRCS := $(filter-out src/audio2/audio_test.c, $(SRCS))

# Exclude files that require kv_url_opt functions
# SRCS := $(filter-out src/settings.c, $(SRCS))

# Exclude files that require vobsub functions
# SRCS := $(filter-out src/subtitles/dvdspu.c, $(SRCS))

# Exclude files that require filepicker functions
# SRCS := $(filter-out src/ui/clipboard.c, $(SRCS))

# Exclude files that require media_buf_alloc_unlocked
# SRCS := $(filter-out src/backend/hls/hls_ts.c, $(SRCS))

# Exclude files that require settings functions
# SRCS := $(filter-out src/media/media_settings.c, $(SRCS))
# SRCS := $(filter-out src/navigator.c, $(SRCS))
# SRCS := $(filter-out src/runcontrol.c, $(SRCS))
# SRCS := $(filter-out src/sd/sd.c, $(SRCS))
# SRCS := $(filter-out src/video/video_settings.c, $(SRCS))

# Exclude files that require navigator functions
# SRCS := $(filter-out src/plugins.c, $(SRCS))
# SRCS := $(filter-out src/service.c, $(SRCS))
# SRCS := $(filter-out src/text/fontstash.c, $(SRCS))
# SRCS := $(filter-out src/upgrade.c, $(SRCS))

# Exclude files that require video_settings
# SRCS := $(filter-out src/fileaccess/fa_video.c, $(SRCS))

# Exclude files that require backend media functions
# SRCS := $(filter-out src/backend/htsp/htsp.c, $(SRCS))
# SRCS := $(filter-out src/backend/icecast/icecast.c, $(SRCS))

# Exclude files that require ecmascript functions
# SRCS := $(filter-out src/ecmascript/es_kvstore.c, $(SRCS))
# SRCS := $(filter-out src/ecmascript/es_scrobble.c, $(SRCS))
# SRCS := $(filter-out src/ecmascript/es_service.c, $(SRCS))
# SRCS := $(filter-out src/ecmascript/es_subtitles.c, $(SRCS))

# Exclude files that require video decoder functions
# SRCS := $(filter-out src/ffmpeg.c, $(SRCS))

# Exclude files that require image functions
# SRCS := $(filter-out src/image/pixmap.c, $(SRCS))

# Exclude files that require SSL/networking functions
# SRCS := $(filter-out src/networking/net_common.c, $(SRCS))
# SRCS := $(filter-out src/fileaccess/fa_http.c, $(SRCS))
# SRCS := $(filter-out src/networking/ftp_server.c, $(SRCS))
# SRCS := $(filter-out src/networking/http_server.c, $(SRCS))
# SRCS := $(filter-out src/backend/bittorrent/peer.c, $(SRCS))
# SRCS := $(filter-out src/networking/asyncio_posix.c, $(SRCS))

# Exclude all bittorrent files (require advanced asyncio/HTTP)
SRCS := $(filter-out src/backend/bittorrent/bencode.c, $(SRCS))
SRCS := $(filter-out src/backend/bittorrent/bt_backend.c, $(SRCS))
SRCS := $(filter-out src/backend/bittorrent/diskio.c, $(SRCS))
SRCS := $(filter-out src/backend/bittorrent/fa_torrent.c, $(SRCS))
SRCS := $(filter-out src/backend/bittorrent/magnet.c, $(SRCS))
SRCS := $(filter-out src/backend/bittorrent/peer.c, $(SRCS))
SRCS := $(filter-out src/backend/bittorrent/torrent.c, $(SRCS))
SRCS := $(filter-out src/backend/bittorrent/tracker.c, $(SRCS))
SRCS := $(filter-out src/backend/bittorrent/tracker_http.c, $(SRCS))
SRCS := $(filter-out src/backend/bittorrent/tracker_udp.c, $(SRCS))

# Exclude files that require advanced asyncio features not yet implemented
# SRCS := $(filter-out src/ecmascript/es_websocket.c, $(SRCS))
# SRCS := $(filter-out src/networking/asyncio_http.c, $(SRCS))
# SRCS := $(filter-out src/networking/ftp_server.c, $(SRCS))
# SRCS := $(filter-out src/networking/http_server.c, $(SRCS))
# SRCS := $(filter-out src/api/soap.c, $(SRCS))
# SRCS := $(filter-out src/api/xmlrpc.c, $(SRCS))
# SRCS := $(filter-out src/ecmascript/es_io.c, $(SRCS))
# SRCS := $(filter-out src/backend/hls/hls.c, $(SRCS))
# SRCS := $(filter-out src/backend/htsp/htsp.c, $(SRCS))
# SRCS := $(filter-out src/api/screenshot.c, $(SRCS))
# SRCS := $(filter-out src/api/stpp.c, $(SRCS))

# Exclude files that require pipe/posix_memalign
# SRCS := $(filter-out src/arch/posix/posix.c, $(SRCS))

# Exclude files that require service functions
# SRCS := $(filter-out src/plugins.c, $(SRCS))

# Exclude files that require upgrade functions
# SRCS := $(filter-out src/usage.c, $(SRCS))

# Add stub file for missing functions
SRCS += src/arch/switch/switch_stubs.c

# Add compiler flags for libnx compatibility
CFLAGS += -fPIC -fdata-sections -ffunction-sections

# Use libnx's switch.specs but add flags to allow dynamic relocations
LDFLAGS += -specs=${DEVKITPRO}/libnx/switch.specs
LDFLAGS += -L${DEVKITPRO}/libnx/lib -L${DEVKITPRO}/portlibs/switch/lib -L${DEVKITA64}/lib

# Force linking of specific NVN symbols using -u
LDFLAGS_cfg += -u nvMapCreate -u nvMapClose -u nvAddressSpaceMap
LDFLAGS_cfg += -u nvAddressSpaceMapFixed -u nvAddressSpaceUnmap -u nvAddressSpaceModify
LDFLAGS_cfg += -u armDCacheFlush -u errorApplicationCreate -u errorApplicationShow
# Force linking of NVN GPU functions
LDFLAGS_cfg += -u nvGpuInit -u nvGpuExit -u nvGpuGetCharacteristics
LDFLAGS_cfg += -u nvGpuGetZcullCtxSize -u nvGpuGetZcullInfo
LDFLAGS_cfg += -u nvGpuChannelCreate -u nvGpuChannelClose
LDFLAGS_cfg += -u nvGpuChannelAppendEntry -u nvGpuChannelKickoff
LDFLAGS_cfg += -u nvGpuChannelZcullBind -u nvGpuChannelGetErrorNotification
LDFLAGS_cfg += -u nvGpuChannelGetErrorInfo
LDFLAGS_cfg += -lnx -ldeko3dd -lm -lmbedtls -lmbedx509 -lmbedcrypto

# Add linker flags to allow dynamic relocations in read-only segments
LDFLAGS += -z notext -z now

# Override default target to build NRO
.DEFAULT_GOAL := ${PROG}.nro

# Build bundle first to ensure all objects are compiled
# Then relink with libnx (bundle.c now returns native path on Switch)
${PROG}.elf: ${PROG}.bundle
	@echo "Relinking with libnx (allowing dynamic relocations, with bundle.o)..."
	@$(LINKER) -o $@ $(OBJS) ${BUILDDIR}/support/dataroot/bundle.o $(BUNDLE_OBJS) $(LDFLAGS) ${LDFLAGS_cfg}

${PROG}.nro: ${PROG}.elf
	@echo "Creating NRO from ELF..."
	@${DEVKITPRO}/tools/bin/elf2nro $< $@

strip: ${PROG}.nro
	@echo "Stripping NRO..."
	@${STRIP} -o ${PROG}.stripped.nro $<

install: ${PROG}.nro
	mkdir -p ${INSTDIR}/bin
	mkdir -p ${INSTDIR}/lib

	cp ${PROG}.nro ${INSTDIR}/bin/
	cp -a ${GLLIBS}/*.so ${INSTDIR}/lib/
	cp -a ${FFMPEG_INSTALL_DIR}/lib/lib* ${INSTDIR}/lib/
