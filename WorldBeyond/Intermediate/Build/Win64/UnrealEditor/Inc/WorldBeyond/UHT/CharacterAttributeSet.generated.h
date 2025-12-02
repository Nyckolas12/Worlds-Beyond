// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

// IWYU pragma: private, include "CharacterAttributeSet.h"

#ifdef WORLDBEYOND_CharacterAttributeSet_generated_h
#error "CharacterAttributeSet.generated.h already included, missing '#pragma once' in CharacterAttributeSet.h"
#endif
#define WORLDBEYOND_CharacterAttributeSet_generated_h

#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Net/Core/PushModel/PushModelMacros.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

struct FGameplayAttributeData;

// ********** Begin Class UCharacterAttributeSet ***************************************************
#define FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_CharacterAttributeSet_h_22_RPC_WRAPPERS_NO_PURE_DECLS \
	DECLARE_FUNCTION(execOnRep_MaxStamina); \
	DECLARE_FUNCTION(execOnRep_CurrentStamina); \
	DECLARE_FUNCTION(execOnRep_MaxHealth); \
	DECLARE_FUNCTION(execOnRep_CurrentHealth);


WORLDBEYOND_API UClass* Z_Construct_UClass_UCharacterAttributeSet_NoRegister();

#define FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_CharacterAttributeSet_h_22_INCLASS_NO_PURE_DECLS \
private: \
	static void StaticRegisterNativesUCharacterAttributeSet(); \
	friend struct Z_Construct_UClass_UCharacterAttributeSet_Statics; \
	static UClass* GetPrivateStaticClass(); \
	friend WORLDBEYOND_API UClass* Z_Construct_UClass_UCharacterAttributeSet_NoRegister(); \
public: \
	DECLARE_CLASS2(UCharacterAttributeSet, UAttributeSet, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/WorldBeyond"), Z_Construct_UClass_UCharacterAttributeSet_NoRegister) \
	DECLARE_SERIALIZER(UCharacterAttributeSet) \
	NO_API void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override; \
	enum class ENetFields_Private : uint16 \
	{ \
		NETFIELD_REP_START=(uint16)((int32)Super::ENetFields_Private::NETFIELD_REP_END + (int32)1), \
		CurrentHealth=NETFIELD_REP_START, \
		MaxHealth, \
		CurrentStamina, \
		MaxStamina, \
		NETFIELD_REP_END=MaxStamina	}; \
	DECLARE_VALIDATE_GENERATED_REP_ENUMS(NO_API) \
private: \
	REPLICATED_BASE_CLASS(UCharacterAttributeSet) \
public:


#define FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_CharacterAttributeSet_h_22_ENHANCED_CONSTRUCTORS \
	/** Deleted move- and copy-constructors, should never be used */ \
	UCharacterAttributeSet(UCharacterAttributeSet&&) = delete; \
	UCharacterAttributeSet(const UCharacterAttributeSet&) = delete; \
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, UCharacterAttributeSet); \
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(UCharacterAttributeSet); \
	DEFINE_DEFAULT_CONSTRUCTOR_CALL(UCharacterAttributeSet) \
	NO_API virtual ~UCharacterAttributeSet();


#define FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_CharacterAttributeSet_h_19_PROLOG
#define FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_CharacterAttributeSet_h_22_GENERATED_BODY \
PRAGMA_DISABLE_DEPRECATION_WARNINGS \
public: \
	FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_CharacterAttributeSet_h_22_RPC_WRAPPERS_NO_PURE_DECLS \
	FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_CharacterAttributeSet_h_22_INCLASS_NO_PURE_DECLS \
	FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_CharacterAttributeSet_h_22_ENHANCED_CONSTRUCTORS \
private: \
PRAGMA_ENABLE_DEPRECATION_WARNINGS


class UCharacterAttributeSet;

// ********** End Class UCharacterAttributeSet *****************************************************

#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_CharacterAttributeSet_h

PRAGMA_ENABLE_DEPRECATION_WARNINGS
