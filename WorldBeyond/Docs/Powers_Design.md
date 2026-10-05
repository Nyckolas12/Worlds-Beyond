# Worlds Beyond — demigod powers

Two demigods, two kinds of power:

- **Angel** — the storm. Electricity in blue and purple: a lightning dash, strikes from the sky, arcane bolts, healing.
- **Ji-Woong** — the *Radiant Hexblade*. A warlock who weaves sword attacks with golden radiance: marks enemies with
  curses, then his blade collects on them.

Together they have one super move, **Heaven's Judgment**, charged by the party's **Bond** meter.

Numbers below are the prototype values on the ability assets (`/Game/WorldsBeyond/Abilities/`); tune them there.

---

## Ji-Woong — the Radiant Hexblade

**Core idea: weave.** His magic sets enemies up and his sword finishes them. Every spell should either be triggered
by, or pay off with, a sword hit.

**Core mechanic (design): Radiant Weave.** Sword hits build up to 3 *Radiance* (a gold glow on the blade). His spells
spend Radiance for a stronger version. Not built yet; Sunbrand's detonation is the first "sword pays off magic" loop.

| Power | Status | Slot | What it does | Weave |
|---|---|---|---|---|
| **Sunbrand** | ✅ prototype (`GA_JiWoong_Sunbrand`) | E | Hurls a golden sigil at the enemy under the crosshair (aim assist 20°). For 8 s the target takes **+20 % damage**. His next **sword hit** on it detonates the brand: **40 damage** to enemies within 3 m, stagger, and he **heals 50 %** of the damage dealt (warlock drain). 8 s cooldown. | Magic → sword |
| **Gilded Step** | ✅ prototype (`GA_JiWoong_GildedStep`) | Q | Golden dash, 7 m in 0.25 s, invincible, passes through enemies. Everyone he crossed is hit **0.4 s later** for 30 (stagger). Counts as a sword hit, so it **detonates Sunbrands** too. Stamina cost + cooldown from `GE_Dash_Cost` / `GE_Dash_Cooldown`. | Sword → magic |
| Dawnblade Crescent | design | LMB finisher | The last swing of his combo releases a golden crescent wave that pierces enemies. With 3 Radiance: three crescents in a fan. | Sword → magic |
| Oathbound Blades | design | ability | Summons 3 spectral golden swords that orbit him for 10 s and strike the nearest enemy every second; each strike applies a short Sunbrand. | Magic → sword |
| Eclipse Siphon | design | R (replaces Heal) | Plants his sword and channels a cone of eclipse light for 3 s, draining enemies and healing both demigods for half the damage. Moving cancels it. | Warlock drain |
| Solar Vow | design | defensive | 0.8 s parry stance. A hit during it is negated, the attacker is blinded (stunned 1.5 s) and his next sword hit is a guaranteed crit. Uses the existing `State.Parrying` pipeline. | Counter |
| Judgment of Noon | design | ultimate | Leaps and plunges his sword into the ground; a pillar of light erupts, then five more in a ring. His solo echo of Heaven's Judgment. | Finisher |

**Building the design ones.** Most reuse what's already there:
- Crescent → `UBeyondGA_Projectile` with a golden projectile Blueprint (sword-wave FX: `P_AU_Sword_Proj_Trail_01_RED`, recoloured).
- Oathbound Blades → a new actor that orbits and calls `ApplyDamage` / `ApplyBrand`.
- Eclipse Siphon → a channel ability (hold input) + `FindHostilesInRadius` with a cone check.
- Solar Vow → `State.Parrying` for the window, then react to `Event.Hit.Parried`.
- Judgment of Noon → `UBeyondGA_GroundStrike` without the aiming reticle, plus a ring of delayed strikes.

**The sword.** It rests on Ji-Woong's left hip, katana-style, and he walks relaxed (`MM_Idle1`). He draws it when an enemy
comes within 10 m, or instantly when his sword combo starts, and his idle becomes the guard stance (`sword-idle`). After 8 s
of calm he sheathes it again. Draw and sheathe use the AwesomeSword Unsheath / Sheath animations on the upper body, so he
can keep walking.

**The sword combo.** LMB swings; pressing again inside each swing's window chains the next (the window is 15 % longer
than the animation's notifies). He calls out once per combo. On the move the swings play on his upper body so his legs
keep running; standing still he keeps the full footwork.

## Angel — the storm (current kit)

| Slot | Power | Notes |
|---|---|---|
| LMB | Magic spell (Blueprint) | Goes to the crosshair (shown with the staff out); hold RMB to aim |
| Q | Blink — his lightning dash | Fixed: no more orange hair once the cue edit in `GAS_Prototype.md` is done |
| E | **Arcane Spikes** (`GA_Angel_LightningStrike`) | Hold to aim the purple circle, release: blue / purple crystal spikes burst out of the ground in a wave; 100 damage in 2.5 m, stagger. His buddy AI uses it too |
| R | Heal | |

**The staff.** It rests diagonally on Angel's back and he walks relaxed (`MM_Idle`). He draws it when an enemy comes
within 10 m or the moment he casts, and his idle becomes the staff stance (`UE5_WZ_Idle_Seq`); after 8 s of calm he
stows it again. Draw and stow use the MagicStaff pack's back unsheathe / sheathe on the upper body, so he can keep walking.

**Aiming.** With the staff out the camera sits over his right shoulder and a crosshair marks where his spells go; the
enemy under it glows purple. Hold RMB to aim closer and turn with the camera. Every cast turns him to the crosshair.

**Arcane Spikes (E).** No more yellow bolt: the ground answers him. Faceted crystals in Angel's blue-to-purple burst
out of the floor from the centre outward (tallest in the middle, leaning out at the edge), flash, hold for a moment and
sink back. The hit lands as the first crystals break the surface.

Ideas that tie into Ji-Woong: Angel's lightning on a **Sunbranded** enemy *overcharges* it — the bolt chains to one more
enemy and gives extra Bond. (Hook: `UBeyondCombatSubsystem::IsBranded`.)

---

## Bond meter

Shared by the party, shown bottom centre (blue → purple → gold; "HEAVEN'S JUDGMENT READY [G]" when full).

| Gain | Amount |
|---|---|
| Damage dealt by either demigod | 0.2 per point |
| Damage taken by either demigod | 0.1 per point |
| **Synergy**: both demigods hit the same enemy within 3 s | +4 |
| Hits during Heaven's Judgment itself | none |

Full at 100. Empties on use and on a party wipe. Tuned on `BP_PC → Party Component → Party | Bond`.
The duo move sits in its own slot to the left of the meter: its icon (the **Icon** of `GA_Duo_HeavensJudgment`) is greyed
out while charging and lights up, with the **G** key, when the meter is full.

## Heaven's Judgment (duo super move)

*Storm Judgment* + *Heaven & Earth*: Angel tears the sky open, Ji-Woong takes the storm into his blade, adds his gold,
and brings it down as one shockwave.

**Trigger:** G with a full Bond meter, both demigods alive and within 15 m. Works whoever the player controls. Roles
come from **Duo Role** on each character (Angel = Conduit, Ji-Woong = Striker). Both are invincible and can't be
interrupted for the whole move; the buddy AI waits.

| Time | Phase | Angel (Conduit) | Ji-Woong (Striker) | Effect |
|---|---|---|---|---|
| 0 – 1.4 s | **1. Heaven** | Telekinesis channel (`AM_Duo_Conduit_Telekinesis`) | holds | 7 flashes of lightning, alternating **blue** / **purple**, strike random enemies within 12 m of Ji-Woong: 15 damage, stun, launched into the air (bosses stay grounded) |
| 1.4 – 2.2 s | **2. Absorb** | lightning leaves his hands | battle cry (`Montage_Axe_Battlecry`) | bolts land in a line from Angel to Ji-Woong; a storm ball and golden flare wrap Ji-Woong |
| 2.2 – 3.2 s | **3. Judgment** | — | leaping slam (`Montage_Sword_Jump_Attack`) | at impact: gold + blue + purple shockwave, camera shake; **180 damage at the centre → 80 at 9 m**, unblockable, knock-back |
| 3.2 – 3.8 s | recovery | | | control returns |

All timings, damage, radii, montages and effects are properties on `GA_Duo_HeavensJudgment`. Blueprint hook
**On Phase Started (Phase, Conduit, Striker)** is there for polish (camera, slow motion, voice lines).

### Polish list
- Retarget the mannequin montages (battle cry, jump attack, sword draw, hadouken) onto the MetaHuman skeleton, and give
  the MetaHumans' `CharacterMesh0` the body mesh (or copy root motion) so leaps actually travel.
- Real VFX: a Niagara bolt system with a colour parameter (so blue / purple / gold are exact), a beam from Angel's hands
  to Ji-Woong's blade during Absorb, a ground-cracking shockwave ring.
- Camera: pull back and orbit during Heaven, snap in on the slam; 0.1 s hit-stop at impact.
- Audio: thunder layers per flash, a rising charge during Absorb, a bass drop on impact; both voice actors call it out.
- UI: pulse / sound when the meter fills; a hint when the partner is too far away. Concept art for a designed duo
  meter (curved bronze arc / braided twin bars / radial ring around the Heaven's Judgment icon) was made in Higgsfield
  during pass 5.
- Arcane Spikes: a dust / shard burst and a ground-crack decal when the crystals break through.
- Icons: the new abilities show placeholder icons in the ability bar (`DT_AbilityMetaData`).
