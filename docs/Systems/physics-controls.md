# Loose props and water impacts

Desktop carry controls:

- Press E to pick up or put down a carryable prop.
- Hold the middle mouse button and move the mouse to turn the held prop horizontally or vertically. The camera stays still during this gesture.
- Scroll while carrying to move the prop farther away or closer. Reach is limited by the prop's size, a maximum distance of 2.5 m, and world collision.
- Left click throws the held prop. Releasing with E while spinning or flicking the look also flings it along that path (a hard upward flick sends it up). Soft put-downs keep almost no speed. Empty-handed mouse wheel still selects weapons. VR carrying continues to use the tracked hand velocity.

Moving modeled props of at least 8 kg can push the player, guards and creatures — including while you are carrying them (walking forward, extending reach, or turning the prop in place). A body pinned against a wall stops the held prop instead of letting it pass through. Walking actors can still shove resting props aside, but their walk animation cannot reverse an incoming heavy object before contact. Existing wall and door collision limits body movement.

Impact damage follows the object's kinetic energy using body-relative closing speed: heavier objects hurt more at equal speed, and faster objects hurt more at equal weight. Slow contact is harmless, damage is capped at 80 per impact, and resting overlap does not cause continuous damage. Falling props sweep through the head-contact plane instead of relying on one overlap sample, so high frame rates and fast falls do not skip the hit. A shelf or other support stops the prop before it can hurt a body below it.

Firearm rounds entering standing water throw droplets above the surface. A bridge, wall or target in front of the water blocks the splash. After crossing the surface, a round can still hit an underwater target.

The Starter Map pools use a wider spray of larger drops that stays visible around shotgun recoil. The mist lasts about 0.7 seconds. The positional splash has a 24 m sound radius and a 0.58-second sample with a wet tail that follows the gunshot. Native 640×360 Vulkan captures on 2026-11-16 show the effect in the Welcome basin and in both Light pools, including the murky one. The gameplay mixer also registered a shotgun water strike from 10 m.

Console: `give 12 GA`, `give 12 gauge`, `give 12ga`, or `give shells` adds 16 shells to the shotgun's reserve without loading the tube or changing the equipped weapon.

Verified locally on 2026-11-15: 66 game engine checks, five selected editor-engine checks, native editor self/editing tests, rendered water frames, and a focused real-mixer splash check passed. The separate full audio stress run hit its runner timeout and is not counted as passed. Both game and editor binaries were rebuilt from the synchronized working sources.
