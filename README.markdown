Movian mediaplayer
==================

## Version 8.0.0

### Changes in v8.0.0

#### 1. Migration from libav to FFmpeg
- Replaced submodule `ext/libav` with `ext/ffmpeg` (branch release/7.1)
- Updated `support/configure.inc` to use FFmpeg instead of libav
- Modified build system to support FFmpeg

#### 2. Migration from polarssl to mbedtls
- Replaced polarssl with mbedtls 2.28.7
- Updated networking files to use mbedtls

#### 3. Nintendo Switch support (in development)
- Added new architecture `src/arch/switch/`
- Added GLW backend for Switch (`glw_switch.c`, `glw_deko3d.c/h`)
- Added configuration file `configure.switch`
- Added deko3d shaders in `bundles/shaders/` and `src/ui/glw/shaders/`

#### 4. Library updates
- zlib: 1.2.3 → 1.2.13
- freetype: 2.4.9 → 2.13.2
- sqlite: updated to version 3.45.0

#### 5. Platform status
- **PS3**: Deprecated for now
- **Nintendo Switch**: In development

## Support

If you'd like to support the development of Movian, you can buy me a coffee:

[![Support me on Ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/czz78)

## How to build for Linux

First you need to satisfy some dependencies (for Ubuntu 26.04 LTS)

	sudo apt-get install libfreetype6-dev libfontconfig1-dev libxext-dev libgl1-mesa-dev libasound2-dev libasound2-dev libgtk2.0-dev libxss-dev libxxf86vm-dev libxv-dev libvdpau-dev yasm libpulse-dev libssl-dev curl libwebkitgtk-dev libsqlite3-dev libavahi-client-dev

Then you need to configure:

	./configure

If your system lacks libwebkitgtk or some other lib you can configure like this:

	./configure --disable-webkit

If any dependencies are missing the configure script will complain.
You then have the option to disable that particular module/subsystem.

	make

Build the binary, after build the binary resides in `./build.linux/`.
Thus, to start it, just type:

	./build.linux/movian

Settings are stored in `~/.hts/showtime`

If you want to build with extra debugging options for development these options might be of interest:

	--cc=gcc-5 --extra-cflags=-fno-omit-frame-pointer --optlevel=g --sanitize=address --enable-bughunt


## How to build for Debian

To build a Debian package:

	./Autobuild.sh -t debian -v 8.0.0

This will generate a .deb package in the parent directory.

## How to build for Ubuntu 26.04 LTS

To build an Ubuntu 26.04 LTS package (based on Debian 13 Trixie):

	./Autobuild.sh -t ubuntu-26.04 -v 8.0.0

This will generate a .deb package in the parent directory.

## How to build for Android

To build for Android, you need the Android NDK and SDK installed.

	./configure.android --sdk=/opt/android/sdk-27/ --ndk=/opt/android/android-ndk-r23b/ --kind=api16_armv7
	make

Available architectures:
- --kind=api16_armv7 (ARMv7)
- --kind=api21_arm64 (ARMv8/ARMv9)
- --kind=api16_x86 (x86)

To sign the APK, you have two options:

**Option 1: Using environment variable**
	export MOVIAN_KEYSTORE_PASS=your_password

**Option 2: Using apksigner**
	apksigner sign --ks your_keystore.jks --ks-key-alias your_alias build.android/movian.apk

## How to build for Nintendo Switch (in development)

To build for Nintendo Switch, you need devkitA64 toolchain installed at /opt/devkitpro.

	./configure.switch

	make

The binary will be built in `./build.switch/`.

Note: This is currently in development and requires a Nintendo Switch development environment.

## How to build for Mac OS X (in development)

To build for Mac OS X you need Xcode and yasm. Xcode should be installed from Mac Appstore.

To install yasm, install [Brew](http://brew.sh/) and then

	$ brew install yasm

Now run configure

	$ ./configure

Or if you build for release

	$ ./configure --release

If configured successfully run:

	$ make

Run Movian binary from build directory

	$ build.osx/Movian.app/Contents/MacOS/movian

Note that in this case Movian loads all resources from current directory
so this binary can't be run elsewhere.

If you want a build that can be run as a normal Mac Application you shold do

	$ make dist

This will generate a DMG

## How to build for Raspberry Pi

First you need to satisfy some dependencies (for Ubuntu 26.04 LTS 64bit):

	sudo apt-get install git-core build-essential autoconf bison flex libelf-dev libtool pkg-config texinfo libncurses5-dev libz-dev python-dev libssl-dev libgmp3-dev ccache zip squashfs-tools

$ ./Autobuild.sh -t rpi -v 8.0.0

To update Movian on rpi with compiled one, enable Binreplace in settings:dev and issue:

	curl --data-binary @build.rpi/showtime.sqfs http://rpi_ip_address:42000/api/replace

