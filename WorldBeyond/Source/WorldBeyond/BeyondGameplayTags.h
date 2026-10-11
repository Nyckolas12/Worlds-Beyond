// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "NativeGameplayTags.h"

// Tags the C++ combat pipeline relies on. Declared natively so a typo is a compile error
// instead of a silently missing tag. Content-only tags stay in Config/DefaultGameplayTags.ini.
namespace BeyondTags
{
	// Character state (loose tags on the ASC)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Invincible);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Blocking);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Parrying);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Uninterruptible);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Stunned);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Duo);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Branded);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dashing);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Aegis);
	// An enemy walking back home after losing its target: takes no damage, refills when it arrives
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Resetting);
	// Warded elite: spells barely hurt it until a melee hit breaks the ward
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Warded);

	// Owned while an attack / cast ability runs (blocks other attacks)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Active);
	// Asset tag of enemy attacks (hit reactions interrupt them)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Enemy_Attack);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_HitReact);

	// Cooldown slots for enemy abilities (one per ability on the same enemy)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Enemy_Primary);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Enemy_Secondary);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Enemy_Special);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Enemy_Ultimate);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Enemy_Mobility);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Enemy_Summon);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Enemy_Beam);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Enemy_Buff);

	// Where an enemy belongs (roster regions)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Region_Forest);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Region_CorruptedWoods);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Region_Frost);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Region_Molten);

	// Elite affixes
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy_Affix);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy_Affix_Molten);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy_Affix_Venomous);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy_Affix_Stormcharged);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy_Affix_Warded);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy_Affix_Juggernaut);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy_Affix_Swift);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy_Affix_Vampiric);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Enemy_Affix_Brood);

	// Boss phases (loose tags on the boss; phase-gated abilities require them)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Boss_Phase_1);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Boss_Phase_2);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Boss_Phase_3);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Boss_Phase_4);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Boss_Enraged);

	// Gameplay events sent to the damaged character (mirror E_DamageResponse)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Hit);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Hit_Light);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Hit_Stagger);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Hit_Stun);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Hit_KnockBack);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Hit_Blocked);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Hit_Parried);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Death);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Abilities_Changed);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Montage_Trigger);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_ShootProjectile);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Weapon_Equipped);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Progression_LevelUp);

	// Instigator tags on Event.Weapon.Equipped (no action tag = toggle)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_Action_Draw);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_Action_Sheathe);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_Action_Instant);

	// Damage classification carried as dynamic asset tags on the damage spec (mirror E_DamageType)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageType_Melee);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageType_Projectile);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageType_Explosion);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageType_Environment);
	// Damage from armor-set effects (poison, burn, chain lightning): never triggers set effects itself
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageType_Proc);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageType_Proc_Poison);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageType_Proc_Burn);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(DamageType_Proc_Lightning);

	// Damage flags (mirror S_DamageInfo booleans)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Unblockable);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Unparryable);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_IgnoreInvincible);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_ForceInterrupt);

	// SetByCaller magnitudes
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Damage);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Heal);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Duration);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Experience);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_MaxHealth);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_MaxStamina);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Strength);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Arcana);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(SetByCaller_Defense);

	// Gameplay cues fired by the shared damage / heal effects
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Damage_Burst);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Heal_Burst);

	// Ji-Woong's powers and the duo super move
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_GildedStep_Trail);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_GildedStep_Detonate);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sunbrand_Mark);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Sunbrand_Detonate);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Duo_Flash);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Duo_Absorb);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Duo_Shockwave);

	// Ability input slots
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Input_Primary);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Input_Q);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Input_E);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Input_R);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Input_Dodge);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Input_Duo);
	// A granted ability no key activates (duo powers not on the duo slot)
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Input_Unbound);
}
