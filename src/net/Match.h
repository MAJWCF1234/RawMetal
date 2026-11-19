#pragma once
// The game over a Session (docs/Systems/multiplayer.md): the host's game is the truth. Created by Game when its
// Session enters a lobby, dropped when it leaves. Fills Game::m_remotePlayers and Game::m_matchView every update.
#include "NetTypes.h"
#include <iosfwd>
namespace retro { class Game; struct InputState; }
namespace retro::net {
class Match{
public:
 explicit Match(Game& game);
 // Called from Game::update with the local input, before the local player moves (guests) / after (host).
 void update(const InputState& input,float dt);
private:
 Game& m_game;
};
// --net-test: matches over a LoopbackHub, several Games in one process.
bool testMatch(std::ostream& report);
}
