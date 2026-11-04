#!/bin/bash
# Runs ReXGlue codegen for Resident Evil 5 using app/re5_manifest.toml.
# Needs your own default.xex in app/ (git-ignored, never committed).
#
#   REXGLUE=/path/to/rexglue tools/codegen.sh app
set -e
RAIZ=$(cd "$(dirname "$0")/.." && pwd)
APP=$RAIZ/${1:-app}
REXGLUE=${REXGLUE:?set REXGLUE to the rexglue executable built from sdk/}
[ -f "$APP/default.xex" ] || { echo "Put your own default.xex in $APP/ first." >&2; exit 1; }
cd "$APP"
echo "== codegen ($APP)"
"$REXGLUE" codegen re5_manifest.toml --log_file codegen.log || { echo "codegen failed, see $APP/codegen.log" >&2; exit 1; }
echo "done"
