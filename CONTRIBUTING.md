# Contributing to Solstice

Thank you for your interest in contributing to Solstice! This document provides guidelines and information for contributors.

## Getting Started

### Prerequisites

- **CMake** 3.20 or higher
- **C++20** compatible compiler (MSVC 2019+, GCC 10+, Clang 12+)
- **Python 3** (for shader compilation)
- **Git** with submodule support

### Cloning the Repository

```bash
# Clone with all submodules
git clone --recursive https://github.com/bumbelbee777/Solstice.git
cd Solstice

# If you already cloned without --recursive
git submodule update --init --recursive
```

### Building

```bash
# Configure with CMake
cmake --preset default

# Build
cmake --build out/build/default

# Or use your preferred IDE (Visual Studio, CLion, VS Code with CMake Tools)
```

## Development Guidelines

### Code Style

- Use **4 spaces** for indentation (no tabs)
- PascalCase naming for file, function, and variable names
- Follow the existing code style in the project
- Use `.hxx` for C++ headers and `.cxx` for C++ source files
- Use meaningful variable and function names
- Comment complex logic or any important notes

### Commit Messages

Follow conventional commit format:

```
[<type>:<scope>] <description>

[optional body]

[optional footer]
```

Types:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes (formatting, etc.)
- `refactor`: Code refactoring
- `perf`: Performance improvements
- `test`: Adding or updating tests
- `build`: Build system changes
- `ci`: CI/CD changes
- `chore`: Maintenance tasks

Example:
```
[feat:physics] add continuous collision detection for convex hulls

Implements CCD using GJK algorithm with support for sphere, box,
and convex hull colliders.

Closes #123
```

### Pull Request Process

1. Fork the repository
2. Create a feature branch from `main`
3. Make your changes
4. Ensure the project builds without errors
5. Test your changes
6. Submit a pull request

## Project Structure

```
Solstice/
├── source/           # Engine source code
│   ├── Core/         # Core utilities (debug logger, allocators, job system audio, etc.)
│   ├── Entity/       # Entity component system
│   ├── Math/         # Math library (quaternions, vectors)
│   ├── Physics/      # Physics subsystem
│   ├── Render/       # Rendering subsystem
│   ├── Scripting/    # Moonwalk scripting support
│   ├── Shaders/      # BGFX shaders
│   └── UI/           # UI subsystem
├── example/          # Example projects
├── 3rdparty/         # Third-party dependencies
├── tools/            # Build tools and scripts
└── CMakeLists.txt    # Main CMake configuration
```

## Reporting Issues

When reporting issues, please include:

1. **Description**: Clear description of the issue
2. **Steps to Reproduce**: How to reproduce the problem
3. **Expected Behavior**: What you expected to happen
4. **Actual Behavior**: What actually happened
5. **Environment**: OS, compiler version, CMake version
6. **Screenshots/Logs**: If applicable

## License

By contributing to Solstice, you agree that your contributions will be licensed under the same license as the project.

## Questions?

If you have questions, feel free to:
- Open a GitHub issue
- Start a discussion in the Discussions tab
