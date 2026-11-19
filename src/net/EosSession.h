#pragma once
// The real Session: Epic Online Services (docs/Systems/multiplayer.md). Credentials are baked in at build time
// from src/ThirdParty/EOS/eos_config.json (gitignored); without them, or when the SDK cannot load, the session
// is Offline and says why, and the game plays on alone.
//  - Sign-in: EOS Connect with this PC's Device ID (no Epic account), named after the Windows user.
//  - The SDK DLL is packed in the exe and unpacked to %LOCALAPPDATA%\RawMetal\eos\<hash>\ on the first tick;
//    construction is cheap, and everything after runs asynchronously from tick(). Log: RawMetal-eos.txt beside
//    the exe.
//  - Lobbies carry mode, map, maphash, name, joincode, private, players and version attributes; every lobby is
//    advertised so a code search finds a private one, and the browser filters private=1 out.
//  - P2P on socket "DepthWorks": reliable is ReliableOrdered, else UnreliableUnordered. Every packet carries a
//    one-byte header, so an unreliable message holds at most EosUnreliableMax bytes (a bigger one is refused);
//    a reliable one up to EosReliableMax is cut into EOS-sized fragments and arrives whole.
#include "NetTypes.h"
#include <cstddef>
#include <iosfwd>
#include <memory>
namespace retro::net {
inline constexpr std::size_t EosPacketMax=1170;                 // EOS_P2P_MAX_PACKET_SIZE
inline constexpr std::size_t EosUnreliableMax=EosPacketMax-1;   // one header byte
inline constexpr std::size_t EosReliableMax=64*1024;
std::unique_ptr<Session> makeEosSession();
// --eos-test: sign in, host a lobby, find it in a search, read it by its code, leave (one machine, one user).
bool testEos(std::ostream& report);
}
