#!/bin/zsh
set -e
BASE="${TMPDIR:-/tmp}/clockosv1-setup"
mkdir -p "$BASE"
INSTALLER="$BASE/install.py"
URL="https://raw.githubusercontent.com/n0rm0/Clock/main/.source/install/install.py"
printf '\nClockOSV1 Setup\nDownloading the latest installer...\n'
curl -fL --retry 3 "$URL" -o "$INSTALLER"
exec python3 "$INSTALLER"
