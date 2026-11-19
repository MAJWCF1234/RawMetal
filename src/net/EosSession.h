#pragma once
// The real Session: Epic Online Services (docs/Systems/multiplayer.md). Credentials are baked in at build time
// from src/ThirdParty/EOS/eos_config.json (gitignored); without them, or when the SDK cannot load, the session
// is Offline and says why, and the game plays on alone.
#include "NetTypes.h"
#include <iosfwd>
#include <memory>
namespace retro::net {
std::unique_ptr<Session> makeEosSession();
// --eos-test: sign in, host a lobby, find it in a search, read it by its code, leave (one machine, one user).
bool testEos(std::ostream& report);
}
