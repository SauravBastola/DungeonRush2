#!/usr/bin/env bash
# Build DungeonRush (binary: bin/Prelude).
#   ./build.sh          configure + build (Debug, as in the original project)
#   ./build.sh run      build, then start the game
#   ./build.sh clean    remove build/ and bin/
set -euo pipefail
cd "$(dirname "$0")"

case "${1:-}" in
  clean)
    rm -rf build bin lib
    echo "Cleaned."
    exit 0
    ;;
esac

cmake -S . -B build
cmake --build build -j "$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"
echo "Built bin/Prelude"

if [[ "${1:-}" == "run" ]]; then
  # The game loads assets from ../assets, so it must start inside bin/.
  cd bin && exec ./Prelude
fi
