# FST v1 — Wars of Heaven (WoH) sample + Moonwalk scripts
# After configure+build, use --root ${CMAKE_BINARY_DIR}/bin (or set source_root below).
# Runtime: WarsOfHeaven.exe, SDL3 + SolsticeEngine DLLs, shaders, fonts, scripts/, optional RELIC bootstrap.
#
# Optional RELIC: ship a bootstrap or content `.relic` beside the exe (see docs/RelicFormat.md). Rows below
# use optional=yes so packaging succeeds when files are not yet produced.

source_root = ${SOLSTICE_BUILD_DIR}/bin
lib_root = ${SOLSTICE_PACKAGE_LIB_DIR}
package_id = com.solstice.warsofheaven
display_name = Wars of Heaven
version = 0.0.0
maintainer = Solstice
description = WoH sample: space sim smoke test with Moonwalk UI and match scripting
deb_depends = libc6, libstdc++6

kind	src	dst	os	base	optional
file	WarsOfHeaven.exe		windows	root	no
file	SDL3.dll		windows	root	no
file	SolsticeEngine.dll		windows	root	no
dir	shaders	shaders	windows	root	no
dir	fonts	fonts	windows	root	no
dir	scripts	scripts	windows	root	no
file	game.data.relic		windows	root	yes
file	WarsOfHeaven		linux	root	no
glob	libSDL3.so*		linux	lib	no
glob	libsolsticeengine.so*		linux	lib	no
dir	shaders	shaders	linux	root	no
dir	fonts	fonts	linux	root	no
dir	scripts	scripts	linux	root	no
file	game.data.relic		linux	root	yes
file	WarsOfHeaven		darwin	root	no
glob	libSDL3*.dylib		darwin	lib	no
glob	libsolsticeengine*.dylib		darwin	lib	no
dir	shaders	shaders	darwin	root	no
dir	fonts	fonts	darwin	root	no
dir	scripts	scripts	darwin	root	no
file	game.data.relic		darwin	root	yes
