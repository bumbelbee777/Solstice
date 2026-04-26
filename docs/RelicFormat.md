# RELIC Format (Resource and Level Index Container)

RELIC is Solstice's native asset packaging format for slow-drive friendliness, sequential streaming, prefetching, and mod/DLC layering.

## File tiers

- **game.data.relic** — Bootstrap manifest. The only hardcoded file; engine looks for it at a fixed path relative to the executable (e.g. `basePath/game.data.relic`). Contains an ordered list of RELIC files to load, per-RELIC priority, streaming hints (preload / stream / lazy), and tag set (base, DLC, mod, locale). Parsed once at startup and never read again.
- **Content / music / level-group RELICs** — Game-specific `.relic` files listed in the bootstrap (e.g. `content.relic`, `music.relic`, `sectionN_levelN.relic`). Same container layout; container type (content vs music) is in the header for optional I/O tuning.

## Bootstrap file layout (game.data.relic)

- Magic (4 bytes): `RELIC_BOOTSTRAP_MAGIC`
- Version (2 bytes)
- Reserved (2 bytes)
- Entry count (4 bytes)
- Per entry:
  - Path length (2 bytes)
  - Path UTF-8 (path length bytes)
  - Priority (4 bytes)
  - Streaming hint (1 byte: preload / stream / lazy)
  - Tag set (2 bytes: base, DLC, mod, locale bits)

## RELIC container layout

- **Header** (fixed size, 68 bytes for format version 1): Magic (`RELI`), format version, container type (content/music), reserved, tag set, reserved, manifest offset/size, dependency table offset/size, data blob offset, path table offset/size (0 when no path table). Legacy containers may use a **52-byte** header only: `ManifestOffset` is **52** and the manifest immediately follows; path table fields are absent (read as zero). New writers use **68** bytes of header and set `ManifestOffset` to **68**.
- **Manifest**: Contiguous array of manifest entries. Each entry ≤ 48 bytes:
  - Asset hash (64-bit)
  - Byte offset into data blob (64-bit)
  - Compressed size (32)
  - Uncompressed size (32)
  - Asset type tag (16: mesh, texture, material, audio, script, lightmap, etc.)
  - Flags (16: is_delta, is_base, compression type, streaming priority)
  - Dependency list offset (32) — byte offset into dependency table
  - Cluster ID (32) — for prefetch grouping
- **Dependency table**: Contiguous blob. Per-asset list at `DependencyListOffset`: uint32_t count, then count × uint64_t asset hashes.
- **Path table** (optional): At `PathTableOffset` with byte size `PathTableSize`. Layout: `uint32_t entryCount`, then per entry `uint16_t pathLen`, UTF-8 path (`pathLen` bytes), `uint64_t assetHash`. Placed after the dependency table and before the data blob in newly written files.
- **Data blob**: Starts at data blob offset. Assets stored in cluster order. Compression per entry (flags): none, LZ4, zstd.

## Compression

- **LZ4** — Streaming assets; in-house implementation in `Core/System/LZ4.cxx`. Block format (no frame).
- **Zstd** — One-shot assets (UI, global audio, cutscenes); external lib (compress and decompress in tooling; runtime may decompress only).

## Delta / variant assets

- Delta entries have `is_delta` set; base asset hash in dependency list. Delta payload: algorithm (1), output size (4), then algorithm-specific bytes. Supported algorithm: ChunkedXOR (output = base XOR delta, same length).

## Virtual table and layering

- At startup, the engine opens each RELIC listed in the bootstrap and merges manifests. Same asset hash in multiple RELICs: later/higher-priority RELIC wins (mod/DLC override). The result is a single in-memory virtual table; all lookups are by 64-bit asset hash with no disk seeks.

## Path table (optional)

- For path-based API compatibility (`LoadGLTF(path)`), an optional path table can map logical path string → asset hash. If present, the loader resolves path to hash and loads from RELIC; if absent, only hash-based loading is used.

## Script bindings

- `Assets.Prefetch(clusterId)` — Prefetch all assets in the cluster (and dependencies).
- `Assets.IsLoaded(assetHash)` — Returns whether the asset is in the cache.
- `Assets.Unload(clusterId)` — Evict all assets in the cluster from the cache.

## Naming

- All containers use the `.relic` extension. Only `game.data.relic` is hardcoded; other filenames are declared in the bootstrap.
