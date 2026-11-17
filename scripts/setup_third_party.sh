#!/usr/bin/env bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$REPO_ROOT/third_party"
if [ ! -d "$REPO_ROOT/third_party/snesrecomp" ]; then
    git clone --recursive https://github.com/sp00nznet/snesrecomp.git "$REPO_ROOT/third_party/snesrecomp"
else
    echo "third_party/snesrecomp already exists."
fi
