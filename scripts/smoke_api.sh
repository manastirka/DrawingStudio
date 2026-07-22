#!/usr/bin/env bash
# Automated command-server smoke (Week-1 safety net).
# Usage:
#   ./scripts/smoke_api.sh [path-to-DrawingStudio]
# Env:
#   DRAWINGSTUDIO_BIN  override binary
#   SMOKE_PORT         default 19100
#   SMOKE_TOKEN        required when reusing an already-running server
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${1:-${DRAWINGSTUDIO_BIN:-$ROOT/build/DrawingStudio}}"
APP_PORT="${SMOKE_PORT:-19100}"
TOKEN="${SMOKE_TOKEN:-}"
LOG="${TMPDIR:-/tmp}/drawingstudio_smoke_$$.log"

if [[ ! -x "$BIN" ]]; then
  echo "error: binary not found or not executable: $BIN" >&2
  echo "build first: cmake --build build -j --target DrawingStudio" >&2
  exit 1
fi

# Free port if a previous smoke left something behind
if command -v lsof >/dev/null 2>&1 \
  && lsof -iTCP:"$APP_PORT" -sTCP:Listen >/dev/null 2>&1; then
  if [[ -z "$TOKEN" ]]; then
    echo "error: SMOKE_TOKEN is required to reuse the server on $APP_PORT" >&2
    exit 1
  fi
  echo "reusing already-running server on $APP_PORT"
  STARTED=0
else
  if [[ -z "$TOKEN" ]]; then
    TOKEN="drawingstudio-smoke-$(uuidgen | tr -d '-')"
  fi
  echo "starting $BIN ..."
  DRAWINGSTUDIO_AUTOMATION_TOKEN="$TOKEN" \
    "$BIN" --enable-automation --automation-port "$APP_PORT" >"$LOG" 2>&1 &
  APP_PID=$!
  STARTED=1
  cleanup() {
    if [[ "$STARTED" -eq 1 ]] && kill -0 "$APP_PID" 2>/dev/null; then
      kill "$APP_PID" 2>/dev/null || true
      sleep 0.5
      kill -9 "$APP_PID" 2>/dev/null || true
    fi
  }
  trap cleanup EXIT

  for i in $(seq 1 30); do
    if curl -sf --max-time 1 -H "Authorization: Bearer $TOKEN" \
      "http://127.0.0.1:${APP_PORT}/api/status" >/dev/null; then
      echo "server up after ${i}s"
      break
    fi
    if ! kill -0 "$APP_PID" 2>/dev/null; then
      echo "error: app exited early; log:" >&2
      tail -50 "$LOG" >&2 || true
      exit 1
    fi
    sleep 1
  done
fi

BASE="http://127.0.0.1:${APP_PORT}"
AUTH_HEADER="Authorization: Bearer $TOKEN"

echo "== status =="
STATUS=$(curl -sf --max-time 3 -H "$AUTH_HEADER" "$BASE/api/status")
echo "$STATUS" | grep -q '"status":"ok"' || {
  echo "status failed: $STATUS" >&2
  exit 1
}
BEFORE=$(echo "$STATUS" | python3 -c "import sys,json; print(json.load(sys.stdin).get('objectCount',0))")

echo "== commands catalog =="
CATALOG=$(curl -sf --max-time 3 -H "$AUTH_HEADER" "$BASE/api/commands")
echo "$CATALOG" | grep -q '"status":"ok"' || {
  echo "commands catalog failed: $CATALOG" >&2
  exit 1
}
echo "$CATALOG" | python3 -c "import sys,json; d=json.load(sys.stdin); assert d.get('commandCount',0)>20; assert 'draw_line' in [c.get('action') for c in d.get('commands',[])]"

echo "== draw_line =="
DRAW=$(curl -sf --max-time 5 -X POST "$BASE/api/command" \
  -H "$AUTH_HEADER" \
  -H 'Content-Type: application/json' \
  -d '{"action":"draw_line","params":{"x1":10,"y1":10,"x2":120,"y2":80,"color":"#00aa00","lineWidth":2}}')
echo "$DRAW" | grep -q '"status":"ok"' || {
  echo "draw_line failed: $DRAW" >&2
  exit 1
}

STATUS2=$(curl -sf --max-time 3 -H "$AUTH_HEADER" "$BASE/api/status")
AFTER=$(echo "$STATUS2" | python3 -c "import sys,json; print(json.load(sys.stdin).get('objectCount',0))")
echo "objectCount $BEFORE -> $AFTER"
if [[ "$AFTER" -lt $((BEFORE + 1)) ]]; then
  echo "error: expected objectCount to increase" >&2
  exit 1
fi

echo "== undo =="
curl -sf --max-time 3 -X POST -H "$AUTH_HEADER" "$BASE/api/undo" >/dev/null

STATUS3=$(curl -sf --max-time 3 -H "$AUTH_HEADER" "$BASE/api/status")
AFTER_UNDO=$(echo "$STATUS3" | python3 -c "import sys,json; print(json.load(sys.stdin).get('objectCount',0))")
echo "objectCount after undo: $AFTER_UNDO"

echo "SMOKE OK"
