#!/usr/bin/env bash
# Local/CI build. Usage: OO_PS4_TOOLCHAIN=/path/to/OpenOrbis ./build.sh
set -euo pipefail
: "${OO_PS4_TOOLCHAIN:?Set OO_PS4_TOOLCHAIN to the extracted OpenOrbis toolchain folder}"
# PkgTool.Core is a .NET app; run without ICU if it is not installed
export DOTNET_SYSTEM_GLOBALIZATION_INVARIANT=1
make clean
make
echo "Built:"; ls -lh out/*.pkg
