# FST v1 — SMM / MovieMaker release payload
# CMake target: MovieMaker. May include ffmpeg.exe next to the binary when configured with
# SOLSTICE_FFMPEG_EXECUTABLE (optional post-build copy on Windows).
#
# ffmpeg row is optional: if missing, packaging warns and continues.

source_root = ${SOLSTICE_BUILD_DIR}/bin
lib_root = ${SOLSTICE_PACKAGE_LIB_DIR}
package_id = com.solstice.moviemaker
display_name = Solstice Movie Maker
version = 0.0.0
maintainer = Solstice
description = Parallax / SMM authoring (MovieMaker)
deb_depends = libc6, libstdc++6

kind	src	dst	os	base	optional
file	MovieMaker.exe		windows	root	no
file	SDL3.dll		windows	root	no
file	SolsticeEngine.dll		windows	root	no
file	ffmpeg.exe		windows	root	yes
dir	shaders	shaders	windows	root	no
dir	fonts	fonts	windows	root	no
file	MovieMaker		linux	root	no
glob	libSDL3.so*		linux	lib	no
glob	libsolsticeengine.so*		linux	lib	no
dir	shaders	shaders	linux	root	no
dir	fonts	fonts	linux	root	no
file	ffmpeg		linux	lib	yes
file	MovieMaker		darwin	root	no
glob	libSDL3*.dylib		darwin	lib	no
glob	libsolsticeengine*.dylib		darwin	lib	no
dir	shaders	shaders	darwin	root	no
dir	fonts	fonts	darwin	root	no
file	ffmpeg		darwin	lib	yes
