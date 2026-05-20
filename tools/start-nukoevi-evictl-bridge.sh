#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
remote_evictl_default="/Users/${USER}/.local/share/mise/installs/bun/1.3.13/bin/bun run /Users/${USER}/ghq/github.com/schroneko/evictl/src/cli.ts"

: "${NUKOEVI_EVI_BRIDGE_HOST:=0.0.0.0}"
: "${NUKOEVI_EVI_BRIDGE_PORT:=18787}"
: "${EVICTL_TARGET:=nukoevi}"
: "${EVICTL_REMOTE_HOST:=mymacstudio}"
: "${EVICTL_REMOTE_EVICTL:=${remote_evictl_default}}"

exec python3 "${repo_dir}/tools/nukoevi-evictl-bridge.py" \
  --host "${NUKOEVI_EVI_BRIDGE_HOST}" \
  --port "${NUKOEVI_EVI_BRIDGE_PORT}" \
  --target "${EVICTL_TARGET}" \
  --remote-host "${EVICTL_REMOTE_HOST}" \
  --remote-evictl "${EVICTL_REMOTE_EVICTL}"
