#!/usr/bin/env bash
# Local/CI build. Usage: OO_PS4_TOOLCHAIN=/path/to/OpenOrbis ./build.sh
set -euo pipefail
: "${OO_PS4_TOOLCHAIN:?Set OO_PS4_TOOLCHAIN to the extracted OpenOrbis toolchain folder}"
make clean
make
echo "Built:"; ls -lh out/*.pkg
