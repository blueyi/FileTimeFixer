# After sourcing this script in Git Bash, cmake is available (if installed e.g. via winget)
# Usage: source env_cmake.sh   or  . env_cmake.sh
if [ -d "/c/Program Files/CMake/bin" ]; then
  export PATH="/c/Program Files/CMake/bin:$PATH"
  echo "CMake added to PATH (Git Bash)."
elif [ -d "/c/Program Files (x86)/CMake/bin" ]; then
  export PATH="/c/Program Files (x86)/CMake/bin:$PATH"
  echo "CMake added to PATH (Git Bash)."
else
  echo "CMake bin not found. Install from https://cmake.org/download/ or: winget install Kitware.CMake"
fi
