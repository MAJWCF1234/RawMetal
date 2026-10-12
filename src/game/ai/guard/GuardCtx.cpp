#include "GuardCtx.h"
#include "../../Game.h"
namespace retro {
int GuardCtx::get(std::string_view field) const { return game.state(game.friendlyKey(actor, field)); }
void GuardCtx::put(std::string_view field, int value) { game.setState(game.friendlyKey(actor, field), value); }
}
