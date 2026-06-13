FREETYPE_VER="2.13.2"
BD=${BUILDDIR}/freetype/build

build:
	${MAKE} -C ${BD}
	${MAKE} -C ${BD} install

configure:
	