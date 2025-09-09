# bfetch

> **The fastest system information fetch tool in the universe**

A lightning-fast system information display tool written in pure C, specifically optimized for Bedrock Linux. bfetch is **675x faster** than traditional shell scripts and **57x faster** than fastfetch, executing in just **0.002 seconds**.

## Performance Comparison

| Tool | Execution Time | Speed Comparison |
|------|---------------|------------------|
| **bfetch** | **0.002s** | **Baseline** |
| fastfetch | 0.115s | 57x slower |
| Original shell script | 1.35s | 675x slower |

## Features

- **Blazing fast** - 0.002 second execution time
- **Beautiful ASCII art** with Nord color palette
- **Bedrock Linux optimized** - Multi-strata package counting
- **Zero subprocess spawning** - Direct filesystem access only  
- **Perfect visual match** to the original shell script
- **Aggressive optimizations** - Native CPU instructions, LTO
- **Tiny binary** - Only 15KB stripped

## Visual Output

```
 ┌──┐ ┌──────────────────────────────────┐ ┌────┐
 │▒▒│ │─\\\\\\\\\\\\\\\─────────────────────│ │ 境 │
 │██│ │──\\\      \\\────────────────────│ │    │
 │██│ │───\\\      \\\───────────────────│ │ 界 │
 │██│ │────\\\      \\\\\\\\\\\\\\\\\\\────│ └────┘
 │██│ │─────\\\                    \\\───│
 │██│ │──────\\\                    \\\──│
 │██│ │───────\\\        ──────      \\\─│
 │██│ │────────\\\                   ///─│
 │██│ │─────────\\\                 ///──│
 │██│ │──────────\\\               ///───│
 │██│ │───────────\\\////////////////────│
 │██│ └──────────────────────────────────┘
 │██│ Version: 0.7.31beta2 Poki
 │██│ Kernel: 6.16.4-200.fc42.x86_64
 │██│ Uptime: 2 days, 39 minutes
 │██│ WM: Hyprland
 │██│ Packages: 40 (nix),3076 (rpm),284 (emerge),557 (pacman)
 │██│ Terminal: zsh
 │██│ Memory: 15Gi / 31Gi
 │██│ Shell: zsh
 │██│ CPU: AMD Ryzen 5 3600 6-Core Processor
 │▒▒│ GPU: NVIDIA Corporation GA104 [GeForce RTX 3070 Ti] (rev a1)
 └──┘
```

## Installation

### Using Nix (Recommended)

```bash
# Run directly
nix run github:Mjoyufull/bfetch

# Install to profile
nix profile install github:Mjoyufull/bfetch

# Add to NixOS configuration
environment.systemPackages = [
  inputs.bfetch.packages.${system}.default
];
```

### Build from Source

```bash
git clone https://github.com/Mjoyufull/bfetch.git
cd bfetch

# Build with maximum optimizations
make fast

# Run it
./bfetch

# Install system-wide (optional)
make install
```

### Development

```bash
# Enter development environment
nix develop

# Build and test
make fast && ./bfetch
```

## Optimization Techniques

bfetch achieves its incredible speed through several key optimizations:

1. **Zero subprocess spawning** - No `popen()` calls, no external commands
2. **Direct filesystem access** - Reads from `/proc` and `/sys` only
3. **Hardcoded static data** - CPU, GPU, and other unchanging info
4. **Aggressive compiler flags** - `-Ofast`, `-march=native`, `-flto`
5. **Minimal memory operations** - Stack-allocated structs only
6. **Inline functions** - Reduces function call overhead

## System Information Displayed

- **Version** - Bedrock Linux version
- **Kernel** - Linux kernel version  
- **Uptime** - System uptime in human-readable format
- **Window Manager** - Currently running WM/DE
- **Packages** - Package counts across all Bedrock strata
- **Terminal** - Current terminal emulator
- **Memory** - RAM usage (used/total)
- **Shell** - Current shell
- **CPU** - Processor model
- **GPU** - Graphics card information

## Bedrock Linux Support

bfetch automatically detects and counts packages from multiple Bedrock Linux strata:

- **fedora** - RPM packages
- **gentoo** - Portage packages  
- **tut-arch** - Pacman packages
- **bedrock** - Nix packages

## Build Requirements

- GCC compiler
- GNU Make
- Linux system (tested on Fedora with Bedrock Linux)

## Compiler Flags

The `make fast` target uses aggressive optimization flags:

```bash
-Ofast -march=native -mtune=native -flto -funroll-loops 
-finline-functions -ffast-math -fomit-frame-pointer 
-fno-stack-protector -fno-unwind-tables 
-fno-asynchronous-unwind-tables -DNDEBUG -s
```

## License

MIT License - see [LICENSE](LICENSE) file for details.

## Contributing

This project is specifically optimized for the author's Bedrock Linux system. For general-purpose system fetch tools, consider using [fastfetch](https://github.com/fastfetch-cli/fastfetch) or [neofetch](https://github.com/dylanaraps/neofetch).

However, contributions for additional optimizations or Bedrock Linux improvements are welcome!

## Acknowledgments

- Inspired by the original shell script that this C version replaces
- Built for Bedrock Linux systems
- Optimized with Nord color theme
- Japanese characters (境 界) for aesthetic appeal

---

**bfetch - When you need system information at the speed of light!**
