// Fill out your copyright notice in the Description page of Project Settings.

#include "BeyondGameplayTags.h"

namespace BeyondTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "Health reached zero");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Invincible, "State.Invincible", "Ignores damage unless the damage has Damage.IgnoreInvincible");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Blocking, "State.Blocking", "Blocks damage unless the damage has Damage.Unblockable");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Parrying, "State.Parrying", "Parry window: negates damage and staggers the attacker");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Uninterruptible, "State.Uninterruptible", "Damage does not trigger hit reactions");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Stunned, "State.Stunned", "Cannot act");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Duo, "State.Duo", "Performing the duo super move; the companion AI waits");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Branded, "State.Branded", "Carries a brand (Sunbrand): takes extra damage, the brander's melee hit detonates it");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dashing, "State.Dashing", "Mid-dash");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Aegis, "State.Aegis", "Storm shield (Tempest Aegis): less damage taken, part of it reflected");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Resetting, "State.Resetting", "Enemy returning home after losing its target: no damage, refills on arrival");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Warded, "State.Warded", "Warded elite: spell damage is cut until a melee hit breaks the ward");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Active, "Ability.Active", "Any attacking/casting ability is running");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Enemy_Attack, "Ability.Enemy.Attack", "An enemy attack (interrupted by stagger / stun / knock-back)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_HitReact, "Ability.HitReact", "Plays an enemy's hit reaction");

	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Enemy_Primary, "Cooldown.Enemy.Primary");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Enemy_Secondary, "Cooldown.Enemy.Secondary");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Enemy_Special, "Cooldown.Enemy.Special");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Enemy_Ultimate, "Cooldown.Enemy.Ultimate");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Enemy_Mobility, "Cooldown.Enemy.Mobility");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Enemy_Summon, "Cooldown.Enemy.Summon");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Enemy_Beam, "Cooldown.Enemy.Beam");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Enemy_Buff, "Cooldown.Enemy.Buff");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Region_Forest, "Region.Forest", "The starting forest (wolves, raiders, treants)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Region_CorruptedWoods, "Region.CorruptedWoods", "The dark-magic corrupted woods (wraiths, cursed knights, corrupted beasts)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Region_Frost, "Region.Frost", "The frozen forest in the north-east (Hrimgar's land)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Region_Molten, "Region.Molten", "The volcanic land in the north-west (Kael'thar's land)");

	UE_DEFINE_GAMEPLAY_TAG(Enemy_Affix, "Enemy.Affix");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Enemy_Affix_Molten, "Enemy.Affix.Molten", "Hits burn; leaves lava when it dies");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Enemy_Affix_Venomous, "Enemy.Affix.Venomous", "Hits poison");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Enemy_Affix_Stormcharged, "Enemy.Affix.Stormcharged", "Pulses lightning around itself");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Enemy_Affix_Warded, "Enemy.Affix.Warded", "Spells barely hurt it until a melee hit breaks the ward");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Enemy_Affix_Juggernaut, "Enemy.Affix.Juggernaut", "Can't be interrupted, tougher, bigger");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Enemy_Affix_Swift, "Enemy.Affix.Swift", "Faster");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Enemy_Affix_Vampiric, "Enemy.Affix.Vampiric", "Heals from the damage it deals");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Enemy_Affix_Brood, "Enemy.Affix.Brood", "Splits into smaller copies when it dies");

	UE_DEFINE_GAMEPLAY_TAG(Boss_Phase_1, "Boss.Phase.1");
	UE_DEFINE_GAMEPLAY_TAG(Boss_Phase_2, "Boss.Phase.2");
	UE_DEFINE_GAMEPLAY_TAG(Boss_Phase_3, "Boss.Phase.3");
	UE_DEFINE_GAMEPLAY_TAG(Boss_Phase_4, "Boss.Phase.4");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Boss_Enraged, "Boss.Enraged", "The boss is enraged (a twist)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Hit, "Event.Hit", "Parent of all hit reaction events");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Hit_Light, "Event.Hit.Light", "E_DamageResponse::HitReaction");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Hit_Stagger, "Event.Hit.Stagger", "E_DamageResponse::Stagger");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Hit_Stun, "Event.Hit.Stun", "E_DamageResponse::Stun");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Hit_KnockBack, "Event.Hit.KnockBack", "E_DamageResponse::KnockBack");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Hit_Blocked, "Event.Hit.Blocked", "Damage was blocked");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Hit_Parried, "Event.Hit.Parried", "Damage was parried");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Death, "Event.Death", "Sent to a character when its health reaches zero");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Abilities_Changed, "Event.Abilities.Changed", "Granted abilities changed (ability bar refresh)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Montage_Trigger, "Event.Montage.Trigger", "Sent by a montage notify at the moment an ability should take effect");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_ShootProjectile, "Event.ShootProjectile", "Sent by a cast montage notify when the spell leaves the hand");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Weapon_Equipped, "Event.Weapon.Equipped", "Equip / sheathe request; the weapon tag is in TargetTags");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Progression_LevelUp, "Event.Progression.LevelUp", "Sent to a demigod when it levels up (EventMagnitude = new level)");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Action_Draw, "Weapon.Action.Draw", "Equip event: draw the weapon (instigator tag)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Action_Sheathe, "Weapon.Action.Sheathe", "Equip event: put the weapon away (instigator tag)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Action_Instant, "Weapon.Action.Instant", "Equip event: skip the animation (instigator tag)");

	UE_DEFINE_GAMEPLAY_TAG(DamageType_Melee, "DamageType.Melee");
	UE_DEFINE_GAMEPLAY_TAG(DamageType_Projectile, "DamageType.Projectile");
	UE_DEFINE_GAMEPLAY_TAG(DamageType_Explosion, "DamageType.Explosion");
	UE_DEFINE_GAMEPLAY_TAG(DamageType_Environment, "DamageType.Environment");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(DamageType_Proc, "DamageType.Proc", "Damage from armor-set effects; never triggers set effects itself");
	UE_DEFINE_GAMEPLAY_TAG(DamageType_Proc_Poison, "DamageType.Proc.Poison");
	UE_DEFINE_GAMEPLAY_TAG(DamageType_Proc_Burn, "DamageType.Proc.Burn");
	UE_DEFINE_GAMEPLAY_TAG(DamageType_Proc_Lightning, "DamageType.Proc.Lightning");

	UE_DEFINE_GAMEPLAY_TAG(Damage_Unblockable, "Damage.Unblockable");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Unparryable, "Damage.Unparryable");
	UE_DEFINE_GAMEPLAY_TAG(Damage_IgnoreInvincible, "Damage.IgnoreInvincible");
	UE_DEFINE_GAMEPLAY_TAG(Damage_ForceInterrupt, "Damage.ForceInterrupt");

	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Damage, "SetByCaller.Damage");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Heal, "SetByCaller.Heal");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Duration, "SetByCaller.Duration", "Duration of UBeyondGE_Cooldown / UBeyondGE_Brand");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Experience, "SetByCaller.Experience", "EXP granted by UBeyondGE_GrantExperience");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_MaxHealth, "SetByCaller.MaxHealth", "Max health added by UBeyondGE_LevelStats");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_MaxStamina, "SetByCaller.MaxStamina", "Max stamina added by UBeyondGE_LevelStats");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Strength, "SetByCaller.Strength", "Strength added by UBeyondGE_LevelStats");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Arcana, "SetByCaller.Arcana", "Arcana added by UBeyondGE_LevelStats");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Defense, "SetByCaller.Defense", "Defense added by UBeyondGE_LevelStats");

	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Damage_Burst, "GameplayCue.Damage.Burst");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Heal_Burst, "GameplayCue.Heal.Burst");

	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_GildedStep_Trail, "GameplayCue.GildedStep.Trail");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_GildedStep_Detonate, "GameplayCue.GildedStep.Detonate");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Sunbrand_Mark, "GameplayCue.Sunbrand.Mark");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Sunbrand_Detonate, "GameplayCue.Sunbrand.Detonate");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Duo_Flash, "GameplayCue.Duo.Flash");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Duo_Absorb, "GameplayCue.Duo.Absorb");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Duo_Shockwave, "GameplayCue.Duo.Shockwave");

	UE_DEFINE_GAMEPLAY_TAG(Ability_Input_Primary, "Ability.Input.Primary");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Input_Q, "Ability.Input.Q");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Input_E, "Ability.Input.E");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Input_R, "Ability.Input.R");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Input_Dodge, "Ability.Input.Dodge");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Input_Duo, "Ability.Input.Duo");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Ability_Input_Unbound, "Ability.Input.Unbound", "Granted but on no key (duo powers not in the duo loadout)");
}
