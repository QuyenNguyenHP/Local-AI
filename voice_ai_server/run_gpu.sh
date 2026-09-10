#!/usr/bin/env bash
set -euo pipefail
server_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
export WHISPER_DEVICE=cuda
export WHISPER_COMPUTE_TYPE="${WHISPER_COMPUTE_TYPE:-float16}"
exec "$server_dir/../.venv/bin/python" "$server_dir/run.py"
