# FST v1 — Sharpon (script editor) release payload
# After a full configure+build, point --root (or "root" below) at ${CMAKE_BINARY_DIR}/bin
# so paths match the CMake post-build copy layout (Windows). LibUI is linked statically; only
# SDL3 + SolsticeEngine are shipped as shared libraries alongside the exe.
#
# Optional directives (override via CLI: --root, --lib-root, --build-dir):
#   source_root, lib_root, package_id, display_name, version, maintainer, description
#
# TSV table columns (tab-separated):
#   kind   file | dir | glob
#   src    path relative to base (root or lib) unless absolute; ${VAR} from env; ${SOLSTICE_BUILD_DIR} from --build-dir
#   dst    path under staged package (empty = same basename as src, dirs keep trailing name)
#   os     all | windows | linux | darwin
#   base   root | lib  — "lib" uses lib_root (defaults to source_root; set for Linux .so not co-located with the exe)
#   optional  yes | no  — if yes, missing source logs a warning and the row is skipped (not an error)
#
# Linux: CMake does not copy .so into bin like Windows. Set lib_root to a directory that contains
#   libSDL3*.so* and libsolsticeengine.so* (e.g. a folder you populated from the build tree), or copy them
#   next to the executable and keep base=root.

source_root = ${SOLSTICE_BUILD_DIR}/bin
lib_root = ${SOLSTICE_PACKAGE_LIB_DIR}
package_id = com.solstice.sharpon
display_name = Sharpon
version = 0.0.0
maintainer = Solstice
description = Moonwalk and JSON scripting workspace (Sharpon)
deb_depends = libc6, libstdc++6

kind	src	dst	os	base	optional
file	Sharpon.exe		windows	root	no
file	SDL3.dll		windows	root	no
file	SolsticeEngine.dll		windows	root	no
dir	shaders	shaders	windows	root	no
dir	fonts	fonts	windows	root	no
file	Sharpon		linux	root	no
glob	libSDL3.so*		linux	lib	no
glob	libsolsticeengine.so*		linux	lib	no
dir	shaders	shaders	linux	root	no
dir	fonts	fonts	linux	root	no
file	Sharpon		darwin	root	no
glob	libSDL3*.dylib		darwin	lib	no
glob	libsolsticeengine*.dylib		darwin	lib	no
dir	shaders	shaders	darwin	root	no
dir	fonts	fonts	darwin	root	no
