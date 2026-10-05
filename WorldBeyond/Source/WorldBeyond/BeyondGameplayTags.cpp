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
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Weapon_Equipped, "Event.Weapon.Equipped", "Equip / sheathe request; the weapon tag is in TargetTags");

	UE_DEFINE_GAMEPLAY_TAG(DamageType_Melee, "DamageType.Melee");
	UE_DEFINE_GAMEPLAY_TAG(DamageType_Projectile, "DamageType.Projectile");
	UE_DEFINE_GAMEPLAY_TAG(DamageType_Explosion, "DamageType.Explosion");
	UE_DEFINE_GAMEPLAY_TAG(DamageType_Environment, "DamageType.Environment");

	UE_DEFINE_GAMEPLAY_TAG(Damage_Unblockable, "Damage.Unblockable");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Unparryable, "Damage.Unparryable");
	UE_DEFINE_GAMEPLAY_TAG(Damage_IgnoreInvincible, "Damage.IgnoreInvincible");
	UE_DEFINE_GAMEPLAY_TAG(Damage_ForceInterrupt, "Damage.ForceInterrupt");

	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Damage, "SetByCaller.Damage");
	UE_DEFINE_GAMEPLAY_TAG(SetByCaller_Heal, "SetByCaller.Heal");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(SetByCaller_Duration, "SetByCaller.Duration", "Duration of UBeyondGE_Cooldown / UBeyondGE_Brand");

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
}
