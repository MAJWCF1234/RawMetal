# v0.6.6 — Eye-Level Carry Fix

Carried objects were capped at 0.48 metres above the player's feet, keeping
items near knee height. Their centre now follows the camera's aim 0.9 metres
from the current eye position, including standing, crouching and looking
up or down. Ground support, obstruction checks, dropping and punting remain
active. Rendering quality and the v0.6.5 performance improvements are retained.

Regression checks cover all seven carryable shapes in both stances at three
aim angles, rotated object bounds, shelf placement/retrieval, seam transfer,
throw damage, gravity and settling. The Vulkan smoke suite and Windows binary
audit passed; all 166 embedded assets remain lossless and the executable is
below 22,000,000 bytes. No save-format or map changes are required.

Windows 10/11 x64 requires the Visual C++ x64 runtime 14.44 or later and a
Vulkan-capable graphics driver. Extract RawMetal.exe beside existing saves,
or run the replacement executable in N:\rawmetal.
