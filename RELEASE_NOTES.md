# RawMetal Release Notes

Changelog of shipped updates. Newest first. Each section is one release.

---

## v1.6.5 - Leon (2026-11-20)

A third body to go down in.

- **Leon.** A white cat in a black bow and black stockings, for anyone who would rather not be Claire or the Limitator. CHARACTER, on the multiplayer page and in settings, now holds three cards. He is who you are in first person, in third person and in VR, and who the others see in a match; a player on an older release sees the Limitator in his place. He runs, kneels, aims and falls on Claire's own animations.
- **Credit where it is due.** Leon's model is based on "Boykisser Custom Low Poly Avatar (Scratch Model)" by NEGATIVEMARE (https://skfb.ly/pO6ZD), licensed under CC BY 4.0 (http://creativecommons.org/licenses/by/4.0/). The three meshes were joined and reduced, the rig was mapped onto the game's skeleton, the materials became flat colours, and he is animated with the game's own animations. The author did not make, review or endorse DepthWorks. The same credit is in CREDITS.txt in each zip.

Verified with the match, character, settings, multiplayer menu and third-person tests, the Campaign Builder's full editing test, and tools/release/ship.py.

---

## v1.6.4 - Bare hands (2026-11-19)

Nobody comes down armed.

- **Deathmatch begins with fists.** A player starts, and starts again after a fall, with no gun, no rounds and nothing left from an earlier life. Every gun is found lying on the map: the handgun, the shotgun or the MAC-11, as the map's maker has set them down. In co-op you still carry the shotgun, as before.
- **A gun picked up is a gun in the hand.** The shotgun comes loaded with a box of shells, the handgun with a magazine, and either is drawn as it is taken.
- **What is taken is taken for everyone, and comes back.** In a deathmatch a pickup one player takes leaves every player's map, and it lies there again thirty seconds later, so the guns never run out.
- **The Builder says so.** The Multiplayer page warns when a deathmatch map has no gun on it, so a map is never quietly fists only.
- **One release for all.** The way players talk to each other changed; everyone in a match needs this release.

Verified with the match, map format and multiplayer menu tests, the Campaign Builder's full editing test, and tools/release/ship.py.

---

## v1.6.3 - Something to pick up (2026-11-19)

Supplies and guns, on any floor.

- **Upstairs, it stays upstairs.** A health box, a box of shells or a gun left on an upper floor in the Campaign Builder used to be saved without a height and turned up on the ground floor beneath it. It is saved with its floor's height now, and rests where it was set down.
- **More to leave lying about.** The Health and ammo tool also offers the handgun and the shotgun, beside the MAC-11. The handgun comes with a magazine and is drawn if it is new to you. You always carry the shotgun, so its drop is a full box of shells, and the gun comes to hand if yours were empty. Each lies on the floor at its own length. Older saves load as they did.

Verified with the map format, saves, settings and MAC-11 tests, the Campaign Builder's full editing test, and tools/release/ship.py.

---

## v1.6.2 - A forgiving Builder (2026-11-19)

The Builder should never be the reason a map is lost.

- **Numbers stay numbers.** In the Campaign Builder, a box that takes a number now keeps its last good value if something that is not a number is typed, and says so. Before, the Builder would accept it and save it, and the game would then refuse the whole map. The Builder is what let that through, and it does not any more.
- **Saved, then checked.** Saving always saves. Then the Builder asks the game whether it can open the map, and if it can't, says why in plain words and picks out the piece at fault.
- **The archive names what it cannot open.** A map in custom maps that will not load is listed with its reason, instead of simply being absent.
- **Names with spaces.** A map whose file name has a space in it opens and checks like any other.
- **Joints keep their manners.** A hinge's friction, a drawer's catch and a latch's hold now survive the Builder's export.
- **Fists count.** In a match, a punch lands on another player the way a shot does, judged by the host.
- **Cloth remembers.** Curtains, straps and cables keep how they hang across a save.
- **A stiff hinge opens.** A hinge or slider set stiffer than 4 made the whole map refuse to load, because its friction was read as a prop's. It is read as its own now, up to 5000.
- **More things hold their shape.** Thirty-six of the newer props, rocks and dressing among them, now collide as the shapes they are and not as fat boxes.

Verified with the map format, settings, multiplayer, match, saves, cloth, props, shapes, Physgloves and campaign map tests, the Campaign Builder's full editing test, and tools/release/ship.py.

---

## v1.6.1 - Every one of you (2026-11-19)

The dark below learns that you did not come down alone. It now listens to all of you, and so do the others.

- **The MAC-11.** A machine pistol for the hip: eighteen rounds a second, a climb you have to hold down, a spread that blooms while the trigger stays back and settles when it lets go. Its own .380 rounds, its own boxes to find, and the Builder can place both. A magazine with rounds left changes quickly; an empty one also wants the knob drawn back. Key 4, or the wheel.
- **Brass that falls.** Every gun throws its spent cases now: the shotgun's shell when the pump works back, the pistols' brass to the right. They tumble, ring off the floor, and lie a while. The shell that used to hang beside the shotgun after a shot is gone.
- **Guns in the hand, in VR.** The barrel points where the hand points and the sights meet the eye. Reloads are the hands' own work: B drops the magazine, the other hand takes a fresh one from the belt and seats it, an empty slide or knob is drawn back by hand; the shotgun takes its shells one at a time through the port, then the pump. The support hand wraps the fore-end without the wrist turning over, and the pump cycles every time. The kick is in the gun, never the view, and the controllers answer each shot, each magazine and each stroke. The MAC-11 rides the hip and fires as long as the finger holds. Hold the left stick down to speak.
- **Co-op that sees everyone.** Creatures, guards and the people below choose among all of you: who they can see, who hurt them, who is near; a pack spreads itself across you, and nobody flips from one of you to another in a heartbeat. Pressure plates and triggers answer any of your steps, and a trigger can wait for all of you. A guest's lever is pulled on the host's world. Captions, sounds, states and objectives reach everyone. Alone, the world is as it was: one of you is simply the whole list.
- **Apart, on purpose.** In co-op you can split up: each map with one of you on it stays awake and runs. Leaving a map no longer drags the others after you; COOPFOLLOW ON in the console brings back the old way. A player who ended up in a wall is let out instead of put back in it, spawns set in a wall step out onto the floor, UNSTUCK frees you, and the host can TP anyone beside anyone.
- **The same crates for all.** Props are the host's: what one of you knocks over falls the same way for everyone, what you carry and throw lands where the host says it lands, and a bottle broken on one screen spills on all of them.
- **Talk.** T opens a line of chat; voice through Epic with push-to-talk (V) or an open mic. In co-op the voices are where the bodies are: close by, clear; down the corridor and behind a wall, thin. The scoreboard marks who is speaking and mutes who you choose.
- **A quieter pause.** RESUME, SETTINGS, SAVE, LOAD, RESTART and QUIT, nothing else; in a match, the code and LEAVE MATCH. SETTINGS is its own place now, in pages: AUDIO, VIDEO, CONTROLS with every key, GAMEPLAY, COMMUNICATION with your microphone, your speakers and a meter to test them, and VR. Each line says what it does; each page can go back to how it came.
- **Still to try between two machines.** None of this has been played across two computers yet, and the VR changes want a headset. docs/Systems/vr-playtest.md says what to try.

Verified with the match, session, Epic sign-in, chat and voice, menu, MAC-11, VR guns, VR, Limitator, saves, settings, scripting, AI, props, clutter, Physgloves, liquids, map format, kits and streaming tests, and tools/release/ship.py.

---

## v1.6.0 - Not alone down here (2026-11-19)

Others came down after you. Not all of them came to help.

- **Multiplayer.** Deathmatch on custom maps and co-op through a campaign, hosted by players, free, over Epic Online Services. No Epic account is needed: the game signs you in on its own. MULTIPLAYER on the title holds it all: QUICK MATCH finds the fullest open game of a kind or opens your own; BROWSE lists open games with their map, players and ping; HOST A GAME picks the mode, the map, how many come down and whether the game is listed; JOIN BY CODE takes the six characters a friend gives you (no 0, O, 1, I or L to misread). A player who lacks the map is handed it as they join. Nobody forwards a port: the connection finds its way through, or through Epic's relays.
- **In a match.** Tab holds the scoreboard. The kill feed runs in the corner, the clock and the frag limit at the top, the count to your return in the middle when you fall. The host's shots and everyone's are judged on the host, against where each player stood when the shot was seen. In deathmatch there are no creatures, only each other; spawns are the map's own, or found on open floor, and a round ends at its frag or time limit and begins again. In co-op you go down together; a fallen player rises beside the host. The pause menu shows the code and holds LEAVE MATCH; no game is saved in a match.
- **VR and desk together.** A player in a headset is seen whole, arms and fingers as their hands move; a player at a desk is seen running, aiming and reloading. Both play the same match.
- **The Limitator.** A second body to go down in: heavier, helmeted, scarfed. CHARACTER, on the multiplayer page, chooses Claire or him; he is who you are in first person, in third person and in VR, and who the others see.
- **A new front door.** The title is grouped and lit: the campaign, then MULTIPLAYER with the state of your link beside it, then settings and quit. A line beneath names what each does. Mouse, keys, a controller and the VR pointer all work it.
- **The Builder makes arenas.** A Multiplayer page sets a campaign's modes, the deathmatch map, how many may come, the frag and time limits and friendly fire. The Spawn tool sets where players start: a ghost figure with its facing, R to turn, a click to set, a team if you want one. The Starter Map's paint yard is now a small arena with eight spawns.
- **Still to come.** In co-op the creatures and the people below still only notice the host; scripts answer only the host's steps. The next update teaches them everyone. None of this has yet been played between two machines.

Verified with the multiplayer, session, Epic sign-in, menu, Limitator, Claire, map format, kits, VR, third-person and settings tests, and tools/release/ship.py.

---

## v1.5.8 - Her own two hands (2026-11-19)

In the headset you are no longer a pair of hands in the dark; you are Claire, all of her. Above the hill the Builder grows a hundred rooms' worth of new things, and the crates that haunted it are laid to rest.

- **All of her, in VR.** In the headset you are Claire, the way VRChat has it. Look down and her body is there. Her arms run from her shoulders to your hands and bend at the elbow as yours do, and her fingers close as yours close. Her legs walk with the stick and kneel when you duck. Her shoulders turn after your head, and she leans when you stoop. Nothing floats free of her.
- **Guns on her body.** The shotgun rides on her back and the handgun at her hip. Reach over a shoulder or down to the hip and grip to draw one. Let go back at its holster and it goes away; let go anywhere else and it stays in her hand. On the shotgun, the other hand wraps the fore-end, the wood slides back with it when you pump, and a reload takes that hand to her hip and back to the gun.
- **VR COMFORT.** A new page under Settings in VR has the turn speed (turning is smooth only), a comfort vignette that closes in while the sticks move you, seated play, height recalibration, left-handed play, the render scale and a frame-rate readout.
- **Steadier frames.** In VR each eye can be drawn at a smaller share of its size and stretched by SteamVR. On AUTO the game picks the share from how long the last frame took. The far sun shadows are redrawn every other frame.
- **Subtitles that follow.** Subtitles, area titles and a short health and ammunition line hang ahead of you and drift round after your look instead of being pinned to the face. Nothing is fixed to her hands or wrists any more.
- **Steadier hands.** Two hands letting go of a crate a moment apart throw it with both. A gun hand or a hand on the fore-end no longer sets off the Physgloves when pumping. A VR hand can no longer wrench loose what is bolted down. A hand that jumps when tracking returns is drawn where it is, not dragged through what lies between.
- **Held props turn with the hand.** In VR a prop held in one hand keeps the place and the turn it had as the hand closed on it, and turns exactly as the hand turns: no snap to the pointing, no lost roll. With two hands it also rolls as both hands roll, and the second hand takes hold closing on the prop's surface, not only reaching inside it.
- **Grip to hold, trigger to punt.** Letting go of the grip lets go of the prop. A grip pressed away from any prop no longer takes one into the other hand. The trigger of the hand holding a prop, or of the gun hand with no gun drawn, punts it along the hand, as hard as fire throws it on the PC. With a gun drawn its trigger still fires.
- **Spilt liquid stays where it was spilt.** Puddles, streams and drops belong to the map they were spilt in. Before, a spill was drawn at the same spot in every map, at its old floor's height, so it hung over crates and showed through walls. Now it is drawn only in its own map, is still there when you come back, and a new map or a new game starts dry.
- **Untried in a headset.** None of this has been tried in a headset yet. docs/Systems/vr-playtest.md lists what to try.
- **104 new parts for the Campaign Builder**, from the purchased packs: cubicles, beds, sofas and wardrobes, a diner kitchen and a bathroom, shelving, crates and boxes, machinery and lab pieces, lamps, doorways, lift doors, windows and tunnel junctions, a minivan, fences, a tent and campfire, rocks, and horror dressing (cobwebs, masks, a butcher's knife, a bare bed frame).
- **New parts-browser tabs:** Kits, Kitchen, Storage, Outdoor and Horror. Street barriers, fences and cars from earlier packs moved to Outdoor.
- **Kits:** 21 ready-made pieces to drop into a map: eight furnished rooms (office, ward, lab, bathroom, storage room, control room, server room, kitchen), four corridor pieces on a 3 m grid, a stairwell, a lift lobby, two doorways, a catwalk run, a loading dock, a yard fence, a ruin corner and a campsite.
- Picking a kit shows its ghost on the plan with an arrow to its front. R turns it, a click sets it down, and one Ctrl+Z takes it back. A placed kit turns with R like anything else, and **Take apart** frees its pieces to move or change one by one.
- **Starter Map:** a Kits Gallery with every kit set down, and two more Parts Gallery yards with every new part, all on forest maps that were already there, so no map moves.
- The library importer can scale a pack and finds the textures some packs name differently. A texture shared by several models is now stored once.
- **Turn a material.** Planks, tiles, brick courses, tread plate and grating can run either way. In the Campaign Builder's Paint tool (and the Floor and Flooring tools) R turns the paint a quarter and Shift+R turns it back; a floor just painted turns with it, so you see it turn. A picked block, shape, stair or balcony floor has a Turn box; any angle is under Advanced. The game draws the turn exactly as the Builder shows it, and older maps look as they always did. The Starter Map's hillside house has its planks turned.
- **The ghost is what you get.** A placing tool's ghost, on the plan and in the 3D view, is now made by the placement itself: a desk snapped to a wall shows against the wall and turned, a sign on its wall, a lamp in a cave on the cave's floor, a pipe or cable from where the drag really starts. A drag on the plan shows the real result in 3D, a preview held back while the picture was busy catches up, and switching tools, maps or floors, Undo and Redo leave no ghost behind. A picked thing that was turned is ringed the way it stands.
- **Nudge with the keyboard.** With something picked, the arrow keys move it a grid step along the floor and Page Up and Page Down raise and lower it (Shift for a centimetre, Ctrl for a metre). In the 3D view Up moves it away from you. Each press is one Undo, a held key one in all, and the floor stops a lowering once before letting it sink. With nothing picked the keys work as before.
- **No more phantom crates and barrels in the Builder's 3D view.** Crates and barrels that were never in the map came and went in the 3D view and could not be deleted. They were the ghosts of an earlier preview: a new one built in the same memory kept drawing what the old one had baked. The renderer now knows each game and each map by its own number, never by where it sits in memory, and lets go of what it baked as soon as the map it belonged to is gone.

Verified with the Campaign Builder's full editing test, the game's VR, liquid, kit, parts, material-turn and scene-reuse tests, and tools/release/ship.py.

---

## v1.5.7 - The headset again (2026-11-18)

A fix for the choice that went missing.

- **Play in VR is back.** Since v1.0.4 the game has looked for SteamVR in the wrong place in the registry, decided there was no headset, and never asked how you wanted to play. It looks in the right place now, finds SteamVR in a second Steam library too, and asks again: Play on PC, or Play in VR. If it ever cannot find the headset, RawMetal-vr.txt beside the game says exactly why.

Verified with tools/release/ship.py and the game's VR test, which now checks the headset detection itself.

---

## v1.5.6 - A quieter hand (2026-11-18)

A small fix for something players said shone too brightly.

- **No more orange.** What the Physgloves are about to pull no longer burns orange. A thin, cold line traces its edge and breathes slowly, and the crosshair draws in a little; nothing else around it lights up. Settings has a Pull Highlight row to make it subtle or turn it off.

Verified with tools/release/ship.py and the feature lab's Physgloves and settings tubes.

---

## v1.5.5 - Sure footing (2026-11-18)

A fix for floors that played like ice, and for the things that came apart when struck too hard.

- **Floors have grip again.** A thrown crate no longer skates across the room. It lands, loses its speed to the floor, and stops about a pace from where it came down. Floors have their own bounce now too: concrete and metal give a little, carpet and mud hardly any, and a wooden crate barely leaves the ground.
- **Nothing slips through.** A body thrown hard, a door slammed against a thin post, a lamp whipped on its chain: none of them pass through thin walls any more, and they stay in one piece doing it.
- **Pins hold.** Hinges, welds, sliders and ropes keep their pins after a hard blow, so a heavy door stays on its hinge and a welded pair stays welded.
- **Round things are round.** Drums, barrels, mugs and tyres roll smoothly against crates, tables and rocks, and still stand flat on their ends.
- **A blast costs less.** Twenty crates thrown by one explosion take a tenth of the work they did, with nothing slipping through the walls.
- **Heavy on light, only when it rests.** A heavy drum falling into a light crate in the air now meets it as the light thing it is.

Verified with tools/release/ship.py and the feature lab's physics tubes for each of the above.

---

## v1.5.4 - Round things roll (2026-11-18)

Things keep their true shapes now, and some of them hang.

- **Round things roll.** Pipes, bottles, extinguishers and gas cylinders are round all the way, not many-sided, so they roll smoothly across a floor and still stand on their bases. A rock lies on its own faces, and the broken pieces of a chair or a monitor settle on the edges they broke along. A round meets a rock where the rock is, not around it.
- **Cloth.** Curtains, straps and cables hang and sway. A curtain parts when you walk through it and falls closed behind you. A cable drapes over what it lies across, a strap swings when it is shot, and outdoors they move with the wind. In the editor, Hang cloth lays them out; the Starter Map has a curtain, a strap and a cable to try.

Verified with tools/release/ship.py, the feature lab's tubes for each of the above, and the editor's test modes.

---

## v1.5.3 - Doors that stay (2026-11-18)

A fix for the small things that should have held, and a lighter game to carry them.

- **Doors and drawers.** A door nudged gently stops where it is left, and a drawer pushed shut catches and stays shut. In the editor, Connect hangs doors, hatches and seesaws with the right stiffness already set, and adds a Drawer and a Nailed board that snaps under a heavy shove.
- **Ramps.** A crate sliding onto a ramp follows the slope instead of hopping at its foot, and a rolling bottle no longer bounces back off the first step.
- **Water.** A sealed drum dropped upright tips onto its side and bobs. A crate rides at the depth its weight says, and a plank dropped on its end lies down flat.
- **Breaking.** When a crate is smashed, its planks fly along the blow that broke it, and a spinning thing breaks spinning. Nothing pops out of a break standing still.
- **Sleepers cost nothing.** Hundreds of things lying still now cost next to nothing while a few others move.
- **Smaller.** The game carries more of its textures, and its skies, as small sketches it redraws the first time they are needed, with a better learned hand that can work from a quarter of the size. Every one was held up against its original and kept only where the two read the same; the rest are kept whole. The game is lighter for it, with room left for what comes next.

Verified with tools/release/ship.py, the feature lab's tubes for each of the above, and the editor's test modes.

---

## v1.5.2 - Held together, below the hill (2026-11-18)

Things hang now, and swing, and come apart along their seams; the dead fall like the dead; and the ground has an inside. Dig, and it opens.

- **Quicker to wake.** The game no longer unpacks the whole library before it opens its eyes. A model and its textures are taken out the first time a map asks for them, the room you stand in on every core at once, the rooms beside it quietly while you play, so it wakes in less than half the time it did, and the library can grow without the wait growing with it.
- **Joints.** Vectra holds things together: hinges, ball joints, sliders, welds, ropes, springs, pulleys and motors, each with its limits, each able to snap when asked too much of. Lamps sway on their chains when struck. A crate hangs on a rope until you shoot the rope. There are seesaws, sprung gates, a rope bridge that sags under you, and a pulley lift with its counterweight. You can stand on what is held together.
- **Connect.** In the editor, pick Rope, Chain, Hinge, Spring, Weld, Motor, Pulley or Hanging lamp, and drag from one thing to another, to a wall, or to the ceiling. A ghost shows what you are making. Hinges find their own pins. One click under a ceiling hangs a caged lamp on its chain.
- **The dead.** Bodies are Vectra bodies now, jointed as a body is, with the bend and twist a knee or a shoulder allows. They tumble down stairs, drape over table edges and lie still where they stop, without the small jump the old ones gave as they settled, and they never rest crouched against a wall like something waiting. A save keeps every body exactly as it lay. The old bodies are still there behind a switch (`RM_RAGDOLL=legacy`).
- **Physgloves.** Point at something loose and flick, and it comes to your hand on a long arc. Catch it. Light things fly; heavy things only lurch toward you; a thing on a rope pulls at its rope. They work at a desk and in VR.
- **Pieces.** A chair breaks into its legs, its seat and its back. A crate comes apart into planks, a bottle into its neck and its shards, a monitor into its casing and its glass. The pieces are cut from the props themselves.
- **Liquid.** Bottles hold water, oil, and something green that glows. It moves when the bottle moves, pours when the bottle tips, runs from the hole a round leaves, and spills all at once when the glass gives. It pools on the floor, dark and wet. A full bottle sinks; an empty one floats.
- **Two hands.** In VR your hands are solid. They push things and do not pass through them, and a crate or a chair can be carried in both, turning as your hands turn.
- **Aim.** A round meets a thing where the thing is, not where a box drawn around it would be. Shots pass the corner of a turned crate and through the gap under a chair. A crate held up as a shield stops only the blows that reach it.
- **Speed.** Fast and spinning things no longer pass through thin posts, thin walls or each other.
- **Building.** The editor offers every shape the engine knows, forty-five of them, as pictures in plain groups. Ten are new: a roof wedge, a spire, a pyramid roof, a spiral stair, a culvert, a buttress, a bevelled block, an eight-sided pillar, a curved wall and a catwalk. Drag a shape to size, or stretch, raise, lift and turn it with handles in the 3D view. Where shapes overlap, the faces buried inside are cut away, so a building of stone, brick, concrete and copper reads as one.
- **The Starter Map** has a joints corner, a room of shapes, and a bench of bottles and breakables to try.
- **Parts.** Seventy-six more pieces from the packs: walls plain and broken, pillars whole and snapped, beams, stairs of concrete and of wood, heavy doors and barred windows, railings, catwalks and scaffold decks, pipe runs and culverts, a tank rig, a cage, lamps that hang and lamps that hood, brick rubble, and from the mine its lantern, barrels, crates, locker and shelving, with boulders and rock walls and two heaps of gems. The editor keeps the building pieces under their own tab, Building, each with its picture.
- **Caves.** The land is no longer only a surface. The editor's Cave tool digs into it: brush a tunnel at the depth you choose, click for a cavern, drag a cave mouth in from the hillside and watch the slope open. Fill it back, pull a wall out with your hand, smooth it, level a floor. The rock is rough and whole, there are no holes in it, and you walk where you dug.
- **Move.** A new brush for the land, too: take hold of a hill and drag it, and it follows. Hold Shift to lift it.
- **Underground.** Under the rock the sky does not reach. It is dark down there but for what you carry and what you hang, and a lantern set in a cave stands on the cave floor, not on the hill above it. Water finds the low places in a cavern as it does anywhere else.
- **Steadier hands.** Vectra lets a falling thing reach the floor before it bounces, where it used to turn back a few centimetres short and spring off the air, and a crate sliding as it lands no longer comes up spinning. A thing spun hard enough to turn a quarter of the way round in one step still stops on a thin post. A cast that grazes past without closing in no longer reports a hit. A crate woken by a shove wakes what it holds in the same moment, and what is carried on a joint by a moving hand wakes and follows it instead of hanging back asleep.
- **Held, not forced.** A thing pulled toward a pose turns no harder than it is pulled. A long plank pressed into a doorframe leans against the jamb and stays there, still, instead of shaking against it.
- **Floors of their own.** The ground answers a sliding thing with its own grip now, not only the thing's. A crate shoved across a puddle runs nearly twice as far as on dry concrete; carpet holds it sooner; gravel and grass drag at what rolls.
- **Weight that stays.** A heavy drum set on a light crate rests on it. It no longer sinks into the crate, nor presses the crate into the floor, nor springs back off it.
- **Quieter rooms cost less.** A room of four hundred resting things costs half what it did each step, and a burst of shards flying through them a fifth less.
- **Scrapes and rolls.** A crate dragged across the floor scrapes as it goes and falls silent when it stops. A barrel rolling on its side rumbles. Wood, metal and glass each sound their own.
- **The Starter Map** has a gallery of the new parts on two gravel yards, and east of it a hill with a mine in it: a mouth in its west face, a timbered drift with rails and an ore cart, a cavern of crystals by lantern light, and a passage south to a pool that drips.

Verified with tools/release/ship.py, the feature lab's tubes for every system above, the editor's test modes, and pictures of each.

---

## v1.5.1 - Ground to stand on (2026-11-17)

Things have weight now, and the places you build have somewhere to stand.

- **Vectra.** The props have a new physics engine, Vectra, and it is the default. Every crate, chair, barrel and bottle is a real body now, turning any way it is pushed. Stacks stand and settle faster than before. Barrels roll down ramps and tumble down stairs. A crate holds on a gentle slope and slides off a steep one. A plank on a crate makes a seesaw, a row of boards falls like dominoes, and a wardrobe pushed high tips while one pushed low slides. A busy room costs less than it did. The old physics is still there behind a switch (`RM_PROPS=legacy`) for anyone who misses it. See docs/Systems/physics.md.
- **Weight.** A crate meets a man now; it no longer passes through him. Heavy things knock people and creatures back by what they weigh and how fast they come: a thrown crate sends a guard reeling until he finds his feet, a rolling drum takes him down, and a mug only annoys him. Harm follows weight too. A dropped crate crushes the mug under it, and a creature hit hard enough forgets what it was reaching for.
- **Mattresses.** They bend now. Carried by one end they sag, let go over a step they drape, and struck they shiver and lie still. A save keeps them as they lay.
- **Building on a hill.** In the editor, a room drawn on a slope finds its own footing: a level pad cut into the ground, a foundation under the low side, stilts you can walk beneath, or terraces with steps between. Pull a floor's edge up and it settles at a step, half a storey or a storey. Doors left high get steps down to the ground on their own. No more stepped floors, jagged walls or daylight under them.
- **Flooring.** Pick a look (planks, tile, carpet, stone, brick, grating) and drag it out. On a hill it follows the ground as a path or levels into a patio.
- **Stairs and balconies** take their own materials now, for steps, fronts and rails, and decks, edges and railings. Click them with Paint.
- **Glass you can place.** The Window wall tool cuts a wall and fills it with a framed grid, clear, dirty, frosted, wired, mesh or already cracked, a doorway in it if you like. Every pane breaks on its own. Plain glass snaps into walls and openings.
- **Holes in walls.** The Breach tool breaks through a wall, ragged, arched or square, with broken edges, rebar and rubble. You can walk through it, see through it and shoot through it, and so can they.
- **Growth.** A Growth brush lays ivy and moss on walls, creepers hanging from ceilings and ledges, and grass up through the floor. Creepers sway, part as you pass and flinch when shot.
- **Light through openings.** Shafts of sun fall through breaches and high windows, from the edge of the opening to where they land, hidden by whatever stands in front, and they leave a warm patch on the floor.
- **Ten new skies**, from clear day to a black sun over red ash, and each one lights the land beneath it. Night is dark now.
- **A quieter editor.** Placing a room no longer plays a horror sting. The editor clicks and chimes softly instead.
- **Nothing lost.** If the editor falls over, it keeps a record of what you were doing and gives you your work back when it opens again.
- **Maps** can name things, change one thing's colour, weight or loyalty without touching the rest, keep values for scripts, and stamp out prefabs. See docs/Systems/map-format.md.
- **Saves** are version 28, built in sections so they can grow. An older save is brought up to date when it loads, and the original is kept beside it, even one whose story was written down out of order. Since v1.0.4 saves and settings had been written to the wrong folder; they are found and brought home.
- **The Starter Map** has an overgrown atrium, a house on a hillside and a physics room to try it all in.
- **Fixes.** A Quarantine Ward terminal no longer sinks its desk into the counter, and the map audit now catches a desk standing on other furniture.

Verified with tools/release/ship.py on the full suite, the feature lab's tubes for every system above, the editor's test modes, and before-and-after pictures of each.

---

## v1.5.0 - Things that give (2026-11-17)

The world answers back now. What you hold, what you break, what you fall into.

- **Glass.** A window takes a round where the round hits it: a hole, a white star, cracks running out from it. Hit it again and the holes join and pieces fall out, real shards that tumble, ring and break again on the floor. Big panes come down in parts. A thrown crate punches its own shape through and keeps going. Rounds and props pass only where the glass is gone. Shards crunch underfoot, and things hear it. Reinforced glass webs, sags and crushes white, and holds.
- **A crate between you and it.** Hold a crate, a chair or a box toward something that wants you, and its blows land on what you hold until it bursts in your hands. A thing that will not break is torn away instead. Then you are on your own.
- **Things in things.** Baskets, crates, bins and carts hold what you put in them. Carry the crate and the bottles come with it. Tip it and they spill.
- **Soft things.** Mattresses, bedrolls and ward beds give under a body and rise again after.
- **Water.** It holds up what floats and takes down what doesn't: crates bob, monitors sink to the bed. Jumping in, wading, throwing things in and shooting the water all splash and send rings across it. A loud splash is heard.
- **The dead.** A corpse takes each round on the part it hit, lies on crates and beds instead of inside them, is moved by a heavy prop, and settles without twitching into place.
- **Plants.** Grass, ferns, bushes and trees move in the wind, bend when you push through and spring back, and flinch when shot.
- **The Builder, outdoors.** Picking, placing, water and triggers now follow the land instead of the map's zero. New Terrain tools raise, lower, smooth, flatten, ramp and paint the ground; Nature brushes scatter the purchased trees, bushes, grass and rocks; Water draws pools, fills hollows and takes its level from a bank. An indoor map now meets an outdoor one with steps instead of a wall of earth.
- **The console.** `summon` brings creatures, guards, soldiers and civilians, on your side, against you or on none, with their own orders. `ally`, `enemy`, `kill`, `freeze`, `killall`, `list` and `help summon` work on what you aim at. Creatures and men on opposing sides fight each other. Summons do not survive a save. See docs/Systems/console.md.
- **Drawing creatures.** Creatures draw through the same instanced stream as people, cutting the CPU's share of a busy outdoor frame by more than half.
- **Saves** are version 27: windows keep their holes. Older saves load.

Verified with tools/release/ship.py on the full suite, the feature lab's tubes for every system above (glass, soft bodies, containers, water, ragdolls, plants, summons, instancing), the Campaign Builder's outdoor test, and before-and-after pictures of each.

---

## v1.4.16 - Set down (2026-11-17)

A small patch for what you put down.

- **Dropping on the move.** A box let go of while walking was thrown, not dropped: the hand's path already carried your walk, and the walk was counted again on top of it. It left your hands at nearly twice your pace, flying, and could crack a window or hurt whoever it met. It falls at your side now. A flick of the look still flings it, as it should.
- **The release checks.** A release now retests everything changed since the last release that actually went out, not since the last one that was only attempted, and changes to the map and game cores bring the save, prop and creature tests with them.

Verified with tools/release/ship.py and the clutter and props suites, with a new check that a prop dropped mid-stride falls at a share of the walk and is not thrown.

---

## v1.4.15 - Open sky (2026-11-17)

A patch for the weather above an outdoor map.

- **Forty-seven skies.** The paid Brutal panoramas are in the engine as neural packets — same AI reconstitute path as the wasteland sky — so they store small and still draw at full size. Outdoor terrain follows every terrain sky, not only the old two.
- **Map sky.** The Campaign Builder has a Map sky control: pick any catalog sky per map, with undo, redo, and save/reopen.
- **Containment desks.** A monitoring CRT that already sits on a table no longer grows a second freestanding desk under it.

Verified with the engine skyboxes suite (decode, terrain, native and software switching) and the Builder's skybox picker checks.

---

## v1.4.14 - Wet hit (2026-11-16)

A patch for the sound when a round meets water.

- **Water splash.** Shooting a pool no longer plays a thin synthetic tone. The strike is a real wet sample from the liquids kit — a short plash and a thrown-splash body — so it reads under the gunshot instead of as a beep.

Verified with the water-audio mixer checks and the handgun water-surface cases.

---

## v1.4.13 - Let it fly (2026-11-16)

A patch for the moment you let go.

- **Flick release.** On desktop, releasing a held prop with E keeps the look you were making: spin hard and it flies sideways; flick the mouse up and it goes up. Soft put-downs still land nearly dead. Left-click throw is unchanged. VR still uses the tracked hand.

Verified with the props checks for look-up and spin release, and the physics-stages checks.

---

## v1.4.12 - In your hands (2026-11-16)

A patch for the heavy thing you are holding.

- **Carried contact.** A modeled prop of at least 8 kg pushes guards and creatures while you carry it — walking forward, reaching it out, or turning it in place. A body pinned against a wall stops the prop instead of letting it pass through. After AI steps, overlaps against the held prop are cleared again so a shove is not walked back into.
- **Cover that breaks.** Solid held props still stop rounds. Enough hits shatter them, drop them from your hands, and leave you open.

Verified with the feature lab's held-prop-contacts tube (reach, rotation, walk, and wall-pinned cases at 60 and 120 Hz), and the props and physics-stages checks.

---

## v1.4.11 - Only what changed (2026-11-16)

A patch so a release does not re-prove the whole game when you only touched one system.

- **Affected ship tests.** `ship.py` maps files changed since the last release to the suite tests, extras and audio they cover (`tools/dev/affected.py`). Preview with `python tools/dev/affected.py`. Unknown files under `src/` still run the full suite. `--full-suite` forces everything; `--full-audio` still forces the long audio proof.

Verified by dry-running the map on the v1.4.10 audio diff (eleven tests and smoke audio, no campaign-map extras) and by shipping this release through the new gate.

---

## v1.4.10 - Quicker out (2026-11-16)

A patch for the time between a change and a release.

- **Ship audio.** The release gate runs a smoke audio proof by default: every score still has to decode, masks and speech still have to hold, and a few area seams still have to carry — without mixing a full second of every campaign map. After soundtrack or mixer work, `python tools/release/ship.py … --full-audio` still runs the long proof.
- **Orbit.** The machine-orbit check reuses one map instead of rebuilding it hundreds of times, so even the full audio test spends less time waiting on the same room.

Verified with tools/release/ship.py (the full test suite, `--audio-test=smoke`, and the Campaign Builder's checks on the release build); smoke audio passed in about half a minute on the ship machine.

---

## v1.4.9 - In the hands (2026-11-16)

A patch for what you carry, what hits a body, and what a round does when it meets water.

- **Carrying.** Hold a prop and the middle mouse button turns it without moving the camera; the mouse wheel reaches it farther or closer without changing weapons. It will not scroll into you or through a wall.
- **What a prop does to a body.** A loose crate, monitor or board that strikes you, a guard or a creature now pushes and hurts by how heavy it is and how hard it meets them. A shove stops at a wall. A prop resting against someone does not keep dealing damage. A thin board's empty corners no longer count as a solid square. A kick from the room itself is not counted as your attack.
- **Water.** A shot into a pool raises a splash and a wet sound on the surface, and still finds whatever stands under or beyond the water. Cover before the pool keeps it dry. A round parallel to the surface, or that begins already under it, makes no entry splash. It works across an open doorway into the next chunk.
- **The console.** `GIVE 12 GA`, `GIVE 12GA` or `GIVE SHELLS` adds sixteen shotgun shells to the reserve.

Verified with tools/release/ship.py (the full test suite, the audio test and the Campaign Builder's checks on the release build), the props and physics-stages checks for carry and body contact, the handgun water-surface checks, and the feature lab's starter-water and water-audio tubes.

---

## v1.4.8 - Weight (2026-11-15)

A patch about things that are heavy, and things that should have been where they were.

- **Glass.** A crate thrown at a window cracked nothing unless it was flying fast and struck dead centre. Glass now takes a blow by how hard it is hit: a thrown crate cracks it, a hard throw or a heavy drop breaks it and goes through, a tossed mug does not, and the reinforced panes still only craze. A breaking window is heard as far as a gunshot.
- **Chairs.** A crate or a barrel struck a chair and the chair did not care. Chairs now slide, tip and go over when something heavy meets them, and every chair standing on a floor in the game is loose, not only some of them.
- **The torch.** On the shotgun the flashlight hung in the air beside the gun. It rides on the receiver now, at the hip, aimed, walking, crouched and reloading.
- **Beds.** Straps floated above the beds in Medical Screening, over the recovery bed in Surgery, and a body hung above its gurney in the Quarantine Ward. They lie where they are laid. The audit now looks for it.
- **Saves and maps.** Saves are version 26: a v1.4.8 save will not open in an older build, which would show every chair twice; your older saves open here with every chair where it stood. A custom map can keep a chair fixed with the FIXTURE record's new anchored field (custom maps/GUIDE.html); the Campaign Builder reads and writes it.
- **Field Measurement.** The techs' office worked with their backs to the room and their screens to the wall, and the terminals there had grown a second screen. One screen each, facing the chair.

Verified with tools/release/ship.py (the full test suite, the audio test and the Campaign Builder's checks on the release build), the feature lab's heavy-impacts tube (crates at nine speeds against glass and reinforced glass, chairs struck head-on, side-on and by a shove, a resting stack left ten seconds), the arms inspection measuring the torch on its mount in eleven poses, the map audit over every touched map, and before-and-after shots of each fix.

---

## v1.4.7 - Inspected (2026-11-14)

A patch from the inspectors' notes. Every item they marked is mended, and the audit now looks for each kind of fault they found.

- **Saves.** A game carried deep enough grew a save bigger than the game was willing to read back, and the autosave crossing into Deep Access Control was refused because the floor there stands above the map's base. Both load now, and a save that will not load says why.
- **The research levels, put right.** The concourse exhibit is a glass case, not a shelf with a door growing out of its side. Straps lie on the beds instead of above them. Kits, extinguishers and lamps hang on walls and rails instead of in the air. Trim stops at door frames. No bench, notice board or box stands in a door. Terminals face the room. Lamps no longer sit inside one another. Desks no longer stand on desks. A railing no longer crosses the walk onto the specimen gallery. The dead in the quarantine holding room are laid out, not lined up like luggage. The card readers at Deep Access Control are the size of card readers.
- **The Head.** The great ring in the Extraction Interface is solid now, and stands clear of the path through.
- **A man who would not die.** The hurt engineer in Containment Intake could be shot all day; he, and others like him, can die now.
- **noclip.** Typed at the console, it lets you fly through walls, for checking a map.

Verified with tools/release/ship.py (the full test suite, the audio test and the Campaign Builder's checks on the release build), before-and-after shots of every inspector item, the map audit over every touched map, the chapter route over all 170 passages, and saves written by v1.4.4 and v1.4.5 and through a whole game from map 0 to 109.

---

## v1.4.6 - Heard everywhere (2026-11-14)

A patch, for the ears first.

- **Sound.** The plant's own noise was losing to its music and to itself: the score sat too close to footsteps and gunfire, and machines humming in rooms you could not hear had taken most of the voices the mixer could play, so a footstep or a shot could be dropped for a fan two rooms away. The score is lower now, sounds out of earshot let go, and nothing heard near you is ever pushed out by something you cannot hear. Everyone who walks makes footsteps now, not only the guards.
- **Shadows.** Since v1.4.2 a shadow on a smooth floor could be mistaken for something shiny and vanish into a reflection. Shadows only darken again, and people cast them as well as creatures.
- **What you push.** A heavy crate shoved hard into a thin wall could end up on the other side of it. Props now feel for walls along their whole outline, in short steps, and stop.
- **The night engineer** speaks in a man's voice, as his face says he should.

Verified with tools/release/ship.py (the full test suite, the audio test and the Campaign Builder's checks on the release build) and the feature lab's tubes for each fix: the mix in three maps with sounds over the score and no loop holding a voice out of earshot, shadow pixels for people and creatures, fifty props shoved into walls with none inside one, and the engineer's lines.

---

## v1.4.5 - What things are made of (2026-11-13)

A patch. Glass, then chalk; now the things down here look like what they are.

- **Materials.** Every surface now answers light the way its stuff does. Painted steel has a soft sheen and shows bare metal where it has been knocked; bare steel and machinery are plainly metal, bright where a lamp catches them and holding a blurred picture of the room; rust, concrete, cloth and raw wood stay dull; sealed floors and vinyl have a satin finish that carries the ceiling lamps down a corridor in soft smears; chrome, glass and water are sharp. Nothing is one even gloss any more: wear, grime, stains and grout break every highlight up, worked out from each texture itself.
- **Wet only where it is wet.** A whole floor used to shine as if it had just been mopped. Now it is wet around standing water, under drips, leaks and sprays, at drains and in the low spots, and outdoors only where the rain reaches; everywhere else is dry. Custom maps can paint wet patches with a WET record.
- **The lamps in the floor.** The ceiling lamps are now part of what the room reflects, so a polished corridor shows where its lights hang.
- **How the dead fall.** A man who is shot now goes over: his joints stop where joints stop, his limbs no longer pass through him, he does not skate across the floor once he is down, and he settles in two or three seconds where he fell, over a table edge, down a stair, against a wall.
- **Raw Metal Filmmaker.** The Campaign Builder's film cameras grow into a small film studio: a list of shots, a timeline for each, keys for the camera (where it is, where it looks, how wide), for people and creatures (where they stand, which way they face, what they are doing) and for the map (doors, lights, states), a preview in the 3D view, and a render of every shot to frames and, with ffmpeg installed, to video.

Verified with the full test suite (all 66 checks, gore, carcass, campaign-map and repeated-frame rendering) and the audio test on the release build, the feature lab's material, wetness and ragdoll tubes (energy-conserving shading, every texture in its material's range, a dry map dry, ten falls at rest without sliding and within their joints), and the Campaign Builder's self-tests, editing test (a shot made, keyed, saved, reloaded and filmed) and map round-trips.

For anyone building it: `python tools/release/ship.py VERSION` now builds, tests, packages and publishes a release in one command (`--offline` stops before publishing), and `python tools/release/install_map.py MAP.rmc` installs a custom map for the game and the Campaign Builder and checks it loads (docs/Systems/shipping.md).

---

## v1.4.4 - Quiet now (2026-11-12)

A patch. The dead had too much to say, and came apart the wrong way.

- **The dead are quiet.** A man you had killed could go on screaming: a ward's breathing behind a curtain kept playing at a bed whose sleeper was dead, a dead guard finished his radio lines, and shot workers cried out in Claire's voice. A dead man now makes one sound, his last, and nothing after it; a body you shoot again makes the sound of meat, not of a man.
- **How they die.** More cries, and fewer repeats: guards, workers, scientists and women each have several, no man dies with the same cry as the one before him, and a guard dying near his squad sometimes gets a last word out on the radio. The things that were men die groaning, not roaring like a Brute. Lines tied to timed moments in the maps had been silently dropped; they are heard now.
- **Bodies hold together.** A body no longer melts into liquid lumps as it lies: arms, legs, head and trunk keep their shape from the fall to the floor. (They still fold down more than they topple; that is next.)
- **What breaks leaves itself.** A barrel that goes up throws pieces of its own hull, not spare electronics. Ceiling tiles fall as slabs of ceiling; wooden, plastic and metal props break into pieces of themselves.
- **The Machine** has its own pen in the Starter Map's row of every creature. In the Campaign Builder it is the last of the creatures, "Cursed equipment".

Verified with the full test suite (all 66 checks, gore, carcass, campaign-map and repeated-frame rendering) and the audio test on the release build, the feature lab's tubes for each fix (nothing vocal from the dead over shots and a reload; ten deaths with no take twice running; every breakable's pieces drawn from its own material; the Machine drawn where it stands; body sections holding their shape), and the Campaign Builder's self-tests and map round-trips.

---

## v1.4.3 - Matte (2026-11-12)

A patch, and an apology to the eyes. The last one polished too much.

- **Reflections.** Floors and water carried hard lines smeared across what they reflected, and a doubled ghost of the room. That came from the reflection reading the floor's angle in coarse steps and then catching itself. It reads the surface exactly now, will not reflect itself, and fades where it is unsure. Only things that are really smooth hold a picture of the room: still water, puddles, wet floor at a low angle, chrome, glass, mirrors.
- **Finishes.** Painted and worn steel, rust, wood, crates, cloth and rubber had been made to shine like glass and plastic. They are matte again; brushed steel gives a dull sheen, and chrome and glass stay bright.
- **Light.** The bounced light was turned up too far and washed the rooms out. Lit rooms are back to how they looked before, give or take a little; shadows keep a faint warmth of the colour around them.

Verified with the full test suite on the release build (all 66 checks, plus the gore, carcass, campaign-map and repeated-frame rendering tests), and in the feature lab against the v1.4.1 look on the same views: brightness per room, a measure for streaks across reflections, and how much finishes that should be matte still reflect.

---

## v1.4.2 - Air and glass (2026-11-12)

A patch. The great door at the bottom of Chapter 1 is still shut.

- **Light that goes somewhere.** A lamp used to light what it could see and nothing else; the floor beyond its pool was a black slab. Every room now keeps the light that comes off its own walls: the near side of a pillar is warm from the red lamp behind it, a corridor carries a little of the room it leads from, and a dark corner is dark because nothing reaches it, not because the arithmetic gave up. Rooms meant to be black still are.
- **What shines.** Tile, wet floors, steel, chrome, glass and standing water now hold the room in them: the lamps, the doorway, a table's legs. Dusty concrete and rust stay dull. The crystals in the deep levels glow from their cores and throw a cold light onto the rock they grew out of.
- **Air.** Smoke, steam, sparks, grit, dust in the lamp light, drips and spray now share one set of rules and take the room's light. Steam leaks in threads and jets; smoke rolls and frays; a grenade leaves a trail where it flew, goes up as a burning billow and lies down as soot, with a wave of dust along the floor and grit shaken out of the ceiling. The plant breathes where its vents are, and sparks where its panels are broken.
- **Doors.** A Brute on the wrong side of a door that gives no longer stops after two blows to listen to the door rattle. It keeps on.
- **Claire, seen from outside.** The console command THIRDPERSON puts the camera over her shoulder: she walks, runs, crouches, crawls, turns on her feet and holds the shotgun and the pistol the way someone who has used them would.
- **Campaign Builder.** An Emitter tool places any of the air above, on a switch or a timer. A Light menu shows the bounce alone, or what is polished, and bakes the light again after an edit. Play can walk the map over Claire's shoulder. Film cameras can be set down, moved along a path or turned on Claire, and rendered by the engine into a folder of frames.
- **The Starter Map** has a new wing: a room of doors and locks with a Brute to let loose on one, a room of every kind of air, a room built to show the light, the dead laid over edges and the blood of every kind of thing that bleeds, a Machine to wake and short out, an animal behind glass that you can release, and two rooms to hear the difference between a hall and a closet. The guide walks through it.

Verified with the full test suite on the release build (all 66 checks, run in parallel lanes, plus the gore, carcass, campaign-map and repeated-frame rendering tests), the feature lab's door tubes (a Brute through a solid door and a grille), and the Campaign Builder's self-tests, editing test and map round-trips.

---

## v1.4.1 - What it leaves behind (2026-11-12)

An update, not a chapter. The great door at the bottom of Chapter 1 stays shut. Work behind it goes on: people to talk to, things to work out, and in the machine rooms something that was never alive has started to move. The research levels have stopped humming a nursery tune to themselves and the static has been taken off the walls; some rooms down there now have no music at all, and are worse for it. It is not ready to be looked at yet.

- **Blood.** What comes out of things down here was a red smear that sat in the air. It is now laid from real spatter: pools that spread and keep their edges, splats where drops land, bursts on the wall behind a wound with a run trickling down from them, a scatter of drops where something staggered. It is coloured by what bled: red from what was once a person, a sour yellow-green from the things with too many legs, a cold blue-green from the large ones, and black oil from anything that should not have been bleeding at all. It takes the light of the room it lies in. Blood on a tiled floor used to sink under the tiles and vanish; it stays on top now.
- **The dead.** People used to die the way an actor does, and then stay on the floor in the last pose they struck, stiff, held a hand's width off the ground. They fall now. A man who is shot goes down from where he stood, away from what hit him, and lies the way a weight lies: on the floor, over the edge of whatever was under him, an arm off the step. The dead you find were left that way too.
- **What the animal leaves.** A man whose head has been taken lies in what ran out of his neck, and it spreads past where his head was.
- **Doors.** A door could only stand in a wall that ran one way. It stands in either now, and it has weight. Tap the handle and it swings through and knocks its stop; hold it and it eases open under your hand and stays where you leave it, a hand's width or wide. A shut door is latched. A door left ajar gives when you walk into it, and bangs if you run. Every door has a lever, hinges, a kick plate, a frame and a threshold. A locked one shows how it is locked: a key cylinder, or a reader on the wall that is red until it lets you through. A slammed door is heard sixteen metres off by things that listen.
- **Talking.** A person you can speak to now says so ("E / TALK"), and talking to someone comes before picking up the mug at their feet.
- **Light.** Models take the room's light properly; furniture and stone no longer sit dark in a lit room. Lamps throw a little bounce.
- **Campaign Builder.** Doors stand either way: R turns one a quarter, and the inspector sets how it opens, which side its hinge is, which way it swings, the lock it shows, how much battering it takes before something heavy breaks it down, and whether it starts ajar. The plan draws every one of them as it will swing. The builder's engine is brought up to this build with a merge tool (`tools/editor/sync_engine.py`), so custom maps get the new blood, doors and light, and a map's music can now be Silence; model thumbnails were mended, and Turn copy, Flip copy and upper floors no longer spoil doors.

Verified with the full test suite on the release build (all 63 checks pass, including the two that failed on v1.3.0 and v1.4.0, which are now fixed), a new test that lays all 37 of the research levels' dead and finds every one at rest on the floor, the door, hunter, machine, gore and carcass tests, the chapter route over all 170 passages, and the Campaign Builder's self-tests and map round-trips.

---

## v1.4.0 - It gets up (2026-11-11)

This is an update, not a chapter. The great door at the bottom of Chapter 1 is still shut; what is behind it is not ready to be looked at yet, and it will not be shown until it is.

- **R-114.** The specimen in Research Receiving's cold hold used to carry itself like a horse that had been told about people. It does not any more. It waits low on folded legs with its weight in its haunches and its mouth on you; it walks the way something walks when it has decided which of you it wants, feet set down where it means them; and when it comes, it comes all at once, off the floor and onto you. It still takes a lot of killing.
- **What it does to people.** Anything down here with a mouth for it can now take a man's head and stay down on him to feed. While it feeds it is busy. That is your time. Walk.
- **Lockers.** Climbing into one is no longer the end of it. A thing with hands that saw you go in, or hears you knock, tears the doors open and takes hold, and you fight its grip. A Cleanup man who finds you hauls you out, puts you on your knees and calls his squad over to talk about it; the struggle breaks his hold, if you are quick.
- **Sound.** Rooms have their own air now. Every voice and every footfall is carried by the space it happens in: a tiled washroom rings, a plant hall swallows, distance takes the top off. Maps can lay their own beds of sound, near and far, steady or now and then.
- **Campaign Builder.** A Sign designer (plates, symbols, colours, pictures, a library, import and export, snapped to the wall). A Sound tool that places ambient emitters from the engine's own bank. Doors take a leaf (a steel grille that sight and sound pass through, or a painted panel), a label and a locked caption; standing water takes murk. Creatures take a hunter name and a prey, so a map can stage an animal that takes one man first and comes back for the rest. A live edit now rebuilds only the map it changed.

Verified with the full test suite (two older checks fail exactly as they do on the v1.3.0 build: a deck-over-door clearance case and a reactor lamp that hangs without a ceiling over it), the new hunter test (the bite, the feed, flight and return, save and load), the chapter route over all 170 passages, and rebuilds of the game and Campaign Builder engines.

---

## v1.3.0 - What stays on the floor (2026-11-10)

This is an update, not a patch. Until now the things down here did the polite thing when they died: they shrank, and then they were not there. They have stopped being polite. What you kill falls, lies where it fell, and takes its time leaving.

- **Deaths.** Every creature now dies as a body. It is thrown by whatever killed it, falls as a ragdoll, settles on the floor and stays there. After about eleven seconds it thins out grain by grain, and by fifteen it is gone. Nothing shrinks any more.
- **Squibs.** A round into flesh throws a burst of mist and drops that fly, land and dry where they land. Red for what was once a person; green for what never was.
- **Coming apart.** Bodies are built in sections. Heavy fire takes them off at the joint and a blast takes the whole thing to pieces. Each gap is closed with a cap of meat and bone computed for that cut, and a fresh cut pumps for a moment. This holds for all eight kinds of creature.
- **Wounds.** A body keeps the holes you put in it, and a pool spreads under it once it is still.
- **Materials.** The world answers a round by what it is made of: grey chips and dust from concrete, splinters from wood, clods from dirt, shards from tile, a white cloud from plaster, slivers from glass, lint that drifts down from fabric. Sparks are for metal and stone now.
- Both renderers draw all of it, Vulkan and the CPU fallback. The Campaign Editor's engine is the same engine, so custom maps die the same way.

Verified with a new death test over all eight creature kinds on both renderers (fall, rest, fade, despawn, squibs, sectioning and capped cuts under gunfire and blast, material hits), the carcass, creature-behaviour, creature-schedule, gameplay-fixes, chapter-route and campaign-map suites, and rebuilds of the game and Campaign Editor engines. Two older checks still fail exactly as they did in v1.2.19 and are not part of this work: a deck-over-door clearance case in the engine regression suite, and a repeated arc frame on the CPU renderer.

---

## v1.2.19 - Free hanging physics chains (2026-11-10)

The engine now supports free hanging metal chains as Verlet physics cables, including chains pinned to a living creature.

- New `CHAIN` map records and `kit::chain` / `kit::creatureChain` hang a steel cable with the same rope solver as electrical leads, without plugs or a live circuit.
- A chain can mount to an authored creature spawn: its anchor follows that enemy's facing frame (right / forward / up from its feet) so hanging links swing with the body.
- Chains draw as linked pale-steel segments. Existing lead cables are unchanged.
- Boss chain assets leave their chain bones at rest so the engine owns the hang instead of baked sway.

Verified with the custom physics parser (fixed and creature-hung chains), lead settle, and rebuilds of the game and Campaign Editor engines.

---

## v1.2.18 - Editor reliability repairs (2026-11-09)

This patch fixes editing, recovery, layer duplication and native cable-copy faults found while exercising the editor.

- Failed saves retain the previous file and exact recovery copy. Recovery-write failures are reported and can be retried. Opening, starting or importing another project resets its editing history and selection so Undo cannot restore the previous project under the new filename.
- Cancelled drags and drags returned to their starting point preserve the original clean state, change counter and Redo branch. Recovery waits for gestures to finish. The history slider clears obsolete object selections and ignores an unchanged position.
- Group rotation keeps rectangular props centred correctly and turns the climbing direction of stairs. Marquee selection follows rotated footprints. Rejected locked-group operations preserve the full Undo/Redo history.
- Invalid layer commands preserve the active floor and editable draft. Raising the top floor carries its existing roof clearance, keeping lamps and tall content below the roof.
- Duplicating or merging floors preserves automatic furniture ownership and room dressing settings. Native index maintenance no longer turns generated cables into user content.
- Cable copies use the engine's resolved sockets and terminal positions: copied owners receive the copied cable, while a cable copied without its owners becomes independent and leaves the originals untouched.
- Automatic floor finishes fill around narrow painted stripes without overwriting them or leaving gaps. Door clearance follows raised base floors. Non-solid and shaped decorations no longer count as full rectangular floor support or get cut by floor painting.

---

## v1.2.17 - Corridor placement repair (2026-11-09)

Corridors now route a complete two-metre passage between rooms instead of cutting a fixed L-shaped strip.

- Routing checks passage width and wall clearance, detours around authored furniture and uninvolved rooms, and gives identical results when dragged in reverse.
- Native wall panels are opened along the route. Wall geometry above and below the active floor stays intact; corridor walls stay within that floor's height band.
- Upstairs corridors have native walkable floors. Existing concrete, stripes and other materials are preserved, and authored ramp openings stay empty.
- Lighting fills coverage gaps without duplicating lamps. Repeating a completed connection makes no changes, and one Undo restores the whole operation.
- Managed furniture keeps the passage clear. Saved passage reservations follow duplicated, flipped and resized floors.
- Invalid endpoints, insufficient headroom and blocked routes leave the map unchanged. The guide explains the revised tool.

Verified with routing regressions, real editor operations, save/reopen and native game simulation walking exported corridors.

---

## v1.2.16 - Editable floors and native 3D handles (2026-11-09)

Floors now have a dedicated layer dock, and selection and resizing work in the plan and native 3D picture. The game engine also builds upper-floor walls from the saved map.

- Shift-click selects several objects. Move, copy and delete the selection with one Undo.
- Drag real footprint corners, pipe/cable ends and highlighted height edges. Raise a cramped catwalk roof without moving its deck. Materials on native walls and floors retain their metre-based scale.
- Drag props in 3D from the grabbed surface, with live movement and elevation changes. Hover outlines show what the selector will pick.
- Manage named floors, groups, duplication, merging, stack order, footprint transforms, locks, view masks and opacity in the right dock. Other floors have reference outlines. View settings save with the project and leave playable geometry intact.
- Floors accept native materials, including concrete upstairs. Local paint drags create stripes without repainting the surrounding floor. Open-space/erase tools cut the actual floor for ramp gaps, and ramp landings open automatic guardrails.
- New floors reserve standing headroom. Player starts are placed on the selected floor. Upper '=' remains walkable deck, upper '.' remains empty, and upper '#' now has native wall rendering and collision.
- Add maps by choosing their adjoining edge. Occupied edges are protected, a floor opening joins the maps, and the view switches to the new map. Distant custom maps are excluded from rendering.
- The renderer clears cached geometry when replacing a game session. Edits and entering Play no longer risk showing the previous scene.
- The custom-map guide explains the controls, floor semantics, roof clearance and export workflow. The matching game download is updated as well.

Verified with editor mouse/command interaction tests, native rendered movement and layer-view checks, and an editor-authored export loaded and walked by the game engine.

---

## v1.2.15 - Live editing and room dressing (2026-11-09)

Moving or deleting furniture now updates the live 3D picture, including loose props.

- Autodressing has a visible on/off setting. It starts on and remembers your choice.
- Redressing replaces its own generated furniture, floors and lamps while leaving user content alone. All older maps are treated as user-generated. Editing an automatic item makes it yours.
- Drawn walls take priority over intersecting objects on that floor. One Undo restores the wall operation and removed content together.
- Room dressing respects the actual room shape, rotated model sizes, occupied space and doorway access. Repeated dressing and lamp addition no longer pile up duplicates.
- Dressing uses room names and authored furniture as context, pairs chairs with desks or sofas, and leaves usable space in front of equipment.
- Committed edits repair managed arrangements locally. Valid pieces stay put, repeated updates settle to identical saved content, and deliberately deleted items do not immediately respawn.
- Relighting keeps user lamps and other floors intact. Floor painting preserves neighboring rooms and floors.

Verified with rendered movement/deletion checks, creation and ownership regressions, the 56-check authoring suite, Starter save/reopen, simulation, and the 1,729-record Starter reconstruction.

The separate game download is unchanged.

---

## v1.2.14 - Look works in the picture (2026-11-09)

Right mouse on the 3D picture looks around again. The picture was ignoring the mouse.

The game download beside it is unchanged.

---

## v1.2.13 - New campaign stays open (2026-11-09)

New campaign opens a blank map and stays in the editor.

The v1.2.12 editor download was missing the soundtrack list, so New campaign came back to the title. That list ships with the editor now.

The game download beside it is unchanged.

---

## v1.2.12 - New campaign stays open (2026-11-09)

New campaign opens a blank map and stays in the editor. It no longer drops back to the title.

The game download beside it is unchanged.

---

## v1.2.11 - The editor download opens the editor (2026-11-09)

The campaign editor download is the program that opens the editor. New campaign, a saved project, and Play live in that exe.

The game download that ships next to it stays the game. The editor is not packed into that exe.

---

## v1.2.10 - The rooms were there (2026-11-08)

Opening a level from the official campaign showed an empty corner. Those rooms sit underground, and the picture was the empty space above the ceiling. The walls, lights and furniture are in the picture now.

An official level stays in Open from campaign. It does not appear on the title as a project.

---

## v1.2.9 - One program (2026-11-08)

The Campaign Builder and the game are one program now. An edit goes straight
into the running world instead of round-tripping through files, so what you
draw shows up in the 3D view in a couple of frames.

---

## v1.2.8 - The Campaign Builder (2026-11-08)

The Raw Metal Campaign Builder has been rebuilt for people who have never
opened a level editor, and for people who opened too many.

- **Paint on a black plan, build in the live view.** The plan is a black
  sheet with bright lines: white walls, yellow doors, cyan lamps, green
  people. F2 paints in 2D, F4 builds in 3D, F3 shows both. The tools are big
  tiles on five pages (Build, Decor, People, Logic, World), and every one
  shows a ghost of what it will make before you let go.
- **Rooms that dress themselves.** Drag a box and it becomes an office, a
  lounge, a store, a lab, a workshop or a plant room, with walls, a lamp, a
  door onto the floor next to it and a few things left in it. Draw any
  outline and raise it into walls, a block or terraces; brush the floor up
  into steps; join two rooms with a corridor.
- **Paint and parts by picture.** P paints real materials onto whatever you
  touch. The parts browser shows every model and shape as a picture.
  Furniture backs onto the wall it lands near, and V swaps it for another.
- **Lamps you can play with.** Pick a lamp and drag its brightness, reach and
  warmth; the room relights while you drag. Moods (cosy, eerie, alarm, cold
  lab) set a room in one click.
- **Watch it, then walk it.** F6 runs the map where it stands: guards walk
  their rounds, triggers fire, and F6 again puts everything back. F5 drops
  you into it; Escape brings you back to the same spot.
- **The campaign is yours to remix.** Open from campaign brings any released
  level of DepthWorks into the builder, the way it shipped. Change it, save
  it as your own map, or save it into your game and play it there.
- **Nothing lost.** Every step can be undone or scrubbed back through.
  Unsaved work comes back on its own after a crash. The Starter Map still
  opens and saves exactly as it was.

---

## v1.2.7 - The Campaign Editor (2026-11-07)

The map editor is out, and it speaks fluent `.rmc`.

- **DepthWorks Campaign Editor** (separate optional download,
  `DepthWorks-CampaignEditor-v1.2.7.zip`): build your own campaigns in a
  paint program, not a spreadsheet. Drag a room and the game walls and
  floors it for you; paint floors, walls, lamps, signs, stairs, people
  and monsters; furnish a room in one click; then walk inside what you
  made and shoot the 📸 ENGINE SHOT to see it exactly as the game renders.
- It opens the shipped campaigns losslessly — the Starter Map's 150 maps
  and the Ashfall Exclusion Zone load, edit and re-export with every
  record intact — and its 3D view draws the game's real materials,
  models and terrain heightfield, so what you see is what players get.
- 445 models and the game's own finishes are in the Parts palette;
  outdoors, paint ground cover and flatten building pads on real hills.
- Export `.rmc` into the game's `custom maps` folder and play it from
  the title screen.

---

## v1.2.6 - RawMetal Campaigns (2026-11-07)

Custom maps get a file type of their own.

- Custom maps are now `.rmc` files — RawMetal Campaigns. The Starter Map
  ships as `Starter Map.rmc`, the title screen asks for `.rmc` campaign
  files, and the map installer writes `.rmc`.
- Inside, an `.rmc` is still the same readable text: open it in Notepad
  and edit it by hand, exactly as before.
- Old maps keep working: plain `.txt` maps still load, and renaming a
  `.txt` map to `.rmc` keeps its saves — progress follows the map through
  the rename. If a `.txt` and its renamed `.rmc` sit side by side, the
  game lists the map once.
- `GUIDE.html`: rewritten quick start for `.rmc` — opening maps in
  Notepad, and turning a fresh Notepad file into a campaign by renaming
  it from `My Map.txt` to `My Map.rmc`.

---

## v1.2.5 - Light and shadow (2026-11-07)

Lamps now behave like real fittings, and bright light blooms.

- Ceiling lamps shine downward: they throw pools of light on the floor and
  scallops down the walls, and the ceiling between fittings stays dark.
  Wall lamps throw their light into the room. Bare bulbs still light every
  direction.
- Light shafts in hazy rooms follow the same cones, so a downlight shows a
  visible beam to the floor.
- New bloom: a soft, wide glow that spreads at several sizes around
  anything bright, instead of the old ring of points.
- Lamp fittings glow in their lamp's own colour and are bright enough to
  bloom.
- Custom maps: an optional 28th `MAP` field sets floor wetness (0 to 1).
  Wet floors get puddles and a sheen that reflects the lamps.
- Starter Map: the Lights hall has five warm downlights along the corridor,
  booth colours that spill further across the floor, a light haze and a
  wet floor. `GUIDE.html` covers the wetness field.

---

## v1.2.4 - Glass and concrete (2026-11-06)

Glass breaks like glass, and custom maps get a brutalist building kit.

- Glass cracks where you hit it: every round leaves its own spider-web, and
  the cracks reach further as the pane weakens.
- A pane that gives way fractures from the hole outward. The centre blows
  out along the shot, the rest of the web hangs in the frame and lets go
  piece by piece, the shards land and stay on the floor, and jagged teeth
  stay standing in the frame.
- The Chimera no longer comes apart at the waist: its torso is hinged where
  it sits on the horse body and skinned across the join.
- New `PRIM` record for custom maps: 35 ready-made building parts (stairs,
  ramps, round columns, arches, barrel vaults, domes, struts, doorframes,
  ribbon windows, fins, canopies, slits, roundels, lattice screens, waffle
  slabs, bridges, stair cores and more). Curved parts are true shapes with
  collision built in, and every default is sized for the player: 0.17 m
  risers, 1.2 x 2.2 m doors, 1.1 m guard rails, arches you walk through
  with headroom, screens you see through but cannot pass.
- STRUCT materials 40-51: formwork, board-form, cast, weathered, stained and
  ribbed concrete, tread plate, rusted steel, metal roofing, asphalt, dark
  floor tile and plank floor.
- Starter Map: hanging and climbing ivy on the facility instead of bushes,
  round columns under the entrance canopy, and two Architecture rooms
  showing every PRIM part and material.
- `GUIDE.html`: the PRIM record, a table of every part with its default
  size, and the player's scale to build to.

---
## v1.2.3 - The facility in the woods (2026-11-06)

The Starter Map is now one building in a forest.

- Every room of the Starter Map sits inside one massive brutalist concrete
  facility: stained cast-concrete walls, deep ribbon windows, floor ledges
  and a parapet going back to grass and ferns, stair towers at the corners
  with the site emblem, and sky bridges between the wings.
- A pine forest rings the whole building, in dark blue-grey fog. You start
  outside, at the front door.
- Every way out of the building has a landing and two low steps, so you walk
  in and out without jumping.
- The Leyline Sanctuary sample campaign is gone.
- `GUIDE.html`: the indoor-to-outdoor advice is corrected (terrain walks half
  a metre above its TERRAIN_FLAT height and the player only steps up
  0.215 m, so build the difference as steps), with a note on which outdoor
  sky to pick.

---

## v1.2.2 - Wires and switches (2026-11-06)

Custom campaigns can think a little harder.

- An event can now wait on several things at once, all of them or any one:
  `SCRIPT_REQUIRE` adds gates that compare a state (`=`, `!=`, `<`, `<=`, `>`,
  `>=`) or a live value: creatures killed or still alive on the map, the
  player's health, the map they are on.
- Counters: `ADD` adds to a state, so "find five crystals" is one rule.
  `TAKE` takes quest items back.
- `MESSAGE` puts a caption on the screen, with live numbers in it:
  `RELAYS {relays} / 2`.
- `SCRIPT_TRIGGER` says where an event listens (its box, its whole map, or
  every map) and whether it fires once or each time its gates come true.

- The Starter Map has a new room, **Scripts**, east of Levels: a working
  station for each of these (counter pads, a vault with two locks, either-of-two
  switches, a doormat that counts visits, a creature pen, a gas strip that
  warns at low health, and a switch heard by another map).
- `GUIDE.html` has a new chapter, *Scripting: making things happen*, with a
  diagram, common patterns and the vault's records to copy.

Every map made before still plays the same.

---

## v1.2.1 - Out of doors (2026-11-06)

The custom maps kit, finished.

The Starter Map has grown four places:

- Levels, east of Life: a deck over the hall, a stair up to it and a hole to
  look down through. One map, two floors.
- Ground paint, Terrain and Grove, out past the last row of models: real
  ground under a grey sky, every kind of ground cover, a plateau and a road
  levelled into the hills, and trees standing where they grow.
- Seams: where one map meets the next, marked on the ground and signed,
  both ways, and an edge that goes nowhere.

The guide has three new chapters with drawings: stacking floors in a map,
stitching maps together north-south and east-west, and outdoor maps with
their terrain, ground paint, horizon woods, storms and shelters.

Two wall panels, a column and a vending machine that stood as black
shapes now show their paint.

The great door is still shut.

---

## v1.2.0 - Open house (2026-11-06)

A feature release: custom maps, for anyone.

The download now unpacks with a custom maps folder beside the game. Drop a
map file in it and it appears under CUSTOM MAPS, wherever the game is
started from. Inside the folder:

- Starter Map: a walkable debug world. Every model, prop, lamp, hazard,
  creature and body the engine can place stands in rows on its own plinth,
  signed with its ID. Stand beside anything and the HUD names it.
- GUIDE.html: how a map file works, with diagrams, the record reference,
  every model's size and where to find it in the Starter Map.

A map that fails to load is listed in RawMetal-custom-maps.txt beside the
game, with the line it failed on.

The great door is still shut.

---

## v1.1.17 - Head room (2026-11-06)

A patch. Some curtains and partitions down below would hold a person lying
flat along their tops, right up under the ceiling tiles. They will not now.
A row of numbers painted over a row of doors was painted backwards; someone
has gone over them again.

The great door is still shut.

---

## v1.1.16 - Settling (2026-11-05)

A patch. Nothing you will notice on the way down, and a good deal you will
not: things tightened, things put back where they belong, a few lights
rewired. The plant is quieter for it.

The great door is still shut.

---

## v1.1.15 - Live wires (2026-11-05)

Current has weight now. A live floor spits sparks that fall, bounce off the
grating and cool from white to red where they land; the arcs that jump across
it fork and crawl toward the nearest metal, and every time one strikes, the
room flashes blue. A round that hits steel throws a shower of its own.

The floor under her says how loud she is. Grating rings, gravel crunches,
grass and dirt keep quiet. Running carries a long way; crouched, she is
almost nothing. Five marks at the foot of the screen show what the last step
gave away.

The things down here have learned to look the way the guards do. In the dark
they need a moment to make out what they are seeing, though a step closer
than that is too close. You will hear most of them before you see them.

Some cabinets, dials and clocks that came out black now show their paint.

The great door is still shut. Something behind it has been plugged in.

---

## v1.1.14 - Over the shoulder (2026-11-04)

Seen from behind her (the THIRDPERSON console command), she is a person
holding a gun now. She brings it up when you aim and the view closes in over
her shoulder; it kicks when she fires; she reloads it in her hands. Standing
still she lets her body lag the way she is looking and steps round when she
has turned far enough. Against a wall the view no longer ends up inside it.

---

## v1.1.13 - Two at a time (2026-11-04)

She handles the shotgun the way it is meant to be handled now: her left hand
works the pump, her right closes on the stock with a finger on the trigger,
and the gun kicks and is racked with the weight of a real one. Bring it up
with the right mouse button and the sights settle on what she is looking at.

She reloads it shell by shell, two to a hand, and racks it when the tube is
full. Pull the trigger with a shell inside and she stops loading and fires.

Her hands and the guns in them take the light of the room she is in: brighter
under a lamp, dark in the dark.

---

## v1.1.12 - Her size (2026-11-04)

She carries the shotgun now the way it is carried: low on the right, the stock
tucked under her shoulder and out of the way, the muzzle on what she is looking
at. Her right hand wraps the narrow of the stock with a finger on the trigger;
her left closes on the wooden pump and rides it back and forward when she
works it. The gun and her arms are her size, the same as the legs and coat
under them.

Hold the right mouse button and she brings it to her eye: square to the view,
the bead on whatever is in front of her. Let go and it drops back to the hip.

Leaving for the title from the pause menu no longer closes the game when the
click lands where the title's QUIT is.

---

## v1.1.11 - Close in (2026-11-04)

The shotgun comes in closer, as close as her fists, and her arms with it.
Her left hand no longer turns itself palm-up under the fore-end: it takes the
wood the way the arm brings it there, so the elbow stays straight and nothing
snaps when you turn. The pistol is held the way it was.

---

## v1.1.10 - Shouldered (2026-11-04)

The shotgun sits the way she would carry it: turned in toward where she is
looking, low on the right, the stock out of the way. Her right elbow comes out
to the side and her hand closes round the stock instead of folding up under
it, and her left arm reaches straight along the fore-end, glove and all, with
no wrung wrist. The turn of each hand is carried along the forearm, the way a
forearm turns.

---

## v1.1.9 - All of her (2026-11-04)

Look down and she is there: coat, belt, legs and boots, under your own eyes
instead of a step ahead of them, walking when you walk and turning when you
turn. Her shadow goes with her, one shape on the floor under the lamp.

She kneels now. Hold crouch and she goes down on one knee, rifle up, and the
view sinks with her to where her eyes really are. Where a kneel will not fit,
under the ducts and through the low gaps, she goes down on her belly and
crawls, and gets up on her own when there is room again. Z puts her flat
anywhere; a jump or a run brings her back to her feet.

A desk hides a kneeling woman's legs and nothing else. Get lower.

---

## v1.1.8 - Steady hands (2026-11-04)

Her wrists no longer wring themselves thin where the glove meets the skin. The
hand turns on the arm the way a hand does, and the arm keeps its shape doing it.

Her right hand closes round the stock behind the trigger guard instead of
hanging underneath the gun.

---

## v1.1.7 - In her own hands (2026-11-04)

Look down, and there is someone there. The hands on the shotgun are gloved
and banded at the wrist, they close on the grip and the slide, and when you
look at your feet you find legs under you. She stands as tall as she always
said she did.

There is a sidearm in the plant now, and boxes of nine millimetre for it, though
not anywhere tonight's work has taken you yet. The count at the corner of the
screen is for the gun in your hands and nothing else. In the Ashfall Exclusion
Zone the rookies by the fire will hand you one already drawn.

Doorways no longer catch on a shoulder that was nowhere near the frame, and
the trim around them stopped pretending to be a wall. Rails and thresholds lift
your feet instead of stopping your shins. The thing in the cold hold is broader
than it was, it does not walk through the people it is chasing, and it does
not stop at the receiving gate any more. Several of the plant's other residents
have found a quicker pace. Nothing tells you it is coming now except the way
it moves and the sound it makes.

There is a way back up out of the Switch Hall pit, the inspection stairs are
clear of the track above them, the fence panels at Research Receiving hold, and
the lamp in the prologue lift rides with the cab the whole way.

The great door is still shut.

---

## v1.1.6 - Facing the room (2026-11-03)

Clocks that had been hung with their faces to the wall have been turned round,
and so have the control boards, gauges, loudspeakers, fridges, sinks, cabinets
and screens that kept them company. Desks show their drawers to the chair
pulled up to them. Chairs sit at their tables instead of turning their backs on
them, and the ones along a wall look out into the room. The coffee machine in
the security office finally shows its buttons to the night shift.

The washroom stalls have their paint back, a wall clock that hid behind a
door frame has come out from under it, and a rack of gas cylinders that stood
in the dark now has a work light hung over it.

Before anything ships now, every room in the plant is checked for furniture
facing the wrong way.

---

## v1.1.5 - Things left behind (2026-10-31)

The things left on desks and shelves across the plant can be picked up, knocked
aside, thrown and broken. Blasts move them, doors push them aside, and the people
walking through a room can kick them. What lands or breaks makes a noise they
can hear; a squad recognises the noise its own men made.

Ordinary glass breaks a pane at a time. Protective glass still stops bullets,
but its cracks fade away twenty seconds after the last hit. Rooms behind glass
are easier to see, and recessed lights no longer shadow their own fittings.

Behind the great door, chairs face their tables and ward beds face the equipment
at their heads. The Chapter 2 seal stays shut until it is meant to open.

The map editor also supports model props and breakable surfaces. Saved damage,
loose objects and their positions survive saving and travelling between rooms.
Saves made in this version will not open in older versions.

---

## v1.1.4 - Heavier (2026-10-31)

Chapter 1 is as you left it.

Behind the great door, every room past it has been torn out and built again to
the size of the people who worked there: a desk where a clerk sat, a bay where
a patient waited, a corridor two people could pass in. The door is still shut.

Underneath, the plant has learned what things weigh. Something left on a desk
can be knocked off it, thrown about as far as its weight allows, or shot apart,
and whatever breaks makes a noise that carries. Glass can be broken a pane at a
time, and the panes that were put there to stop a bullet only crack. None of it
is set into the rooms yet. That comes next.

Saves made in this version will not open in older ones.

---

## v1.1.3 - Travelling light (2026-10-30)

DepthWorks.exe has gone from 63 MB to 47 MB, and nothing in it looks or sounds
any different.

Every voice keeps the bandwidth of a Half-Life 2 voice at half the
bytes. Footsteps, doors, the lift and the creatures' calls are packed tighter,
and the engine trims each one back to its exact first and last sample, so a shot
lands when you fire and a hum loops without a gap. Bodies move within a
millimetre of where they were made to. Most of the plant's surfaces are stored the way photographs
are, but each one was held to a bar where the difference cannot be found by eye,
and anything with writing on it, a face in a frame or a painted sign was left
exactly as it was.

The picture also costs the graphics card less: one of the last steps of every
frame was filtering the same pixels twice, and now does it once.

---

## v1.1.2 - Furnished (2026-10-30)

Chapter 1 is as you left it.

Behind the great door, someone has been putting the rooms back the way they
were on the night: the chairs where people sat, the paper where it fell, the
pictures the company hung where its visitors waited. The door is still shut.
You will see them when it opens.

---

## v1.1.1 - Their own proportions (2026-10-30)

Things in the plant were the right shape from one side only. The crate under
the power house tanks was squashed flat, the switch cabinets there and in the
machine hall were swollen, the mesh fences in the maintenance yard and on the
perimeter were stretched, and the ladder in the water works was one flight
pulled to twice its height, with its rungs a stride apart. Every prop in
Chapter 1 now stands at the shape it was made in, from every side you can walk
around, and the plant's own inspection will not let one stretch again.

At the far end of the metro platform, the map board's steel backing sat in the
same place as the map. Walk behind it and the map showed through the steel like
a ghost. The backing now stands clear of the board.

The great door at the bottom of the chapter is still shut. It will open.

---

## v1.1.0 - The Ashfall Exclusion Zone (2026-10-29)

Chapter 1 is as you left it. This update is for the Exclusion Zone, the open
ground past the wire where the engine is tried before it goes underground.
Start it from the custom campaigns.

The people out there talk now, and what you say to them stays said. The elder
at the rookie camp has work for you and an opinion about storms. What is left
of a Cleanup squad holds the old mine on a truce: keep your gun low and walk
through, or tell them what their own people sent them in for and find out what
four tired men can still do. Bring them a way out and they will walk you to it.
The scrapyard charges a toll. Pay it in the soldiers' rations and the yard is
yours; walk in without paying and it is not. A squad on a truce warns you once
if you hold a gun on it. The second time it does not warn.

Every few minutes the siren goes. The haze turns brown, the sky comes down, the
ash pours, and the open ground burns. Get under a roof. The first storm wakes
the transformed, the people the zone kept, sleeping under the burned village,
by the reservoir and down the mine shaft. They run as fast as you walk. They do
not care whose side anyone is on.

The scrapyard is a shanty yard, a train rots on a siding by the east road, the
radio tower stands behind a fence, and someone has been fishing in the swamp.

The ? and ! over a guard's head are about half the size, and no longer swell
into red banners across a yard.

---

## v1.0.4 - No longer frozen at the door (2026-10-29)

For the first five seconds after you started DepthWorks there was nothing,
and for the next seven a full-screen window Windows called not responding,
while the plant loaded behind it. It looked like a crash. The loading now
happens behind a screen that says so: the name, what is loading and a bar
that fills, from the first fraction of a second. The window answers Windows
the whole time.

The PC or VR question that opened the game is only asked when SteamVR can
see a headset. Everyone else goes straight in.

---

## v1.0.3 - The night desk (2026-10-28)

Since the office became part of the plant, a second computer had been sitting
on your security desk with its back to you, hiding the screen you work at.
It is gone; the desk's own terminal faces your chair again. When the lift
cable goes, the cab light dies instead of turning into a red sheet across the
ceiling, and one small emergency lamp by the door is all that stays lit.

---

## v1.0.2 - The lights on, again (2026-10-28)

A second walk through the plant, this time with a record of what each fix
looks like before and after.

The feeder isolator in the sump works, the breaker panel, the ammeter and the
fire cabinets in the water chapters were hung on their walls facing into the
brick; you saw their backs. They face the room. The specimen crate on the
Research Receiving lift, the transfer cage and the mesh fences were black in
a lit room and show their metal and rust. The big painted steel of the supply
main, the exchanger shells and the crane girders was one flat colour and has
grain and wear. The brown grating that showed on walls above the decks is
the wall finish of its own hall. The menu says what build it is.

---

## v1.0.1 - Things that were not where they should be (2026-10-28)

A walk through the plant with the lights on, looking for what is wrong.

The Foundry's supervisor had two windows onto the floor, built into a solid
wall; they are open now. The firing-lane barriers the Cleanup Force dragged
across the Energy Research forecourt were drawn as a bar on a post and you
could see through them; they are walls again. The fire cabinet in the heat
exchange vestibule hung in the doorway with nothing behind it and is on the
wall. In Medical and the ready room the wooden tables and chairs went black as
you walked up to them; they have their wood back. The crashed lift in the
Security Office no longer opens onto a second, empty office underneath. The
tool crib's mesh no longer stands half across a doorway, and a steel jamb in
Decontamination stopped shimmering.

---

## v1.0.0 - DepthWorks, Chapter 1 (2026-10-28)

The first chapter of DepthWorks, finished. There is no changelog here. This is
what the chapter is.

**The plant.** You are a night-shift guard at the Depthworks plant, built over
something the company found deep underground and sells as power. Tonight the
service lift drops you into the sub-levels your badge does not open. The
lights are off, Order 7 has sealed the doors, the Cleanup Force is sweeping the
plant to remove witnesses, and the last message from your friend at Lab 4 was
cut off mid-line. The workers the lockdown did not reach are no longer workers.

**Chapter 1: the power plant and the perimeter.** You bring the plant back from
a black start one district at a time, and each district opens the next.

- Power Distribution: the switch hall, the cable trench, the emergency
  generators, and Load Control, where the mail relay wakes with one queued
  message.
- The Machine Hall: the galleries around the turbine floor, the exciter bay,
  the crane end, and the restrike that lights them.
- Ventilation Works and the Air Handling crawlspaces.
- Water and Cooling: the treatment hall, the drainage gallery, the sump works,
  the cooling tower basin and shell, and heat exchange.
- Maintenance: Central Stores, the Fabrication Shop, where the shutter to
  Operations needs a pin you machine yourself, and the Aid Station.
- The Restricted Perimeter: the Operations corridor, the Security Perimeter's
  yard and gatehouse, Checkpoint Delta's holding cells, the Administrative
  Link over the old ore pass, Personnel Transit's held tram, Restricted
  Utilities, Research Receiving, Decontamination, and the forecourt of Energy
  Research, where the chapter ends at the great door.

**What is in the dark.** Huntsmen and xenowasps in the ducts and galleries;
the transformed, who keep their habits; the Snatcher, which waits for you and
does not chase; and the Chimera, which does not wait.

**The Cleanup Force.** They hold the checkpoints, and they are not stupid. A
man who glimpses you does not know what he saw: the "?" over his head fills as
he works it out, and the "!" means he has you. He reaches for his radio, and
you have a second and a half to stop the call. Lose him and his squad hunts
you (EVASION), then stays wary for a long time after (CAUTION). Keep to the
dark, keep low, keep still; knock on a wall, throw a can; take a man from
behind and drag him out of sight, or stuff him in a locker. Hide in one
yourself and watch the room through the holes in the door, and pray nobody
saw you climb in. Patch the perimeter cameras through to your handset and you
can see where every man is looking. Or fight.

**Controls.** WASD move, Shift sprint, Space jump, C or Ctrl crouch, mouse
fire, right mouse guard, R reload, E use (doors, terminals, lockers, bodies),
F flashlight, Q subdue from behind, I inventory, Esc menu. Inside a locker, E
steps out and the left mouse button knocks. Checkpoints are automatic; manual
saves are in the menu. Saves and settings live in %LOCALAPPDATA%\RawMetal.

**Needs.** Windows 10 or 11 (x64), a GPU with a Vulkan 1.0 driver, and the
Microsoft Visual C++ x64 Redistributable. Unzip anywhere and run
DepthWorks.exe; nothing is installed.

**VR (experimental).** SteamVR with Quest, Index or Vive controllers: real
hands, the shotgun in your fist, an Alyx-style interface. Lockers and the
camera patch are desktop-only for now, the reload has no animation yet, and
Quest frame rate is below 72.

Chapter 2 continues below the great door.

---

## v0.14.6 - The lag (2026-10-27)

The plant stopped stuttering when something hunted you. A creature that had
lost its way used to work out every deck of the room again, from scratch, each
time it wondered where you had gone, and every creature that heard the same
noise wondered on the same frame. Six of them on the Foundry's gantries could
cost a frame three times over; standing on a tank where they could not reach
you cost it every second and a half. They remember the room now, and share
it. The guards' fallback routes, the plant's question of which halls to keep
loaded, and their route maps on arrival were doing the same and have stopped.

The guards in the Security Yard who had you and never fired were stuck
walking in place against cover, and a man walking does not shoot. Now a man who is
moving and has a clean line takes it. A flank he cannot reach in two seconds
he gives up. A hunt that has lost you lasts at least twenty seconds, and the
EVASION clock says so honestly. A man who hears something stops for a beat
before he turns.

Lockers: the doors are perforated steel all the way through, and from inside
you are looking through the real doors, not a picture of them. Your hands come
up to the doors before they move, and take a stowed man by the collar and lift
him in where you can see it. Whoever opens that locker gets him falling out at
his feet. Burst out and only a man the doors actually reach goes down, and he
goes down in front of you. Pulled out, you get the pistol in your face for a
moment before he uses it.

Being seen has music now: the score gives way to a hard loop the moment a '!'
goes up, holds through EVASION, and lets go when they give up.

---

## v0.14.5 - Saw that (2026-10-27)

The rest of the Cleanup Force's training: what they do after they lose you,
and where they look.

When a guard who had you loses sight of you, the hunt is not over and the
screen says so: EVASION, in red, with the time his search has left. His squad
keeps the floods on while they look. When the search gives up it becomes
CAUTION: for forty-five seconds they make you out sooner, see a little further,
sweep wide at any noise and do not linger at their posts. ALERT means one of
them has you, and a man in Alert keeps the eyes he found you with.

Being seen has a sound now, and so does being half seen. The "?" over a man's
head fills as he works out what he is looking at, and the "!" lands with a
sting. A guard who has you reaches for his radio; you hear him key it, a
handset glows over his head, and you have a second and a half to stop him.
Looking away does not stop him talking. The floods wait for the call. A man who
finds a body calls that in too, unless you get to him first.

Perimeter Ops has a patch desk at the end of its camera bank. Put the plant's
cameras through to your handset and the map in the corner shows the men in
whatever room you are in: where each one stands, which way he looks and how
far he can see. Pale, he is walking his beat. Yellow, he is not sure. Red, he
is sure. The headset has no map yet, so for now the patch is for the screen.

The steel lockers are the rusty double-door kind now, every one of them, and
you can get into any of them. Your hands take the doors, you back in and pull
them to, and the room goes muffled and small behind a band of holes punched in
the steel at eye height. Anyone coming shows through it. Knock on the door and
somebody comes to see; knock twice and he opens it. Drag a man you have put to
sleep to an empty locker and he goes in instead of you.

A guard who watched you climb in says so, walks over, takes the handle and
drags you out into the light, and racks his gun in your face and gives you a
moment before he uses it. A man who heard the doors, or whose search for you ends
beside a locker, stops, turns to it and looks inside; on Caution some of them
check on a hunch. Whoever is checking the doors when you burst out, and is not
ready for you, takes them in the face.

Smaller: E on a terminal you are facing beats a body lying at its foot, and
the prompt says when E will drag a body, drop it or stow it. Alert calls are
made per squad, so a second squad that sees you says so.

---

## v0.14.4 - Lab coats (2026-10-27)

The plant had one set of clothes. Everyone below the security office wore the
heavy radiation suit, the aid station's doctor included. The research staff
have their own now: two men and a woman in lab coats over what they came to
work in, a doctor with her stethoscope still on, and a technician in the light
hazmat suit and respirator, from the same character packs as everyone else and
moving the way everyone else does. The technician sealed in Decon 1 wears his
suit, the body in the Energy Research firing lane wears a coat, and the aid
station's doctor went down between the cots and her desk.

---

## v0.14.3 - The shaft door (2026-10-27)

A playtest patch, from the second night's reports.

The fall is not a cut any more. The Security Office stands on the plant's own
grid now, nine metres over the Foundry, and when the cab hits the bottom of the
shaft the game does not stop and start again. The dust settles, the cab's south
door gives, and a service passage under the office runs out into the Foundry's
lift alcove. You walk it. Nothing loads.

Loading a save no longer hangs the window. The plant is rebuilt on a second
thread behind a dark screen and a bar that fills as it goes, and Windows stops
asking whether the game has died. That question was the crash: a load took long
enough that a click told Windows the game was frozen, and Windows closed it. The
save menu reads a small label written beside each save instead of rebuilding
every saved game to print one line of it.

The freezes at the end of the freight lift, in Receiving Lanes and in the
warehouses were ours, and came in with the paper. A note taped to a wall was
left out of the plant's collision index without being counted, so on any map
with one the index was thrown away and rebuilt on every question asked of it:
a minute and a half to light the receiving hall, six to reach the warehouse
blocks, and every guard paying for it every step. It is counted now. Nobody was
taken out of Receiving Lanes; all six of the detail together cost less than a
millisecond.

Anything that cannot reach you has stopped working out the whole plant again
every frame to be sure. It wanders, a cell at a time, and asks again later.
Guards planning a route through doors stopped copying the entire world to do it.

The black walls in Emergency Generation were the next hall not being loaded
until you stood within six metres of it. An open seam is a window, and what is
through it is kept now, there and at every other open seam in the plant. Two
low red lamps light the gallery's west wall before the sets are running.

The Cleanup Force can be crept past. A guard who glimpses you does not know
what he saw: a yellow "?" over his head fills while he works it out, and a red
"!" means he has you. Within five metres nothing has changed. Beyond that,
distance, a crouch, keeping still, the edge of his vision and shadow all buy
time, and the five pips under your health are how lit you are. A glimpse he
half made out, he walks over to check, still unsure and still open to a choke
from behind. When he does have you he gets on the radio, and his squad hears
about it a second and a half later, unless you drop him first. A body can be
taken by the collar and dragged out of sight (E on a knocked-out guard, E again
to let go), and a man has to see one fall to call it in. A choke is heard
across a room now, not across the yard. E against a bare wall knocks on it,
and a thrown can no longer gives away the hand that threw it.

The Security Yard was built to be crept through and could not be. The detail
takes longer to make out a shape at the end of the yard. The lane man turns his
back on the portal for a moment; Sentry 1 holds longer on the lane and there
are higher sandbags on his east side, so a crouched player along the south wall
is under his line; the heavy turns to watch the gatehouse with the gate behind
him; the lee of the van by the west wall is dark; there is a stack of crates
between the nest and the gatehouse to break his view; and the Lock 2 bell is
heard across the west half of the yard now, its lamp coming on while it rings. In the gatehouse the desk man watches his monitors with his
back to the door, the bunk-room sharpshooter watches the beds, and there is a
second way in, the smoking door on the south side, under a lamp that is out.

The Snatcher had black holes in it: the gaps between its texture islands were
black and bled through. They are filled. It also had a fit. Watched from across
a room it slipped away, came back, was watched, slipped away, forever, and it
looked for cover forty times a second while it did. It picks its cover once
now, waits there, and the second time you stare it down it stops caring and
comes anyway. With nowhere to hide it does not stand frozen in the open.

The Chimera hunts. It stares, circles to the side you are not watching and
comes at four and a half metres a second, and it takes a third as much again
to kill. It moves like something with four legs: its gait is taken from a
wolf's (Quaternius, CC0), under a torso that hunches and claws ahead of it.

Central Stores has a sling test bay. The test load is a tonne of timber on
skids: lean on it and it goes, and it does not stop until something stops it.
Get it onto the scale and the cage opens. The winch pendant drags it home if
you wedge it.

Smaller: the door switches beside the bulkheads sit on the wall instead of in
the air. The Foundry's shift log and containment note are paper. The drainage
gallery's handwheels are on their stems. The man in the aid station lies on his
bed.

---

## v0.14.2 - Paper you can pick up (2026-10-26)

A playtest patch, from what the testers walked into.

Paper is paper now. A log book, a ledger, a manifest or a note left on a bench
used to open the same green screen as the plant's computers. It lies on the
bench as a sheet, and reading it puts the sheet in your hands: typed for a
printout, in pen on ruled paper for a log or a note, in marker for a sticky
note. The computers keep what a system writes: status, alarms, the PA, the
badge run, the mail relay, a message thread.

What happened tonight is no longer bolted to the walls on enamel plates. The
roof fall in the deep cargo line, the depot lockdown, a jammed shutter, the
condemned footbridge, the welded door at Checkpoint Delta, a sealed vehicle
lock: each is a sheet taped up by whoever closed it, on the wall or on a
crew's barrier, and you read it by walking up to it. The plates keep to what
will still be true next year.

No sign tells you which key to press any more, and no place is called by its
number in our own manifest: what the game announced as "10 SURFACE LIFT" and
"12A R-03 CONTROL" are the Surface Lift and R-03 Control.

Clutter was laid out in a straight row in the reactor complex and on the depot
platform. It is dropped, kicked and left where people stood.

The map audit refuses each of those from here on: tonight's news on a
permanent plate, a sign that names a key, a place titled by its manifest
number, and four pieces of clutter in one straight row.

---

## v0.14.1 - Developer mode gets through the chapter (2026-10-26)

A playtest patch. Developer mode (`--dev`, or DEV in the console) counts
nothing as a hostile now, so the doors and script gates that ordinary play
opens by clearing a room let it through. Before this a tester in developer
mode reached the first enemy-clear door and stopped: the creatures were gone,
but the Cleanup Force guards still held the gate shut. The guards themselves
are unchanged, so stealth, takedowns and their AI are still testable there.
God mode (GOD) still means no damage and nothing else; the console now says
which of the two opens the gates.

---

## v0.14.0 - DepthWorks, Chapter 1 playtest (2026-10-26)

A pre-release for playtesters: the first chapter of DepthWorks as players will
get it, in one file. There is no changelog here. This is what the chapter is.

**The plant.** You are a night-shift guard at the Depthworks plant, built over
something the company found deep underground and sells as power. Tonight the
service lift drops you into the sub-levels your badge does not open. The
lights are off, Order 7 has sealed the doors, the Cleanup Force is sweeping the
plant to remove witnesses, and the last message from your friend at Lab 4 was
cut off mid-line. The workers the lockdown did not reach are no longer workers.

**Chapter 1: the power plant and the perimeter.** You bring the plant back from
a black start one district at a time, and each district opens the next.

- Power Distribution: the switch hall, the cable trench, the emergency
  generators, and Load Control, where the mail relay wakes with one queued
  message.
- The Machine Hall: the galleries around the turbine floor, the exciter bay,
  the crane end, and the restrike that lights them.
- Ventilation Works and the Air Handling crawlspaces.
- Water and Cooling: the treatment hall, the drainage gallery, the sump works,
  the cooling tower basin and shell, and heat exchange.
- Maintenance: Central Stores, the Fabrication Shop, where the shutter to
  Operations needs a pin you machine yourself, and the Aid Station.
- The Restricted Perimeter: the Operations corridor, the Security Perimeter's
  yard and gatehouse, Checkpoint Delta's holding cells, the Administrative
  Link over the old ore pass, Personnel Transit's held tram, Restricted
  Utilities, Research Receiving, Decontamination, and the forecourt of Energy
  Research, where the chapter ends at the great door.

**What is in the dark.** Huntsmen and xenowasps in the ducts and galleries;
the transformed, who keep their habits; the Snatcher, which waits for you and
does not chase; and the Chimera, which does not wait. The Cleanup Force holds
the checkpoints: creep past, take them down from behind, or fight.

**Controls.** WASD move, Shift sprint, Space jump, C or Ctrl crouch, mouse
fire, right mouse guard, R reload, E use, F flashlight, Q subdue from behind,
I inventory, Esc menu. Checkpoints are automatic; manual saves are in the
menu. Saves and settings live in %LOCALAPPDATA%\RawMetal.

**VR (experimental).** SteamVR with Quest, Index or Vive controllers: real
hands, the shotgun in your fist, an Alyx-style interface. Known rough edges:
the support hand can snap on the pump, the pump slide is not drawn, the reload
has no animation yet, and Quest frame rate is below 72.

**For playtesters.** We want to know where you got lost, where you got stuck,
where you got bored, where a death felt unfair, and what your GPU and frame
rate were. Developer mode (start with `--dev`, or type DEV in the console)
gives no damage, no creatures and every door open, for getting back to a
place quickly.

---

## v0.13.0 - Chapter 1 complete, and a VR overhaul (2026-10-26)

Chapter 1 now runs end to end, from the Power Distribution black start to the
great door of Energy Research, with no placeholder rooms left. VR has been
rebuilt around proper hands and an Alyx-style interface.

- **Chapter 1 finished.** Water and Cooling (treatment hall, drainage gallery,
  sump works, cooling tower basin and shell, heat exchange); the Ventilation
  Works and the Air Handling crawlspaces; the Fabrication Shop shutter puzzle
  and the Aid Station; the Restricted Perimeter's gatehouse, Checkpoint Delta,
  Administrative Link and Personnel Transit; and the finale at the Energy
  Research forecourt and great door.
- **Area titles instead of signs.** The first time you enter a major area its
  name fades in across the top, Half-Life 2 style, with a sting. About 300
  room-name and equipment-label signs are gone, and doors no longer carry
  area-name plates. What signage remains warns of hazards, explains locked
  doors or tells part of the story.
- **Darkness and power.** Lamps switch with the plant's power state, and some
  areas are truly dark until you restore it.
- **Fixes.** Personnel Transit's entrance no longer needs a crouch, a
  subtitle no longer sticks on screen two rooms away, and dozens of floating
  or buried props are seated.
- **VR: hands and shotgun.** Hands sit where they really are on Quest, Index
  or Vive controllers, using SteamVR finger tracking. The shotgun sits in
  your fist and aims where its barrel points. With your left hand on the
  pump, slide it back and forward after each shot; one-handed it cycles
  itself. A reload chambers a round.
- **VR: Alyx-style interface.** No wrist tablet. Health shows on your left
  hand, the load and LOADING on the gun, and area titles, subtitles and new
  objectives float in front of you. Menus, inventory and terminals open on a
  floating panel you point at with the right controller.
- **VR: controls and comfort.** Jump right A, reload right B, crouch toggle
  left X, menu left Y, inventory right stick click. Walking speed follows how
  far you push the stick, and smooth turning is quicker with fine control
  near the centre.
- **VR: frame rate roughly doubled.** The second eye reuses the first eye's
  scene and shadows, and VR renders with 2x MSAA.
- **Developer mode.** Start with `--dev` (or type DEV in the console) for no
  damage, no creatures and every door open.

---

## v0.12.1 - Graphics fix for PCs without Resizable BAR (2026-10-25)

- **No more PS1-looking fallback.** On graphics cards or PCs without
  Resizable BAR, the game ran out of a small (256 MB) region of video memory
  after a few levels and switched for good to its software renderer, which
  looks far blockier. It now keeps rendering on the GPU and moves the extra
  geometry into system memory instead.
- Everything else is as in v0.12.0.

---

## v0.12.0 - The Descent, part 1 (playtest) (2026-10-25)

The main campaign continues past Deep Junction. The main-line blast door now
opens into the deep plant, and the route runs on to the Energy Research
Entrance. This is a playtest build: some areas are still placeholder rooms.

- **Power Distribution.** Black-start the plant: Substation A, the Cable
  Trench, Emergency Generation, the Switch Hall and Load Control, then the
  sealed gate to the Machine Hall. A message waits on the mail relay.
- **The Machine Hall.** A 48 m turbine hall spread over four stitched maps,
  then the Ventilation Works and the Air Handling crawlspaces.
- **Maintenance District.** Break rooms, Central Stores (a stealth patrol),
  the Fabrication Shop puzzle and an aid station with a survivor.
- **The Restricted Perimeter.** The Operations Corridor, the Security
  Perimeter and gatehouse, Checkpoint Delta, the Administrative Link and
  Personnel Transit, Restricted Utilities, Research Receiving,
  Decontamination and the Energy Research Entrance.
- **Placeholders.** Water and Cooling (chunks 47-52) and everything past the
  great door are walkable placeholder rooms marked UNDER CONSTRUCTION.
- **Engine.** 178 new purchased models named for the facility, room for
  179 campaign maps, and a 64 MiB size ceiling.

---

## v0.11.0 - Ashfall Exclusion Zone update (2026-10-24)

The Ashfall Exclusion Zone custom campaign is rebuilt as an open,
STALKER-style testbed: twenty maps of woods, marsh, ruins and roads to roam.
It stays a separate custom campaign; the main campaign is unchanged.

- **An open zone.** Rookie camp, fuel station, military checkpoint,
  scrapyard, radio tower, pine forest, swamp, creek, anomaly field, old
  mine, industrial zone and village ruins, with a three-step quest from the
  camp elder to the radio tower.
- **No walls.** The zone is edged by dense forest and wasteland, and beyond
  it the land rolls into wooded hills out to the horizon.
- **Sky and ground.** A broken overcast sky with a low warm sun, god rays
  through trees and ruins, low steam over the ground and wet patches. PSX
  grass, clover, dirt, gravel, forest litter and mud painted over the
  terrain.
- **Lush plants.** Tall grass, goldenrod, aster, bracken, sumac, bushes,
  leafy trees, aspens and pines from the purchased nature packs.
- **Painterly Horizon LOD (experimental).** Distant trees and plants are
  drawn as shared multi-angle impostors with depth and lighting, handing
  over smoothly to the real models up close; distant grass folds into the
  ground colour.
- **Sound.** Wind, birds, crickets, frogs, a creek, crackling camp fires and
  humming anomalies; footsteps on dirt, grass and gravel.
- **Voices.** Every NPC line re-recorded with directed delivery, plus new
  voices for the checkpoint soldiers, the scrapyard bandits and the camp
  guide, who will follow you. Speech is now muffled by what each character
  wears: a sealed hazmat hood, a gas mask, a face plate or nothing, and
  squad callouts come over the radio.
- **Performance.** The zone holds the 60 fps frame cap; small ground plants
  no longer render into the sun shadow maps.
- **Fixes.** An NPC crossing into another map no longer crashes the game;
  crashes now leave RawMetal-crash.txt for diagnosis.

---

## v0.10.0 - Rebuilt facility update (2026-10-24)

Every campaign map from the Foundry to the Deep Junction is rebuilt from
scratch. Each keeps its name, its purpose and its story beats; everything
else is new.

- **The facility (maps 0-9).** Foundry, Pressure Works, Turbine Gantry with
  its catwalks, the Reactor Complex stitched across three chunks, Service
  Gallery, Coolant Return, Cable Vaults, Pump Annex, Utility Junction and
  Waste Handling.
- **The freight district (maps 10-31).** Freight Access, the lift machinery
  and auxiliary lift, the receiving floor and lanes, the high-bay warehouse
  (A-B, C, D and upper racks), the manifest offices and routing wing, the
  transfer conveyors, sorting and rail yard, Platform 3, the rolling-stock
  depot, the maintenance bore and the deep junction.
- **Purchased-library dressing.** Shipping containers on flat wagons, drums,
  storage tanks, gas cylinders and road blocks, and a full subway car (Train
  27) with platform furniture, ticket machines and a vending machine.
- **Offices built like offices.** Glazed partitions, fabric cubicles with
  workstations that face their seats, suspended ceilings with troffers,
  meeting, records, archive and switch rooms.
- **A continuous rail network** from the platform through the depot and the
  bore to the junction, with no tracks into walls.
- **Smarter Cleanup patrols.** Guards walk real beats and scan from their
  posts instead of pacing.
- **Rendering.** Auto exposure, the AgX-free display curve, coloured blackbody
  lamps, wet reflections, combat effects, crater bullet holes and a visible
  autofocus rack on the cinematic lens.
- **Neural textures, round 3.** 45 more textures ship as x2 neural textures.
- **27,915,264-byte EXE / 26.62 MiB**, **5.38 MiB free** under the 32 MiB ceiling.
- All 27 test suites pass and every rebuilt map passes the placement audit.

---

## v0.9.0 - Light update (2026-10-24)

A rendering update on top of v0.8.0's Night Shift content.

- **Per-pixel lamps with cube shadow maps.** Every lamp lights the static
  world per pixel with normal-mapped diffuse and GGX specular, and casts real
  shadows from a six-face shadow map (props, railings and grates all occlude).
- **Sun and sky for Ashfall.** A directional sun with three cascaded shadow
  maps, sky-hemisphere ambient, and a procedural overcast sky with a sun disc
  and drifting cloud deck.
- **Temporal anti-aliasing.** Stable edges and texture detail in motion, on top
  of 4x MSAA. `r_taa off` returns to MSAA only.
- **Close-up quality.** Walking up to a wall no longer collapses it into flat
  blocks: a detail layer keeps relief and lighting at any distance, parallax and
  texel snapping fade with magnification, and the ghost copies the occlusion
  pass painted on near walls are gone.
- **Volumetric light.** Lamp shafts are shadow-mapped, so light pours through
  gaps and stops at walls; haze is continuous from the eye.
- Mailbox presentation (no tearing), per-frame shadow budgeting for streamed
  chunks, and the renderer survives walking into every Security Office wall at
  1080p in the new windowed-path harness.
- **27,827,712-byte EXE / 26.54 MiB**, **5.46 MiB free** under the 32 MiB ceiling.
- Vulkan, performance, prologue, Exclusion and world isolation tests pass.

Console switches for A/B: `r_pixel`, `r_taa`, `r_post`, `r_texels`.

SHA-256: `E4A465B463B12D3ECEB6E6CC92136FEF8AE1178A53B65C1568957167CC9D2FD2`

## v0.8.0 - Night Shift update (2026-10-23)

A content and presentation update, not a patch.

- **Security Office prologue** now opens the main campaign: a shift at your desk,
  a friend messaging you on the workstation, the alarm, the run to the service lift
  and the drop into the Foundry. The workstation opens full screen, and you start
  unarmed.
- **Maps 0-2 rebuilt** (Foundry, Pressure Works, Turbine Gantry) with industrial
  set pieces, new wall and floor surfaces and a second lamp grid. The freight lift
  cab is remodelled, and Map 3 now reads as a second failed escape.
- **New renderer post pass**: ambient occlusion with one-bounce light, denoised
  screen-space lamp shadows (creatures and props now block lamps), depth haze,
  volumetric lamp scattering, wide bloom and a filmic grade with a real black
  point.
- **Redesigned UI**: the title screen, HUD, pause menu, death screen and
  inventory now use anti-aliased type and translucent panels instead of the
  5-pixel font.
- Missing textures now show a pink and black checker and are logged, instead of
  stopping the game.
- Headless frame readback is about 5x faster (104 ms to 21 ms per frame).
- **27,698,688-byte EXE / 26.42 MiB**, **5.58 MiB free** under the 32 MiB ceiling.
- Prologue, world isolation, Exclusion and Vulkan smoke tests passed.

SHA-256: `08B3F110CFAC4E4E27FF206C6D39233D7D2B1BBC94652D63E9075FCD0E39954A`

## v0.7.2-preview.13 - Spatial audio patch (2026-10-19)

Approved spatial audio first pass, integrated with all sixteen reduced textures.

- Walls now smoothly muffle treble as well as volume using existing geometry checks.
- Positional stereo recordings resolve to a single emitter; music and listener-centered cues retain stereo.
- All 80 NPC takes, voice triggers, cue ordering and lift transitions remain intact.
- **24,171,008-byte EXE / 23.05 MiB**, **8.95 MiB free** below the unchanged 32 MiB ceiling. Native trained-PGO shipping build, no new dependencies or sound assets.
- Full Vulkan smoke, audio regression, live Windows device playback and 231 packaged-resource checks passed.
- Initial candidate mixer stress: 48 spatial loops in 0.25-0.28 ms per 11.61 ms output block. Final PGO shipping validation measured 0.537 ms/block. These short runs vary, exclude game-thread geometry and are not a gameplay FPS comparison.

This first slice adds spectral occlusion and point-source mixing; HRTF, room reflections and audio-thread contention work remain for later passes.

SHA-256: `1092F66712970439633C65925E0418B692FC86C9457A97ADBB79B6D12F65967F`

[Audio validation and remaining work](docs/Performance/audio-spatial-pass1-20261019.md)

## v0.7.2-preview.12 - Neural texture reduction patch (2026-10-19)

Nine more textures reduced, **sixteen total**, using the existing shared decoder and weights. Textures reconstruct once at load time and remain cached at their authored dimensions. Scissors is the largest new saving; human textures, hands, weapons, UI, normal maps, animations and AI code are unchanged. Fine grain is slightly softer.

- **23.05 MiB EXE (24,173,568 bytes)**: **1.235 MiB / 5.08% smaller than preview.11**.
- **2.156 MiB / 8.55% smaller than procedural preview.10**.
- **8.95 MiB free** under the unchanged 32 MiB ceiling.
- Exclusion lab: **309.6 aggregate FPS**, **3.23 ms average frame time**, **4.77 ms p99**, **196.7 FPS 1% low**, across 14,400 patch frames on RTX 5060 Ti at 640x360 Vulkan HDR/4x MSAA. Run averages ranged 292-321 FPS. Baseline variability was substantial; these measurements do not establish a speed gain or a stable low-FPS improvement.
- **750 decoder checks**, **231 resource checks**, full **Vulkan smoke**, **28 visibility views**, twelve Exclusion and 23 freight-detail captures passed/inspected.
- **Zero reconstruction during gameplay** in the 240-frame cache test or atlas scale changes.

The release asset matches the tested root executable.

SHA-256: `54E771DF0DBE6F3F08584B01B6BC4CC5D9E1A6BE01E341D51245BE9E7DC7509B`

[Full statistics and methodology](docs/Performance/preview12-stats-20261019.md)
[Screening and visual comparisons](docs/Assets/neural-textures-round2-20261019.md)

## v0.7.2-preview.11 - Performance and bloat reduction patch (2026-10-18)

Seven reduced surface/prop textures reconstruct once with a small shared neural model and remain cached at their authored dimensions. Odd-sized atlases preserve their UV layout. Retired OpenKTG/recipe experiments are archived; no new animation banks or runtime frameworks are added. Fine grain is slightly softer.

- **24.29 MiB EXE (25,468,416 bytes)**: **943 KiB / 3.65% smaller** than preview.10.
- **7.71 MiB free** under the unchanged 32 MiB ceiling.
- Matched trained-PGO ABBA comparison: **315.28 versus 318.69 FPS** (1.07% lower FPS for the size reduction); frame time 3.1718 versus 3.1378 ms. This patch does not claim an FPS gain over preview.10.
- **750 decoder checks**, **231 embedded-resource checks**, full **Vulkan smoke** and **28 visibility views** passed. Twelve Exclusion and eight service captures inspected.
- Zero reconstruction during 240 tested gameplay frames or atlas scale changes.

The release asset is byte-identical to the tested root executable.

SHA-256: `7C96C14B434CA63F3D71BD2A96EA72C947A47871327BE3A923BF53D470F10D42`

[Measurements and visual comparisons](docs/Assets/neural-textures-expanded-20261018.md)

## v0.7.2-preview.10 - Procedural textures (2026-10-18)

Replaces five photographic environment materials with OpenKTG-backed **RMP1**
recipes that expand to ordinary **RMT1** RGBA at load. Resource IDs **125 /
131 / 132 / 149 / 150** (concrete floor, sign rust, panel metal, service floor,
weathered metal). Original PNGs archived under `src/assets/texture-old/`.

Package: five PNGs (~1.13 MiB) ? 160 bytes of recipes; root EXE **27,236,352 ?
26,434,048 bytes (?783 KiB)** vs preview.9. Runtime atlases keep the same
dimensions. Visual HF energy improved vs the first procedural previews;
accepted for in-game look. All **25** release regression checks PASS; procedural
texture unit test PASS; pack verifier reports **5** procedural recipes among
**230** packaged resources.

Executable SHA-256
`8AF412E7214016E025B7FEDEF937A618AD9B15368AE329362FF292FDC3A9970C`.
**7.12 MiB** headroom under 32 MiB.

[Procedural texture report](docs/Assets/procedural-textures-20261018.md).

## v0.7.2-preview.9 - Performance hotfix (2026-10-18)

Promotes the tested round-ten SIMD animation winner unchanged. Same clip samples,
waist/hand behavior, graphics, AI cadence and 2015-compatible x64 baseline.
Interleaved preview.8 comparison: **308.4 to 314.2 FPS** (+1.86%); 1% lows
**187.8 to 199.9 FPS**. All nine runs match gameplay fingerprints. **330 FPS
remains unmet.** All **25 regression checks PASS**, including full smoke.

Executable **27,236,352 bytes / 25.975 MiB**; **6.025 MiB headroom** under 32 MiB.
All 230 packaged resources unchanged; winner is 7 KiB smaller than preview.8.
SHA256 `75774028EE7F71F6F1C94775ABDDA08CE83A50723F4936013DEDF9249735B812`.

[Round-ten results](docs/Performance/Competitive%20Round%2010%2020261018.md).

## v0.7.2-preview.7 - Performance hotfix (2026-10-17)

Promotes the validated round-four winner to the canonical root executable: compact
Vulkan actor triangle stream (**152 ? 92 bytes**, full float precision) with the
original lighting ABI via unused W channels, plus MSVC **/Ob3** inlining. Same
meshes, animation, materials, AI rates and save rules � render/compiler savings only.

Fresh interleaved promote check (this session): root preview.6 median **189.8 FPS**
vs winner **205.9 FPS** (+8.5%); scene **2.83 ? 2.48 ms**. Identical AI queries
**14,029,424** and final health **99158**. Smoke / exclusion / friendly / stealth PASS.
Competition batch met the **200 FPS** average target (median **202.3 FPS**).

Executable: **27,405,312 bytes** (26.14 MiB), **5.86 MiB** under the unchanged 32 MiB
ceiling. SHA256:
`661b557e7d73275327d2ed81edca4d2f6c9b9a9d9bff1ec0a40c3d02c7d69d34`.

[Full round-four results](docs/Performance/Competitive%20Round%204%2020261017.md).

## v0.7.2-preview.6 - Performance hotfix (2026-10-17)

Promotes the validated round-three winner unchanged to the canonical root executable.
Exact NPC state-key prefix hashing and validated sorted-state lookup hints remove
repeated bookkeeping without changing AI behavior, state IDs, save data or visuals.

Three-run Exclusion median: 160.6 -> 186.5 FPS; 1% lows 94.6 -> 107.7 FPS.
Measured at 640x360, Vulkan HDR/4x MSAA; CPU wall time including presentation.
This is an average throughput improvement, not a minimum FPS guarantee.

Validation: state-key/property and cached/reference replay tests, 24 regression/profile
commands, twelve exact cross-build encounter views, 28 visibility views, GPU actor
comparisons, packaged resources, full Vulkan smoke and save checks all passed.
Executable: 26,939,904 bytes (25.69 MiB), below the unchanged 32 MiB ceiling.
SHA256: 1d4c6fd9da0d471d9fb1924ef0230606ec22cb493b443577f4782b92a27536e9

## v0.7.2-preview.5 - Performance hotfix

Promotes the validated round-two support/door broad-phase winner to the root EXE.
Fresh three-run Exclusion medians: average **161.74 -> 163.51 FPS** (+1.1%),
1% lows **95.41 -> 105.14 FPS** (+10.2%), P99 **9.471 -> 8.997 ms**,
and frame-time spread **8% narrower**. RTX 5060 Ti, 640 x 360, Vulkan HDR / 4x MSAA.

- Reject irrelevant door footprints before terrain-height queries
- Use existing spatial indices for prop/terminal support, retaining exact live tests
- 21 regression commands passed, including full Vulkan smoke
- 28 visibility/reference views and 12 direct EXE comparison captures: zero changed pixels
- 56 GPU actor comparisons passed; nine combat replays kept identical gameplay counters
- No additional runtime cache, no reduced visuals/behavior, unchanged 32 MiB ceiling

EXE: **26,896,896 bytes**. SHA-256:
`35c12d64d450e86271eba3365fe4eb24ec4c9fe1c85a354046299d33dfc2534d`.

[Full round-two results](docs/Performance/Competitive%20Round%202%2020261017.md).

---

## v0.7.2-preview.4 - Performance hotfix

Promotes the validated competitive winner to the canonical root executable.
Fresh three-run Exclusion medians on RTX 5060 Ti at 640 x 360 improve average
FPS **140.2 -> 163.6** (+16.7%), 1% lows **92.2 -> 103.6** (+12.4%) and P99 frame
time **10.108 -> 9.039 ms**. Frame-time spread remains essentially unchanged;
this release does not claim hitch elimination or 500 FPS.

- Exact terrain-height reuse, retained minimap occupancy and decorative panel pixels
- Includes the validated decision clock, retained route queries and GPU actor preparation
- Preserves the walk-start optimization and existing gameplay/content edits
- No reduced animation samples, lighting, geometry or AI decision rates in this round
- Hard **32 MiB** executable ceiling unchanged

**Validation:** 26 commands passed, including full `--smoke-test --vulkan`;
28 cache reference views had zero changed pixels, and 56 GPU actor comparisons passed.
The final six competitive replays retained identical gameplay counters.
Full results: [competitive round](docs/Performance/Competitive%20Round%2020261017.md).

**Canonical EXE:** 26,896,896 bytes (25.65 MiB; 6.35 MiB headroom).
SHA-256: `17059affba29ab4e694d7e0177cdee69c35829c86510d1ec3985813c04277c0a`.

**Play:** Custom Maps -> ASHFALL EXCLUSION ZONE, or `RawMetal.exe --pc --exclusion-zone`.

---

## v0.7.2-preview.3

Exclusion yard CPU pass + docs vault reorg. Lab `--lab-performance` on RTX 5060 Ti settled ~**8.4 ms** (~**119 FPS**) after hull-scale walk pitch and static crate/prop VBO bake; calm-patrol think LOD ships for idle friendlies (combat yard totals noise-flat). GoldSrc cost-model notes live in the Obsidian vault under `docs/`.

- Indoor/terrain `aiWalkSamplePitch` coarsened; static C/B + props baked into scene VBO (cache rev 4)
- Calm patrol AI think LOD + `think_debt_us` bank; stealth/guard/tactics follow-ons from the same arc
- Depthworks branding resources in `src/branding/`; docs flattened to topic folders with `Home.md`
- Hard **32 MiB** ceiling unchanged

**Play:** Custom Maps ? ASHFALL EXCLUSION ZONE, or `RawMetal.exe --pc --exclusion-zone`.

**Validation:** EXE 26,665,984 / 33,554,432 bytes (6.57 MiB headroom). `--smoke-test --vulkan`, `--exclusion-test`, `--stealth-test`, `--friendly-ai-test` PASS.

---

## v0.7.2-preview.2

Synced tester build of the NPC radio polish from preview.1. Repo root hygiene: ignore ashfall performance dumps; no gameplay change.

Same radio behavior as preview.1 (Search bark, AllyDown cooldown, reply timing, channel holds). Confirm in play, then cut **v0.7.2**.

**Validation:** EXE 26,533,888 / 33,554,432 bytes (6.70 MiB headroom). `--npc-voice-test` PASS.

---

## v0.7.2-preview.1

Preview toward the next complete NPC radio patch. Builds on 0.7.1 chatter election with Search cues, AllyDown cooldown, longer reply timing, and channel holds so delayed copies do not expire while someone else is talking. Superseded for testers by preview.2.

- Lost-contact **Search** elects one Investigate bark; Investigate?Search stays quiet
- Reply delay stretched (~1.1�2.2 s) so the optional copy lands after the contact call
- Pending tactical / work / AllyDown claims hold the channel while `npc_voice_busy` instead of timing out mid-sentence
- AllyDown picks the nearest living voice, arms a squad cooldown immediately, priority above patrol chatter
- `--npc-voice-test` covers Search bark, Investigate?Search silence, AllyDown election/cooldown

Save format still 14. Hard **32 MiB** ceiling unchanged. Confirm in play, then cut **v0.7.2** when radio feels done.

**Validation:** EXE 26,533,888 / 33,554,432 bytes (6.70 MiB headroom). `--npc-voice-test`, `--exclusion-test`, `--stealth-test`, `--guard-roles-test` PASS.

---

## v0.7.1

Patch on the 0.7 line: Cleanup / Witness NPCs were already in the Exclusion loop; this fixes their placeholder radio behavior so squad contact sounds like chatter instead of a roll call. Exits the `0.7.0-preview.N` series � same playable loop, versioned as a normal patch.

- Shared Alert / Investigate / Retreat elect one radio voice per squad (spotter wins contact); at most one delayed copy answers
- Cue cooldowns keep the rest quiet after a call; pending claims no longer drain as a sequential queue
- Multi-chunk ambient audio uses resident world/enemy views instead of copying whole chunks each frame
- Optional `--ashfall-performance` frame profile harness and cheaper nav connector selection for profiling passes
- Native `--npc-voice-test` covers one-speaker contact + optional reply

No new maps or animation banks. Save format still 14. Hard **32 MiB** ceiling unchanged. Play: Custom Maps -> ASHFALL EXCLUSION ZONE, or `RawMetal.exe --pc --exclusion-zone`.

**Validation:** EXE 26,530,304 / 33,554,432 bytes (6.70 MiB headroom). `--npc-voice-test` PASS.

---

## v0.7.0-preview.16

Same VO chatter fix shipped under the preview label before graduating to **v0.7.1**. Superseded for testers by 0.7.1.

- Shared Alert / Investigate / Retreat elect one radio voice per squad (spotter wins contact); at most one delayed copy answers
- Cue cooldowns keep the rest quiet after a call; pending claims no longer drain as a sequential queue
- Multi-chunk ambient audio uses resident world/enemy views instead of copying whole chunks each frame
- Optional `--ashfall-performance` frame profile harness and cheaper nav connector selection for profiling passes
- Native `--npc-voice-test` covers one-speaker contact + optional reply

**Validation:** EXE 26,530,304 / 33,554,432 bytes (6.70 MiB headroom). `--npc-voice-test` PASS; `--exclusion-test` PASS.

---

## v0.7.0-preview.15

Ashfall Exclusion Zone becomes a short authored stealth/combat loop on the existing three chunks and six Cleanup roles.

**Play:** Custom Maps -> ASHFALL EXCLUSION ZONE, or `RawMetal.exe --pc --exclusion-zone`.

- Arrival cache, north scrap-rig distraction, facility relay, east winch, South Wash departure; optional witness escort / Scrap Basin
- Bounded custom objective/event/action records for interlocks, item grants, objectives, checkpoints, and completion
- Rear **Q knockout** / **E grab** (Fire kills while held); sound-only Search keeps a narrow yaw sweep; north lure second clatter
- Procedural flashlight housing after cache pickup; CRT status lamp stays on the bezel, not the phosphor
- Terrain anchoring: props/terminals keep local floor offsets; cover follows footprint
- Readable cover materials, hazard caps, numbered signs, task lamps and reused industrial props
- East winch west-flank rear KO physically replayed; north checked at three approach timings
- Receiving 02 containment detail is overrun after two player kills; real entities kill its remaining fighters while one survivor physically retreats into Receiving 03
- Save format 14 preserves living and dead script-spawned creatures; legacy versions 1-13 remain readable
- 80 ElevenLabs voice takes for the six Cleanup roles and Witness, with model-specific mask effects, positional playback, priorities and saved cooldowns
- Script exit-42 and console tick-1 fixtures corrected; full Vulkan smoke passes without quarantine
- Native `--exclusion-test` 47/47, `--stealth-test` 36/36, `--exclusion-authoring-test` 38/38

No new areas or animation banks. Hard **32 MiB** ceiling unchanged. See [README](README.md) and [encounter notes](docs/exclusion-encounter-slice-20261015.md).

**Validation:** EXE 26,510,336 / 33,554,432 bytes (6.72 MiB headroom), 230 verified embedded resources. Full Vulkan smoke passes. [Receiving breach evidence](docs/receiving-breach-20261015.md) and [voice validation](docs/cleanup-voices-20261015.md).

---

## v0.7.0-preview.14

Lossless animation packaging frees cartridge headroom for the next content pass.

- New frame-to-frame animation predictor (transform 10) before LZMS; RMA2/RMA3 round-trip byte-exact
- Duplicate packed payloads alias to canonical resources (textures + arm/skin pairs) without changing resource IDs
- Release size check warns under 6 MiB reserve; hard OoT 32 MiB ceiling unchanged
- Changelog consolidated into this single `RELEASE_NOTES.md`

**Validation:** EXE 25,499,648 / 33,554,432 bytes (~8.0 MiB headroom); 229 embedded resources verify; animation codec + split/aim + engineer smokes PASS. See [packaging evidence](docs/space-optimization-20261014.md).

**Play at release:** Same Exclusion Zone / Custom Maps content as preview.13 � packaging only. Superseded for testers by preview.15.

---

## v0.7.0-preview.13

Ashfall Exclusion Zone replaces Guard Lab / Horror OpenWorld as the wasteland feature lab.

- Continuous Surface Nets terrain for stitched Exclusion chunks (world-space heights, soft 1–3 m undulation, flat facility pad)
- Stealth kill on unaware NPCs from behind; crouch reduces sight range; multi-point patrols boot walking instead of spawn-idling
- Throw distract and lose-interest (Investigate ? Search ? Patrol) remain wired
- Exclusion Zone campaign embedded in the EXE (resource 346); Custom Maps, `MAP EXCLUSION`, or `--exclusion-zone`

**Play:** Custom Maps -> ASHFALL EXCLUSION ZONE.

---

## v0.7.0-preview.12

- Ashfall Guard Lab embedded in the EXE for one-file playtester builds under **Custom Maps**
- Squad/escort AI no longer freezes under the shared four-search nav budget: flank/cover/portal backoff, deferred Move stall charging, Deploy/Repair timeouts, and committed reload resume after Danger
- Direct Mixamo squad bake and glue-gun/tank aim from the previous bake pass remain in this build

**Playtester:** Download `RawMetal.exe`, choose **Custom Maps ? RawMetal // Ashfall Guard Lab**. No sidecar `custom maps/` folder required.

**Validation:** Guard Lab loads from packed resource 346; AI smokes (friendly, danger, schedule, tactics, roles, patient, policy); release build under OoT 32 MiB cart ceiling.

---

## v0.7.0-preview.11

- Cleanup NPC idle / walk / hit / crouch retargeted from Mixamo (rifle aiming idle, walking, hit reaction, pistol kneeling idle)
- Punch and death remain Quaternius UAL1; Medic treatment clip stays UAL2
- New bake: `tools/BakeCleanupMixamo.py` (anatomical limb retarget through Mixamo world-scale matrices)

**Validation:** Blender bake evidence `diagnostics/cleanup-mixamo-bake.json`; `--friendly-ai-test` + kill-squad stage 0; EXE 24,044,032 / 33,554,432 bytes.

---

## v0.7.0-preview.10

- Enable-gated squad staging publishes complete authored XYZ (and restores it) before `saved=1`, so activation no longer teleports from origin
- Kill-squad / danger / engineer inspections resolve cargo roles by authored role instead of hard-coded actor indices
- Medic inspection leaves the bay after wounding so treatment can complete; pursuit observer uses hull-safe south-chunk refuges
- Cleanup mesh validation requires head+body material parts; mesh material-part ID ranges cover all six cleanup role resources
- Mixamo packs parked under `source-assets/mixamo-motion/` as local donors (not baked yet)

**Validation:** `--friendly-ai-test` all PASS; kill-squad stages 0–13 PASS; EXE 24,045,056 / 33,554,432 bytes.

---

## v0.7.0-preview.9

- Reuse creature pose scratch buffer in `poseCreature` (no per-frame allocation for every humanoid)
- Skip armed bind-pull / sleeve IK work when influence is negligible
- Enable-gated guards only clear schedule/danger state while that state is still dirty
- Idle patrol guards, escorts, civilians, and pre-enable squads track visible threats via `friendlyAwareness`
- Awareness considers enemies on the NPC's resident streamed chunk, not only the player's level

**Validation:** `--friendly-ai-test`, `--smoke-test`, `--performance-test` PASS; EXE 24,025,600 / 33,554,432 bytes.

---

## v0.7.0-preview.8

- Guard Lab / enable-gated squads keep cycling UAL idle instead of freezing on bind (sample 0)
- Armed draw softens bindPull so posed idle and legs stay readable (cleanup ×0.18, Worker idle ×0.55, walk/crouch ×0.4)
- Ashfall Guard Lab primary guard uses Cleanup Assault mesh for the stronger idle cycle
- Idle phase rate raised to 0.45 so staged and armed idles read as alive

**Validation:** `--friendly-ai-test` PASS (disabled-squad idle advance and armed phase cycling); EXE 24,023,552 / 33,554,432 bytes.

---

## v0.7.0-preview.7

- Store-low / run-high Vulkan texture present scale (`TEX_UP 1|2|4`, default 2×)
- Half-res packed atlases for skybox, service van, remington, scissors (cart size)
- Denser ceiling lighting samples so fixture corona no longer stairs at 1 m tile seams

**Notes:** Console commands `TEX_UP`, `R_SCALE`, `FPS`. Guard-lab NPC animation fix still outstanding at this tag.

---

## v0.7.0-preview.6 — Rendering and transfer performance

Diffuse textures filter in linear light using sRGB images and complete area-weighted mip chains. Fine patterns retain average brightness instead of darkening as they recede. Original embedded assets remain lossless.

Normal-map mip levels and hardware interpolation preserve the mean normal and its variance; GGX uses that variance to soften unresolved highlights. Ceiling light scattering follows room density and lamp intensity, clips to the finite volume, and uses a smooth brightness shoulder.

Vulkan batches immutable texture transfers through a reusable staging buffer. Fixed ceiling fixtures reuse geometry across frames. In a focused Warehouse cache check, CPU scene preparation used 45% less time with identical compared pixels.

Campaign still contains 32 chunks. EXE limit strictly below 28,000,000 bytes. EXE **23,360,000 bytes**; 185 embedded resources verified lossless. Evidence: [docs/render-quality-pass6.md](docs/render-quality-pass6.md).

---

## v0.7.0-preview.5 — Freight construction and material stability

Freight Access gains a human-scale service ceiling, recessed extraction fans, mounted cage lamps and connected painted pipework. Receiving, warehouse, and inspection pit use working-space arrangements. No maps added.

- Imports complete forklift (`forklift.zip`), extraction fans, packing crate, and physical switch/CRT console; original source pixels/geometry retained
- Open steel stairs with optional side rails; shared render/collision/optical geometry
- Narrows inspection pit into a real service trench; develops transfer/sorting/platform construction
- Green status displays on native computer screens; generic wall-mounted lights with authored facing
- Fixes GPU material caching across texture reallocations; linear emission mip chains

EXE **23,312,384 / 25,000,000 bytes**; 185 resources. Evidence: [docs/freight-reference-pass.md](docs/freight-reference-pass.md).

---

## v0.7.0-preview.4 — Freight overlooks and material shading

Manifest offices overlook the real stitched warehouse with protected glazing and authored visibility residency. Vulkan shader fixes a scale-dependent rejection that disabled GGX highlights and parallax on valid close surfaces. Still 32 campaign chunks.

- Generic `VISIBLE_GROUP` authored record for observation windows
- Real warehouse/Manifest window with retaining wall, posts, sill, header and glazing
- Wooden table/chair from PSX Bunkers; terminal facing direction in authored packs
- Cargo Tunnel cables/grilles; Pump Annex motors and drop housings

EXE **22,124,032 / 25,000,000 bytes**; 168 resources. Evidence: [docs/industrial-visual-pass.md](docs/industrial-visual-pass.md).

---

## v0.7.0-preview.3 — Controller interaction and depot rail equipment

VR doors and terminals use the gripping controller's reach and direction instead of requiring head facing. Depot locomotives and suspended inspection chassis developed further; no map chunks added.

- Either hand can touch nearby or point at distant controls; second-hand grip can operate while first remains held
- Locomotives: round steel wheels, axles, hubs, suspension, louvers, stripe
- Suspended chassis: open longitudinal beams/crossmembers; collinear UV fix on cylinder caps
- Precomputed cylinder ring reuse

EXE 22,082,560 bytes. Offline VR/freight/physics/Vulkan checks passed; live headset comfort still playtest-pending.

---

## v0.7.0-preview.2 — Tracking recovery and freight detail

Fixes VR tracking recovery and develops warehouse/depot maps. No campaign chunks added.

- Lost headset tracking clears movement, grips, firing, jump/reload and wrist pointer; recovery starts room-scale from newly tracked position
- Disconnected controllers cannot feed stale SteamVR actions
- Warehouse shelves: varied pallet loads, empty slots, rear bracing
- Inspection-pit workshop: track-height benches, parts, cabinets, tool-bay sign

Executable below 25,000,000-byte limit; embedded assets verified lossless.

---

## v0.7.0-preview.1 — SteamVR controls and wrist computer

Adds PC/VR launch selection and native SteamVR Vulkan stereo rendering while retaining the HDR/material pipeline. VR preview, not finished headset coverage.

- Black/green terminal in a 13.9 cm steel wrist computer; right-controller pointing/clicks/sliders
- Corrects backward shotgun orientation in VR
- Either-hand grip pickup, grip-release drop/throw, shoulder shotgun stow/equip, gripped fist strikes
- Desktop right-eye spectator for streaming
- Fixes SteamVR/Vulkan shutdown ordering
- EXE limit raised to 25,000,000 bytes

EXE 22,072,320 bytes; 166 packaged assets lossless. See [docs/vr.md](docs/vr.md).

---

## v0.6.6 — Eye-Level Carry Fix

Carried objects were capped at 0.48 m above the feet (near knee height). Their centre now follows the camera aim 0.9 m from the current eye, including standing, crouching and looking up/down. Ground support, obstruction, drop and punt remain active. Rendering quality and v0.6.5 performance improvements retained.

Regression checks cover seven carryable shapes in both stances at three aim angles, shelf placement, seam transfer, throw damage and settling. EXE below 22,000,000 bytes; 166 assets lossless.

---

## v0.6.5 — Performance and Frame Pacing

Stationary pipework reuses a world-space Vulkan vertex buffer so camera turns no longer rebuild cylinders. Collision selects nearby fixture/prop/terminal candidates from a spatial grid before exact tests. Arm rig performs one final skinning pass after IK. Vulkan surface capabilities queried on window change, not twice every frame.

Resolution, 4× MSAA, HDR, bloom, parallax and materials unchanged. Local Cable Vaults p95 fell from 58.4 to 7.3 ms; Waste Handling from 36.9 to 9.9 ms (RTX 5060 Ti). See [docs/performance-quality.md](docs/performance-quality.md).

---

## v0.6.4 — Windows Compatibility Patch

Explicitly targets Windows 10/11 x64 in build configuration and embedded application manifest. Renderer fixes from v0.6.3 included unchanged.

Install [MSVC x64 Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist/) 14.44+ and a Vulkan-capable GPU driver. `--software` selects the CPU renderer. See [docs/windows-compatibility.md](docs/windows-compatibility.md).

---

## v0.6.3 — Renderer Lighting Repair

Fixture lights previously stopped at 5.48 m; influence now fades over 12 m. Lower interior ambient lets direct light and occlusion define surfaces. Cached tangent-space normals use the physical surface hemisphere. Moving actors/doors and arms/weapon receive material lighting from visible fixtures.

Shared static-light visibility accelerates exact box intersections. HDR bloom samples small emitters more closely. Receiving worker encounter, soundtrack and saves remain compatible. 166 resources verified; EXE below 22,000,000 bytes. See [docs/renderer-lighting-repair.md](docs/renderer-lighting-repair.md).

---

## v0.6.2 — Freight Worker Encounter

Receiving stages the first human encounter: radiation worker wanders until observed from the overlook, then walks to a fight spot and is attacked by two wasps. Faster duck/strike/hit/death animations; turns toward the active bug; pipe uses hand anchors; persistent body left behind cargo.

`map receiving` / `map 13` place the player at the overlook. Encounter state survives save/load. Shared authored actor timelines support native and custom campaigns. Vulkan lighting/transparency improvements. EXE 21,956,096 bytes; 166 resources.

---

## v0.6.1 — Freight Repairs

Service van wheels restored at correct axles (FBX assembly rebuild). Freight Access and Lift Machinery get clearer signs, cargo dressing and shaft guards. Auxiliary lift release opens its gate; lower landing connects for walk-off. Shared boundary-door headers close openings above entry/transfer doors. Terminal lore renders as green text on recessed black screens.

EXE 20,710,912 bytes; save format 13 unchanged.

---

## v0.6.0 — Freight District Playtest

Main campaign continues from Waste Handling into eight freight areas across 22 connected chunks: Freight Access, Receiving, Warehouse, Manifest Offices, Transfer, Empty Platform, Depot and Cargo Maintenance Tunnels.

District plays **Pressure** (Echoes Audio Super Kit) with music cursor continuity across stitched boundaries. Exactly two mutated humans (Warehouse C, Depot Trackside) with supplied MonsterPSX model and baked animations. Save version 13; versions 1–12 remain readable.

EXE 20,663,808 bytes (under 22,000,000 cap); 157 embedded resources verified.

---

## v0.5.9.7 — Service Map Rendering

Maps 6–9 ambient raised from 0.18–0.26 to 0.48–0.55 while retaining fixture visibility shadows. Mipmapped surface normals on concrete pressure floor; 4× MSAA (2×/1× fallback). EXE 18,649,600 bytes.

---

## v0.5.9.6 — HDR Light Pipeline

Vulkan scene renders into blended RGBA16F when supported (RGBA32F secondary, LDR fallback). Surface light, emission, additives and water stay linear through composition; bloom and filmic tone-map once before HUD. Service-map ambient tuned to 0.18–0.26 for Cable Vaults through Waste Handling. EXE 18,643,968 bytes.

---

## v0.5.7 — Ashfall Coast

Ashfall extends east into three connected coastal chunks. Walkable rock-and-sand shelf slopes into terrain beneath the sea. Coastal water, sky and atmosphere; original wasteland intact. `map coast` jumps to the middle coastal chunk. Existing Ashfall chunk IDs and saves remain compatible.

---

## v0.5.6 — Camera depth and focus

Native Vulkan presentation uses scene depth for distance-aware lens softness (2.10 cm focal length, f/4.8). Distant rooms soften up to seven screen pixels; nearby geometry, weapon and HUD stay sharp. Depth-aware sampling prevents foreground bleed. ACES tone mapping applied once.

---

## v0.5.5 — Explosive barrel fire

Shooting a chemical barrel produces a visible world-space fire burst with rising flames and smoke, occluded by geometry. Tile destroy, nearby damage, camera shake and impact sound retained. Flame/smoke masks generated in-EXE.

---

## v0.5.4 — Square fixture light shafts

Ceiling-lamp fog shafts begin across the 0.8 m square luminous fixture and widen gently downward. Vulkan ray-marched volume and software fallback use matching proportions.

---

## v0.5.3 — Atmospheric renderer update

Vulkan scene depth and material response improved while keeping low-resolution texture art. Restrained contact shading, consistent local lighting, subtler fog shafts, gloss, filmic color and small bright-pixel bloom. Static geometry caching and first-frame lighting fix. Terrain occlusion probes limited to indoor worlds. EXE 18,607,104 bytes.

---

## v0.5.0 — Utility campaign expansion

Continue through the Cable Vaults bulkhead in Coolant Return (map index 5). Four connected chapters:

- **Cable Vaults:** low service passages, damaged electrical trench detours, pulsing arcs, disconnect, flashlight pickup
- **Pump Annex:** three elevations, large machinery, manifold stairs, observation route, pump restart
- **Utility Junction:** elevated bridge, control booth, waste dispatch, checkpoint, locked future connections
- **Waste Handling:** sorting deck, disposal basin, scrap, animated conveyor/compactor

Save version 11 retains prior six-chunk campaign saves and adds the new route. Freight Access remains a future chapter; route ends at Waste Handling dispatch.
