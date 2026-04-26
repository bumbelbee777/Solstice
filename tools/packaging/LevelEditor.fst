# FST v1 — Jackhammer (CMake target LevelEditor) release payload
# Product name: Jackhammer. Executable: LevelEditor.exe (Windows) / LevelEditor (Unix).
# Point --root at ${CMAKE_BINARY_DIR}/bin after build. See Sharpon.fst for column reference.

source_root = ${SOLSTICE_BUILD_DIR}/bin
lib_root = ${SOLSTICE_PACKAGE_LIB_DIR}
package_id = com.solstice.jackhammer
display_name = Jackhammer
version = 0.0.0
maintainer = Solstice
description = .smf level editor (Jackhammer / LevelEditor)
deb_depends = libc6, libstdc++6

kind	src	dst	os	base	optional
file	LevelEditor.exe		windows	root	no
file	SDL3.dll		windows	root	no
file	SolsticeEngine.dll		windows	root	no
dir	shaders	shaders	windows	root	no
dir	fonts	fonts	windows	root	no
file	LevelEditor		linux	root	no
glob	libSDL3.so*		linux	lib	no
glob	libsolsticeengine.so*		linux	lib	no
dir	shaders	shaders	linux	root	no
dir	fonts	fonts	linux	root	no
file	LevelEditor		darwin	root	no
glob	libSDL3*.dylib		darwin	lib	no
glob	libsolsticeengine*.dylib		darwin	lib	no
dir	shaders	shaders	darwin	root	no
dir	fonts	fonts	darwin	root	no
