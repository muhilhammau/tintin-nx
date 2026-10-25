#!/bin/sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
LAUNCHER_DIR="$HERE" PAYLOAD=tintin_nx exec "$HERE/../runtime/launcher/build.sh" "$@"
