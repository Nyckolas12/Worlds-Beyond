// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "Characters/BeyondCharacterBase.h"

#ifdef WORLDBEYOND_BeyondCharacterBase_generated_h
#error "BeyondCharacterBase.generated.h already included, missing '#pragma once' in BeyondCharacterBase.h"
#endif
#define WORLDBEYOND_BeyondCharacterBase_generated_h

#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

class UGameplayAbility;
struct FGameplayAbilitySpecHandle;
struct FGameplayEventData;

// ********** Begin Class ABeyondCharacterBase *****************************************************
#define FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_Characters_BeyondCharacterBase_h_15_RPC_WRAPPERS_NO_PURE_DECLS \
	virtual void ServerSendGameplayEventToSelf_Implementation(FGameplayEventData EventData); \
	virtual void HandleDeath_Implementation(); \
	DECLARE_FUNCTION(execServerSendGameplayEventToSelf); \
	DECLARE_FUNCTION(execSendAbilitiesChangedEvent); \
	DECLARE_FUNCTION(execRemoveAbilities); \
	DECLARE_FUNCTION(execGrantAbilities); \
	DECLARE_FUNCTION(execHandleDeath);


#define FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_Characters_BeyondCharacterBase_h_15_CALLBACK_WRAPPERS
WORLDBEYOND_API UClass* Z_Construct_UClass_ABeyondCharacterBase_NoRegister();

#define FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_Characters_BeyondCharacterBase_h_15_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesABeyondCharacterBase(); \
	friend struct Z_Construct_UClass_ABeyondCharacterBase_Statics; \
	static UClass* GetPrivateStaticClass(); \
	friend WORLDBEYOND_API UClass* Z_Construct_UClass_ABeyondCharacterBase_NoRegister(); \
public: \
	DECLARE_CLASS2(ABeyondCharacterBase, ACharacter, COMPILED_IN_FLAGS(0 | CLASS_Config), CASTCLASS_None, TEXT("/Script/WorldBeyond"), Z_Construct_UClass_ABeyondCharacterBase_NoRegister) \
	DECLARE_SERIALIZER(ABeyondCharacterBase) \
	virtual UObject* _getUObject() const override { return const_cast<ABeyondCharacterBase*>(this); }


#define FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_Characters_BeyondCharacterBase_h_15_ENHANCED_CONSTRUCTORS \
	/** Deleted move- and copy-constructors, should never be used */ \
	ABeyondCharacterBase(ABeyondCharacterBase&&) = delete; \
	ABeyondCharacterBase(const ABeyondCharacterBase&) = delete; \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, ABeyondCharacterBase); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(ABeyondCharacterBase); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(ABeyondCharacterBase) \
	NO_API virtual ~ABeyondCharacterBase();


#define FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_Characters_BeyondCharacterBase_h_12_PROLOG
#define FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_Characters_BeyondCharacterBase_h_15_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_Characters_BeyondCharacterBase_h_15_RPC_WRAPPERS_NO_PURE_DECLS \
	FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_Characters_BeyondCharacterBase_h_15_CALLBACK_WRAPPERS \
	FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_Characters_BeyondCharacterBase_h_15_INCLASS_NO_PURE_DECLS \
	FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_Characters_BeyondCharacterBase_h_15_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


class ABeyondCharacterBase;

// ********** End Class ABeyondCharacterBase *******************************************************

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_Characters_BeyondCharacterBase_h

PRAGMA_ENABLE_DEPRECATION_WARNINGS
