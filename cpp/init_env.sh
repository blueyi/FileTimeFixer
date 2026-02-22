#!/usr/bin/env bash
# Environment init for FileTimeFixer C++ build (Linux / macOS):
#   Installs Exiv2, FFmpeg, and build tools via system package manager.
# Run from repo root or cpp/:  ./cpp/init_env.sh  or  source cpp/init_env.sh
# Requires: sudo (Linux) or non-root for Homebrew (macOS)

set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
CPP_DIR="$(cd "$SCRIPT_DIR" && pwd)"
REPO_ROOT="$(cd "$CPP_DIR/.." && pwd)"

echo "=== FileTimeFixer env init (Linux/macOS) ==="

install_linux() {
  if command -v apt-get &>/dev/null; then
    echo "Using apt (Debian/Ubuntu). Installing exiv2, ffmpeg, cmake, build-essential..."
    sudo apt-get update -qq
    sudo apt-get install -y libexiv2-dev ffmpeg cmake build-essential
  elif command -v dnf &>/dev/null; then
    echo "Using dnf (Fedora/RHEL). Installing exiv2, ffmpeg, cmake..."
    sudo dnf install -y exiv2-devel ffmpeg ffmpeg-devel cmake gcc-c++
  elif command -v pacman &>/dev/null; then
    echo "Using pacman (Arch). Installing exiv2, ffmpeg, cmake..."
    sudo pacman -Sy --noconfirm exiv2 ffmpeg cmake base-devel
  else
    echo "Unknown Linux package manager. Install manually:"
    echo "  libexiv2-dev (or exiv2-devel), ffmpeg, cmake, build-essential/gcc-c++"
    exit 1
  fi
}

install_macos() {
  if ! command -v brew &>/dev/null; then
    echo "Homebrew not found. Install from https://brew.sh then re-run this script."
    exit 1
  fi
  echo "Using Homebrew. Installing exiv2, ffmpeg, cmake..."
  brew install exiv2 ffmpeg cmake
}

case "$(uname -s)" in
  Linux*)   install_linux ;;
  Darwin*)  install_macos ;;
  *)
    echo "Unsupported OS: $(uname -s). Install Exiv2, FFmpeg, and CMake manually."
    exit 1
    ;;
esac

echo ""
echo "Next steps:"
echo "  cd $CPP_DIR/build"
echo "  cmake .."
echo "  cmake --build ."
echo "=== Done ==="
