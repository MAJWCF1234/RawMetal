# Cleanup dialogue and casting

The API follow-up generated 80 takes using these ElevenLabs voices and applied
equipment-specific effects. The runtime uses the finished bank for Cleanup and
the Witness. The authored script below includes production direction; the exact
shipped wording and cue mapping live in [the catalog](../source-assets/cleanup-voices/catalog.json).
Some action claims were tightened before generation, including wounded cover,
pad supervision and damage absorbed by armor. See [validation](cleanup-voices-20261015.md).
The source of visual direction is the native stage-3 front/side skinned render,
not texture thumbnails alone.

| Role | Visible equipment and delivery | Voice | Mask treatment |
|---|---|---|---|
| Assault | Helmet, red goggles, full respirator; rough, clipped orders | Harry | Firm midrange filter and light radio breakup |
| Sharpshooter | Hood and gasmask; husky, quiet, precise | Callum | Narrower, restrained filter |
| Rusher | Light vest, goggles and partial face cover; quick and impatient | Charlie | Mild muffling, preserve energy and consonants |
| Heavy | Bulky sealed armor and full mask; low, measured | Brian | Strongest low resonance and saturation |
| Engineer | Bandana, shades, exposed face; practical and conversational | Chris | Natural voice, no gasmask effect |
| Medic | Armored full visor/respirator; even, clinical urgency | Eric | Clean filtered midrange, lighter distortion |
| Witness | Yellow hazmat suit and full face filter; frightened, breathless | George | Mild sealed-mask filter, retain emotion |

Keep dry masters. Apply equipment effects after generation, with consistent
loudness and enough consonants to understand under gunfire. Bitcrushing must
not bury gameplay information. Do not pitch-shift every character into the same
voice. Patrol barks should be sparse; alert and injury take priority. No dead
actor may report its own death in third person. The supplied named down reports
belong to a living squadmate; personal dying lines are separate.

## Assault

- Patrol: “Sector clear. Keep moving.” / “Check the corners. Keep your spacing.”
- Investigate: “Heard that. Checking.” / “Hold here. I'll look.”
- Alert: “Contact! Closing!” / “Move up! Cover the flank!”
- Hurt: “I'm hit!” / “Still in it!”
- Dying: “I'm down—keep moving!”
- Retreat: “Detail's gone! Falling back!”
- Ally down: “We've lost one. Close the gap!”

## Sharpshooter

- Patrol: “Lane's covered.” / “Clear sightline. Holding.”
- Investigate: “Heard something. Holding angle.” / “Movement at the edge. Checking.”
- Alert: “Target marked. Taking the shot.” / “Keep them in the open.”
- Hurt: “Winged—still up.” / “Lost the angle. Moving.”
- Dying: “I'm out. Cover the lane.”
- Retreat: “Position lost. Breaking contact.”
- Ally down: “One down. Watch their lane.”

## Rusher

- Patrol: “Eyes forward.” / “Don't bunch up.”
- Investigate: “Something moved. I'm on it.” / “Cover me. I'll flush it out.”
- Alert: “Rushing! Get clear!” / “I'm going through!”
- Hurt: “Keep going!” / “Not stopping!”
- Dying: “Can't—keep moving!”
- Retreat: “We're overrun! Moving back!”
- Ally down: “They're down! Don't stop here!”

## Heavy

- Patrol: “Holding the door.” / “Stay behind the armor.”
- Investigate: “Noise. Nobody run past me.” / “Watch the gap. I'll hold.”
- Alert: “Suppressing! Stay behind me!” / “Keep your heads down!”
- Hurt: “Armor's holding.” / “That got through.”
- Dying: “I'm down—door's open!”
- Retreat: “Line's broken. Pulling back.”
- Ally down: “Lost one. Get behind me.”

## Engineer

- Patrol: “Kit's ready.” / “Connections look good.”
- Investigate: “Could be a short. Checking.” / “Keep watch while I check this.”
- Alert: “Contact! Cover me!” / “Need room to work!”
- Deploy: “Drone out! Cover me!” / “Drone's live. Watch your fire.”
- Repair: “Hold them off. I'm repairing it.”
- Support: “Pad's live. Don't step on it.”
- Hurt: “Tools first—then me.” / “I'm hit. Kit's still working.”
- Dying: “I'm down. Save the kit.”
- Retreat: “They're all gone. Falling back deeper!”
- Ally down: “Lost another. We need to move!”

## Medic

- Patrol: “Vitals quiet.” / “Keep a dressing where you can reach it.”
- Investigate: “That didn't sound good.” / “Checking. Stay in sight.”
- Alert: “Stay low! I'm pulling wounded!” / “Cover the wounded!”
- Treat: “Hold still. I've got you.” / “Pressure on. Keep breathing.”
- Hurt: “Medic's hit—still working.” / “I can wait. Cover the patient.”
- Dying: “I'm down. Self-treat.”
- Retreat: “No one left to pull out. Falling back!”
- Ally down: “We've lost them. Stay together.”

## Witness

- Follow: “I'm with you. Don't leave me.” / “Just keep going. I'm right here.”
- Wait: “I'll stay put. Be quick.” / “Here? All right. Hurry back.”
- Danger: “They're coming—please—” / “I can hear them. We have to go!”
- Hurt: “Ah—don't let them take me.” / “I'm hurt. Please, wait!”

## Named casualty reports from surviving squadmates

“Assault's down—” / “Shooter's out.” / “Rusher down!” /
“Heavy's down—door's open!” / “Engineer's down. Kit's yours.” /
“Medic's down. Self-treat.”

Trigger claims from real outcomes: deployment after a drone appears, treatment
only with a live patient, armor lines only if armor absorbed damage, and named
casualty reports only when that role actually dies. Do not say “east” without a
known eastward source. Avoid promising a pad or wounded retrieval that is not
currently happening. Silence is preferable to a misleading callout.
