#include "Match.h"
#include "../game/Game.h"
#include <ostream>
namespace retro::net {
// Placeholder until the match layer lands.
Match::Match(Game& game):m_game(game){}
void Match::update(const InputState&,float){}
bool testMatch(std::ostream& report){report<<"FAIL the match layer is not built yet\n";return false;}
}
