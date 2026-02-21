# 在 Git Bash 中 source 此脚本后，可使用 cmake（若已用 winget 安装）
# 用法: source env_cmake.sh  或  . env_cmake.sh
if [ -d "/c/Program Files/CMake/bin" ]; then
  export PATH="/c/Program Files/CMake/bin:$PATH"
  echo "CMake added to PATH (Git Bash)."
elif [ -d "/c/Program Files (x86)/CMake/bin" ]; then
  export PATH="/c/Program Files (x86)/CMake/bin:$PATH"
  echo "CMake added to PATH (Git Bash)."
else
  echo "CMake bin not found. Install from https://cmake.org/download/ or: winget install Kitware.CMake"
fi
