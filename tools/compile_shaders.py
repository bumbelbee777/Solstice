#!/usr/bin/env python3
"""
Cross-platform BGFX shader compiler for Solstice.
Compiles .sc shader files to platform-specific binaries.
"""

import sys
import subprocess
import argparse
import platform
from pathlib import Path


def get_platform_info():
    """Detect current platform and return (platform_name, profile, exe_suffix)."""
    system = platform.system().lower()
    if system == "windows":
        return "windows", "s_5_0", ".exe"
    elif system == "darwin":
        return "osx", "metal", ""
    elif system == "linux":
        return "linux", "440", ""
    else:
        return "linux", "440", ""  # Default to Linux


def find_executable(name: str, start_dir: Path) -> Path | None:
    """Recursively search for an executable named `name` under start_dir."""
    for root, dirs, files in start_dir.walk():
        if name in files:
            return root / name
    return None


def find_shaderc(args, exe_suffix: str) -> Path | None:
    """Find shaderc executable, checking args first, then default locations."""
    # If explicitly provided via args
    if args.shaderc:
        shaderc_path = Path(args.shaderc)
        if shaderc_path.exists():
            return shaderc_path
        print(f"WARNING: Specified shaderc not found: {shaderc_path}")

    exe_name = f"shaderc{exe_suffix}"
    
    # Default locations to check (relative to script location or CWD)
    script_dir = Path(__file__).parent.resolve()
    project_root = script_dir.parent
    
    default_paths = [
        project_root / "out" / "build" / "3rdparty" / "cmake" / "bgfx" / "Release" / exe_name,
        project_root / "out" / "build" / "3rdparty" / "cmake" / "bgfx" / "Debug" / exe_name,
        project_root / "build" / "3rdparty" / "cmake" / "bgfx" / "Release" / exe_name,
        project_root / "build" / "3rdparty" / "cmake" / "bgfx" / "Debug" / exe_name,
    ]
    
    for path in default_paths:
        if path.exists():
            return path
    
    # Search recursively as fallback
    print(f"Searching for {exe_name}...")
    found = find_executable(exe_name, project_root)
    return found


def compile_shaders(args):
    """Main shader compilation logic."""
    print("========================================")
    print("Compiling BGFX Shaders for Solstice")
    print("========================================\n")

    # Detect platform
    detected_platform, detected_profile, exe_suffix = get_platform_info()
    target_platform = args.platform or detected_platform
    target_profile = args.profile or detected_profile
    
    print(f"Target platform: {target_platform}")
    print(f"Shader profile: {target_profile}\n")

    # Find shaderc
    shaderc = find_shaderc(args, exe_suffix)
    if not shaderc:
        print(f"ERROR: shaderc{exe_suffix} not found!")
        print("Please build the project first to generate the shaderc tool,")
        print("or specify it with --shaderc <path>")
        return 1

    print(f"Using shaderc: {shaderc}\n")

    # Resolve directories
    script_dir = Path(__file__).parent.resolve()
    project_root = script_dir.parent
    
    shader_dir = Path(args.input_dir) if args.input_dir else project_root / "source" / "Shaders"
    output_dir = Path(args.output_dir) if args.output_dir else shader_dir / "bin"
    bgfx_include = Path(args.bgfx_include) if args.bgfx_include else project_root / "3rdparty" / "bgfx" / "src"
    
    if not shader_dir.exists():
        print(f"ERROR: Shader directory does not exist: {shader_dir}")
        return 1

    output_dir.mkdir(parents=True, exist_ok=True)

    print(f"Shader source: {shader_dir}")
    print(f"Output directory: {output_dir}")
    print(f"BGFX include: {bgfx_include}\n")

    # Find and compile shaders
    sc_files = sorted(shader_dir.glob("*.sc"))
    if not sc_files:
        print("No .sc shader files found.")
        return 0

    compiled_count = 0
    for sc_file in sc_files:
        stem = sc_file.stem
        stem_lower = stem.lower()

        # Skip varying definition files
        if "varying" in stem_lower:
            print(f"Skipping {sc_file.name} (varying definition)")
            continue

        # Determine shader type from prefix
        shader_type = None
        if stem_lower.startswith("vs_"):
            shader_type = "vertex"
        elif stem_lower.startswith("fs_"):
            shader_type = "fragment"
        elif stem_lower.startswith("cs_"):
            shader_type = "compute"

        if not shader_type:
            print(f"Skipping {sc_file.name} (unknown shader type)")
            continue

        # Determine output file and varying definition
        out_file = output_dir / f"{stem}.bin"
        varying_def = shader_dir / "varying.def.sc"
        if "_imgui" in stem_lower:
            varying_def = shader_dir / "varying_imgui.def.sc"
        elif "_ui" in stem_lower:
            varying_def = shader_dir / "varying_ui.def.sc"

        if not varying_def.exists():
            print(f"WARNING: Varying definition not found: {varying_def}")
            varying_def = shader_dir / "varying.def.sc"

        print(f"Compiling {sc_file.name} -> {out_file.name} ({shader_type})")

        # Build command
        cmd = [
            str(shaderc),
            "-f", str(sc_file),
            "-o", str(out_file),
            "--type", shader_type,
            "--platform", target_platform,
            "--profile", target_profile,
            "-i", str(bgfx_include),
            "--varyingdef", str(varying_def),
        ]

        try:
            proc = subprocess.run(cmd, capture_output=True, text=True)
        except OSError as e:
            print(f"ERROR: Failed to run shaderc: {e}")
            return 1

        if proc.returncode != 0:
            print(f"\nERROR: Failed to compile {sc_file.name}")
            if proc.stdout:
                print(proc.stdout)
            if proc.stderr:
                print(proc.stderr, file=sys.stderr)
            return 1

        compiled_count += 1

    print(f"\n========================================")
    print(f"Shader compilation complete!")
    print(f"Compiled {compiled_count} shader(s) to {output_dir}")
    print("========================================\n")
    return 0


def main():
    parser = argparse.ArgumentParser(
        description="Compile BGFX shaders for Solstice engine",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s                              # Auto-detect everything
  %(prog)s --platform linux --profile 440
  %(prog)s --shaderc /path/to/shaderc --input-dir ./Shaders
        """
    )
    parser.add_argument(
        "--shaderc",
        help="Path to shaderc executable"
    )
    parser.add_argument(
        "--input-dir",
        help="Directory containing .sc shader source files"
    )
    parser.add_argument(
        "--output-dir",
        help="Directory for compiled shader binaries"
    )
    parser.add_argument(
        "--bgfx-include",
        help="BGFX include directory for shader compilation"
    )
    parser.add_argument(
        "--platform",
        choices=["windows", "linux", "osx", "android", "ios"],
        help="Target platform (default: auto-detect)"
    )
    parser.add_argument(
        "--profile",
        help="Shader profile (e.g., s_5_0, metal, 440)"
    )

    args = parser.parse_args()
    sys.exit(compile_shaders(args))


if __name__ == "__main__":
    main()
