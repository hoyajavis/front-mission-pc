#!/usr/bin/env bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
mkdir -p "$REPO_ROOT/.git/hooks"
cp "$REPO_ROOT/tools/git-hooks/pre-commit" "$REPO_ROOT/.git/hooks/pre-commit"
chmod +x "$REPO_ROOT/.git/hooks/pre-commit"
echo "Installed git pre-commit hook."
