#!/usr/bin/env bash
# Idempotent bootstrap for the RawMetal repository on Linux Cloud Agents.
#
# The RawMetal game itself is a Windows-only target (MSVC + Win32 + Vulkan SDK)
# and cannot be built or run here. The component that IS cross-platform is the
# Depthworks Level Editor: a pure Python 3 standard-library web server that
# serves an HTML/Three.js editor and reads assets straight from src/assets.
#
# There are no Python package dependencies. The only cacheable dependency is
# the pinned Three.js runtime the editor fetches on first use; warming it here
# keeps the 3D preview working even if network egress is later restricted, and
# makes the terminal server start instantly.
set -euo pipefail

cd "$(dirname "$0")/.."

echo "[install] python3: $(python3 --version)"

# Warm the pinned Three.js vendor cache used by the Level Editor's 3D preview.
# Importing server.py only defines constants/functions (main() is guarded), so
# this is safe and idempotent: ensure_vendor() skips files already cached.
python3 - <<'PY'
import sys
sys.path.insert(0, "tools/level-editor")
import server
server.ensure_vendor()
ready = (server.VENDOR_ROOT / "three.module.js").exists()
print(f"[install] Three.js vendor cache ready: {ready}")
PY

echo "[install] done"
