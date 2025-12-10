# bfetch

A lightweight, highly optimized system fetch tool written in C.

`bfetch` is designed for speed and minimal resource usage. It performs direct filesystem access to gather system information without spawning external subprocesses, making it significantly faster than standard shell scripts or even other C implementations.

## Features

- **Extreme Performance**: Execution time is typically under 2ms.
- **Direct Filesystem Access**: Reads `/proc` and `/sys` directly; no `popen()` or `exec()`.
- **System Support**: Native support for **Bedrock Linux**, **CachyOS**, and **Gentoo**.
- **Accurate Detection**: 
  - Correct terminal emulator detection (not just shell).
  - Detailed GPU information (Vendor, Model, Type).
  - Package counts for multiple package managers (RPM, DPKG, Pacman, Emerge, Nix).
- **Aesthetic**: Custom ASCII art with Nord color palette.

## Usage

```bash
# Auto-detect system and run
./bfetch

# Force specific ASCII art/mode
./bfetch --cachyos   # CachyOS mode (Teal/Gray theme)
./bfetch --gentoo    # Gentoo mode (Purple/White theme)
./bfetch --bedrock   # Bedrock Linux mode (Default)
```

## Comparisons

| Tool | Approx. Time | Approach |
|------|-------------|----------|
| **bfetch** | **~0.002s** | Direct C syscalls |
| fastfetch | ~0.01-0.10s | Optimized C |
| neofetch | ~0.20-1.00s | Shell script |

## Installation

### From Source

```bash
git clone https://github.com/Mjoyufull/bfetch.git
cd bfetch

# Standard build (GCC with optimizations)
make

# Run
./bfetch
```

### Nix

```bash
nix run github:Mjoyufull/bfetch
```

## License

MIT License
