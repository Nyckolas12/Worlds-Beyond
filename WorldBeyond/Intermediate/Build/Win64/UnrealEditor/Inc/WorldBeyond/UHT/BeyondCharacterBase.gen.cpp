// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "WorldBeyond/Characters/BeyondCharacterBase.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "GameplayAbilitySpecHandle.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

void EmptyLinkFunctionForGeneratedCodeBeyondCharacterBase() {}

// ********** Begin Cross Module References ********************************************************
COREUOBJECT_API UClass* Z_Construct_UClass_UClass();
ENGINE_API UClass* Z_Construct_UClass_ACharacter();
GAMEPLAYABILITIES_API UClass* Z_Construct_UClass_UAbilitySystemComponent_NoRegister();
GAMEPLAYABILITIES_API UClass* Z_Construct_UClass_UAbilitySystemInterface_NoRegister();
GAMEPLAYABILITIES_API UClass* Z_Construct_UClass_UGameplayAbility_NoRegister();
GAMEPLAYABILITIES_API UEnum* Z_Construct_UEnum_GameplayAbilities_EGameplayEffectReplicationMode();
GAMEPLAYABILITIES_API UScriptStruct* Z_Construct_UScriptStruct_FGameplayAbilitySpecHandle();
GAMEPLAYABILITIES_API UScriptStruct* Z_Construct_UScriptStruct_FGameplayEventData();
UPackage* Z_Construct_UPackage__Script_WorldBeyond();
WORLDBEYOND_API UClass* Z_Construct_UClass_ABeyondCharacterBase();
WORLDBEYOND_API UClass* Z_Construct_UClass_ABeyondCharacterBase_NoRegister();
WORLDBEYOND_API UClass* Z_Construct_UClass_UCharacterAttributeSet_NoRegister();
// ********** End Cross Module References **********************************************************

// ********** Begin Class ABeyondCharacterBase Function GrantAbilities *****************************
struct Z_Construct_UFunction_ABeyondCharacterBase_GrantAbilities_Statics
{
	struct BeyondCharacterBase_eventGrantAbilities_Parms
	{
		TArray<TSubclassOf<UGameplayAbility>> AbilitiesToGrant;
		TArray<FGameplayAbilitySpecHandle> ReturnValue;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "AbilitySystem" },
		{ "ModuleRelativePath", "Characters/BeyondCharacterBase.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FClassPropertyParams NewProp_AbilitiesToGrant_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_AbilitiesToGrant;
	static const UECodeGen_Private::FStructPropertyParams NewProp_ReturnValue_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_ReturnValue;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FClassPropertyParams Z_Construct_UFunction_ABeyondCharacterBase_GrantAbilities_Statics::NewProp_AbilitiesToGrant_Inner = { "AbilitiesToGrant", nullptr, (EPropertyFlags)0x0004000000000000, UECodeGen_Private::EPropertyGenFlags::Class, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, Z_Construct_UClass_UClass, Z_Construct_UClass_UGameplayAbility_NoRegister, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams Z_Construct_UFunction_ABeyondCharacterBase_GrantAbilities_Statics::NewProp_AbilitiesToGrant = { "AbilitiesToGrant", nullptr, (EPropertyFlags)0x0014000000000080, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(BeyondCharacterBase_eventGrantAbilities_Parms, AbilitiesToGrant), EArrayPropertyFlags::None, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_ABeyondCharacterBase_GrantAbilities_Statics::NewProp_ReturnValue_Inner = { "ReturnValue", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, Z_Construct_UScriptStruct_FGameplayAbilitySpecHandle, METADATA_PARAMS(0, nullptr) }; // 417001783
const UECodeGen_Private::FArrayPropertyParams Z_Construct_UFunction_ABeyondCharacterBase_GrantAbilities_Statics::NewProp_ReturnValue = { "ReturnValue", nullptr, (EPropertyFlags)0x0010000000000580, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(BeyondCharacterBase_eventGrantAbilities_Parms, ReturnValue), EArrayPropertyFlags::None, METADATA_PARAMS(0, nullptr) }; // 417001783
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_ABeyondCharacterBase_GrantAbilities_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_ABeyondCharacterBase_GrantAbilities_Statics::NewProp_AbilitiesToGrant_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_ABeyondCharacterBase_GrantAbilities_Statics::NewProp_AbilitiesToGrant,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_ABeyondCharacterBase_GrantAbilities_Statics::NewProp_ReturnValue_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_ABeyondCharacterBase_GrantAbilities_Statics::NewProp_ReturnValue,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_ABeyondCharacterBase_GrantAbilities_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_ABeyondCharacterBase_GrantAbilities_Statics::FuncParams = { { (UObject*(*)())Z_Construct_UClass_ABeyondCharacterBase, nullptr, "GrantAbilities", Z_Construct_UFunction_ABeyondCharacterBase_GrantAbilities_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_ABeyondCharacterBase_GrantAbilities_Statics::PropPointers), sizeof(Z_Construct_UFunction_ABeyondCharacterBase_GrantAbilities_Statics::BeyondCharacterBase_eventGrantAbilities_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_ABeyondCharacterBase_GrantAbilities_Statics::Function_MetaDataParams), Z_Construct_UFunction_ABeyondCharacterBase_GrantAbilities_Statics::Function_MetaDataParams)},  };
static_assert(sizeof(Z_Construct_UFunction_ABeyondCharacterBase_GrantAbilities_Statics::BeyondCharacterBase_eventGrantAbilities_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_ABeyondCharacterBase_GrantAbilities()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_ABeyondCharacterBase_GrantAbilities_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(ABeyondCharacterBase::execGrantAbilities)
{
	P_GET_TARRAY(TSubclassOf<UGameplayAbility>,Z_Param_AbilitiesToGrant);
	P_FINISH;
	P_NATIVE_BEGIN;
	*(TArray<FGameplayAbilitySpecHandle>*)Z_Param__Result=P_THIS->GrantAbilities(Z_Param_AbilitiesToGrant);
	P_NATIVE_END;
}
// ********** End Class ABeyondCharacterBase Function GrantAbilities *******************************

// ********** Begin Class ABeyondCharacterBase Function HandleDeath ********************************
static FName NAME_ABeyondCharacterBase_HandleDeath = FName(TEXT("HandleDeath"));
void ABeyondCharacterBase::HandleDeath()
{
	UFunction* Func = FindFunctionChecked(NAME_ABeyondCharacterBase_HandleDeath);
	if (!Func->GetOwnerClass()->HasAnyClassFlags(CLASS_Native))
	{
	ProcessEvent(Func,NULL);
	}
	else
	{
		HandleDeath_Implementation();
	}
}
struct Z_Construct_UFunction_ABeyondCharacterBase_HandleDeath_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "Damage" },
		{ "ModuleRelativePath", "Characters/BeyondCharacterBase.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_ABeyondCharacterBase_HandleDeath_Statics::FuncParams = { { (UObject*(*)())Z_Construct_UClass_ABeyondCharacterBase, nullptr, "HandleDeath", nullptr, 0, 0, RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x0C080C00, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_ABeyondCharacterBase_HandleDeath_Statics::Function_MetaDataParams), Z_Construct_UFunction_ABeyondCharacterBase_HandleDeath_Statics::Function_MetaDataParams)},  };
UFunction* Z_Construct_UFunction_ABeyondCharacterBase_HandleDeath()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_ABeyondCharacterBase_HandleDeath_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(ABeyondCharacterBase::execHandleDeath)
{
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->HandleDeath_Implementation();
	P_NATIVE_END;
}
// ********** End Class ABeyondCharacterBase Function HandleDeath **********************************

// ********** Begin Class ABeyondCharacterBase Function RemoveAbilities ****************************
struct Z_Construct_UFunction_ABeyondCharacterBase_RemoveAbilities_Statics
{
	struct BeyondCharacterBase_eventRemoveAbilities_Parms
	{
		TArray<FGameplayAbilitySpecHandle> AbilityHandlesToRemove;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "AbilitySystem" },
		{ "ModuleRelativePath", "Characters/BeyondCharacterBase.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStructPropertyParams NewProp_AbilityHandlesToRemove_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_AbilityHandlesToRemove;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_ABeyondCharacterBase_RemoveAbilities_Statics::NewProp_AbilityHandlesToRemove_Inner = { "AbilityHandlesToRemove", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, Z_Construct_UScriptStruct_FGameplayAbilitySpecHandle, METADATA_PARAMS(0, nullptr) }; // 417001783
const UECodeGen_Private::FArrayPropertyParams Z_Construct_UFunction_ABeyondCharacterBase_RemoveAbilities_Statics::NewProp_AbilityHandlesToRemove = { "AbilityHandlesToRemove", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(BeyondCharacterBase_eventRemoveAbilities_Parms, AbilityHandlesToRemove), EArrayPropertyFlags::None, METADATA_PARAMS(0, nullptr) }; // 417001783
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_ABeyondCharacterBase_RemoveAbilities_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_ABeyondCharacterBase_RemoveAbilities_Statics::NewProp_AbilityHandlesToRemove_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_ABeyondCharacterBase_RemoveAbilities_Statics::NewProp_AbilityHandlesToRemove,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_ABeyondCharacterBase_RemoveAbilities_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_ABeyondCharacterBase_RemoveAbilities_Statics::FuncParams = { { (UObject*(*)())Z_Construct_UClass_ABeyondCharacterBase, nullptr, "RemoveAbilities", Z_Construct_UFunction_ABeyondCharacterBase_RemoveAbilities_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_ABeyondCharacterBase_RemoveAbilities_Statics::PropPointers), sizeof(Z_Construct_UFunction_ABeyondCharacterBase_RemoveAbilities_Statics::BeyondCharacterBase_eventRemoveAbilities_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_ABeyondCharacterBase_RemoveAbilities_Statics::Function_MetaDataParams), Z_Construct_UFunction_ABeyondCharacterBase_RemoveAbilities_Statics::Function_MetaDataParams)},  };
static_assert(sizeof(Z_Construct_UFunction_ABeyondCharacterBase_RemoveAbilities_Statics::BeyondCharacterBase_eventRemoveAbilities_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_ABeyondCharacterBase_RemoveAbilities()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_ABeyondCharacterBase_RemoveAbilities_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(ABeyondCharacterBase::execRemoveAbilities)
{
	P_GET_TARRAY(FGameplayAbilitySpecHandle,Z_Param_AbilityHandlesToRemove);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->RemoveAbilities(Z_Param_AbilityHandlesToRemove);
	P_NATIVE_END;
}
// ********** End Class ABeyondCharacterBase Function RemoveAbilities ******************************

// ********** Begin Class ABeyondCharacterBase Function SendAbilitiesChangedEvent ******************
struct Z_Construct_UFunction_ABeyondCharacterBase_SendAbilitiesChangedEvent_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "AbilitySystem" },
		{ "ModuleRelativePath", "Characters/BeyondCharacterBase.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_ABeyondCharacterBase_SendAbilitiesChangedEvent_Statics::FuncParams = { { (UObject*(*)())Z_Construct_UClass_ABeyondCharacterBase, nullptr, "SendAbilitiesChangedEvent", nullptr, 0, 0, RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04020401, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_ABeyondCharacterBase_SendAbilitiesChangedEvent_Statics::Function_MetaDataParams), Z_Construct_UFunction_ABeyondCharacterBase_SendAbilitiesChangedEvent_Statics::Function_MetaDataParams)},  };
UFunction* Z_Construct_UFunction_ABeyondCharacterBase_SendAbilitiesChangedEvent()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_ABeyondCharacterBase_SendAbilitiesChangedEvent_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(ABeyondCharacterBase::execSendAbilitiesChangedEvent)
{
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->SendAbilitiesChangedEvent();
	P_NATIVE_END;
}
// ********** End Class ABeyondCharacterBase Function SendAbilitiesChangedEvent ********************

// ********** Begin Class ABeyondCharacterBase Function ServerSendGameplayEventToSelf **************
struct BeyondCharacterBase_eventServerSendGameplayEventToSelf_Parms
{
	FGameplayEventData EventData;
};
static FName NAME_ABeyondCharacterBase_ServerSendGameplayEventToSelf = FName(TEXT("ServerSendGameplayEventToSelf"));
void ABeyondCharacterBase::ServerSendGameplayEventToSelf(FGameplayEventData EventData)
{
	BeyondCharacterBase_eventServerSendGameplayEventToSelf_Parms Parms;
	Parms.EventData=EventData;
	UFunction* Func = FindFunctionChecked(NAME_ABeyondCharacterBase_ServerSendGameplayEventToSelf);
	ProcessEvent(Func,&Parms);
}
struct Z_Construct_UFunction_ABeyondCharacterBase_ServerSendGameplayEventToSelf_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "Category", "AbilitySystem" },
		{ "ModuleRelativePath", "Characters/BeyondCharacterBase.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStructPropertyParams NewProp_EventData;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_ABeyondCharacterBase_ServerSendGameplayEventToSelf_Statics::NewProp_EventData = { "EventData", nullptr, (EPropertyFlags)0x0010000000000080, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(BeyondCharacterBase_eventServerSendGameplayEventToSelf_Parms, EventData), Z_Construct_UScriptStruct_FGameplayEventData, METADATA_PARAMS(0, nullptr) }; // 924940328
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_ABeyondCharacterBase_ServerSendGameplayEventToSelf_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_ABeyondCharacterBase_ServerSendGameplayEventToSelf_Statics::NewProp_EventData,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_ABeyondCharacterBase_ServerSendGameplayEventToSelf_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_ABeyondCharacterBase_ServerSendGameplayEventToSelf_Statics::FuncParams = { { (UObject*(*)())Z_Construct_UClass_ABeyondCharacterBase, nullptr, "ServerSendGameplayEventToSelf", Z_Construct_UFunction_ABeyondCharacterBase_ServerSendGameplayEventToSelf_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_ABeyondCharacterBase_ServerSendGameplayEventToSelf_Statics::PropPointers), sizeof(BeyondCharacterBase_eventServerSendGameplayEventToSelf_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x04220CC0, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_ABeyondCharacterBase_ServerSendGameplayEventToSelf_Statics::Function_MetaDataParams), Z_Construct_UFunction_ABeyondCharacterBase_ServerSendGameplayEventToSelf_Statics::Function_MetaDataParams)},  };
static_assert(sizeof(BeyondCharacterBase_eventServerSendGameplayEventToSelf_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_ABeyondCharacterBase_ServerSendGameplayEventToSelf()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_ABeyondCharacterBase_ServerSendGameplayEventToSelf_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(ABeyondCharacterBase::execServerSendGameplayEventToSelf)
{
	P_GET_STRUCT(FGameplayEventData,Z_Param_EventData);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->ServerSendGameplayEventToSelf_Implementation(Z_Param_EventData);
	P_NATIVE_END;
}
// ********** End Class ABeyondCharacterBase Function ServerSendGameplayEventToSelf ****************

// ********** Begin Class ABeyondCharacterBase *****************************************************
void ABeyondCharacterBase::StaticRegisterNativesABeyondCharacterBase()
{
	UClass* Class = ABeyondCharacterBase::StaticClass();
	static const FNameNativePtrPair Funcs[] = {
		{ "GrantAbilities", &ABeyondCharacterBase::execGrantAbilities },
		{ "HandleDeath", &ABeyondCharacterBase::execHandleDeath },
		{ "RemoveAbilities", &ABeyondCharacterBase::execRemoveAbilities },
		{ "SendAbilitiesChangedEvent", &ABeyondCharacterBase::execSendAbilitiesChangedEvent },
		{ "ServerSendGameplayEventToSelf", &ABeyondCharacterBase::execServerSendGameplayEventToSelf },
	};
	FNativeFunctionRegistrar::RegisterFunctions(Class, Funcs, UE_ARRAY_COUNT(Funcs));
}
FClassRegistrationInfo Z_Registration_Info_UClass_ABeyondCharacterBase;
UClass* ABeyondCharacterBase::GetPrivateStaticClass()
{
	using TClass = ABeyondCharacterBase;
	if (!Z_Registration_Info_UClass_ABeyondCharacterBase.InnerSingleton)
	{
		GetPrivateStaticClassBody(
			StaticPackage(),
			TEXT("BeyondCharacterBase"),
			Z_Registration_Info_UClass_ABeyondCharacterBase.InnerSingleton,
			StaticRegisterNativesABeyondCharacterBase,
			sizeof(TClass),
			alignof(TClass),
			TClass::StaticClassFlags,
			TClass::StaticClassCastFlags(),
			TClass::StaticConfigName(),
			(UClass::ClassConstructorType)InternalConstructor<TClass>,
			(UClass::ClassVTableHelperCtorCallerType)InternalVTableHelperCtorCaller<TClass>,
			UOBJECT_CPPCLASS_STATICFUNCTIONS_FORCLASS(TClass),
			&TClass::Super::StaticClass,
			&TClass::WithinClass::StaticClass
		);
	}
	return Z_Registration_Info_UClass_ABeyondCharacterBase.InnerSingleton;
}
UClass* Z_Construct_UClass_ABeyondCharacterBase_NoRegister()
{
	return ABeyondCharacterBase::GetPrivateStaticClass();
}
struct Z_Construct_UClass_ABeyondCharacterBase_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
		{ "HideCategories", "Navigation" },
		{ "IncludePath", "Characters/BeyondCharacterBase.h" },
		{ "ModuleRelativePath", "Characters/BeyondCharacterBase.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_AbilitySystemComponent_MetaData[] = {
		{ "Category", "AbilitySystem" },
#if !UE_BUILD_SHIPPING
		{ "Comment", "//Ability System Component\n" },
#endif
		{ "EditInline", "true" },
		{ "ModuleRelativePath", "Characters/BeyondCharacterBase.h" },
#if !UE_BUILD_SHIPPING
		{ "ToolTip", "Ability System Component" },
#endif
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_AttributeSet_MetaData[] = {
		{ "AllowPrivateAccess", "true" },
		{ "Category", "Attributes" },
		{ "EditInline", "true" },
		{ "ModuleRelativePath", "Characters/BeyondCharacterBase.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ASCReplicationMode_MetaData[] = {
		{ "Category", "AbilitySystem" },
		{ "ModuleRelativePath", "Characters/BeyondCharacterBase.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_StartingAbilities_MetaData[] = {
		{ "Category", "AbilitySystem" },
		{ "ModuleRelativePath", "Characters/BeyondCharacterBase.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FObjectPropertyParams NewProp_AbilitySystemComponent;
	static const UECodeGen_Private::FObjectPropertyParams NewProp_AttributeSet;
	static const UECodeGen_Private::FBytePropertyParams NewProp_ASCReplicationMode_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_ASCReplicationMode;
	static const UECodeGen_Private::FClassPropertyParams NewProp_StartingAbilities_Inner;
	static const UECodeGen_Private::FArrayPropertyParams NewProp_StartingAbilities;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FClassFunctionLinkInfo FuncInfo[] = {
		{ &Z_Construct_UFunction_ABeyondCharacterBase_GrantAbilities, "GrantAbilities" }, // 3822998545
		{ &Z_Construct_UFunction_ABeyondCharacterBase_HandleDeath, "HandleDeath" }, // 3181988539
		{ &Z_Construct_UFunction_ABeyondCharacterBase_RemoveAbilities, "RemoveAbilities" }, // 3745364620
		{ &Z_Construct_UFunction_ABeyondCharacterBase_SendAbilitiesChangedEvent, "SendAbilitiesChangedEvent" }, // 2820691442
		{ &Z_Construct_UFunction_ABeyondCharacterBase_ServerSendGameplayEventToSelf, "ServerSendGameplayEventToSelf" }, // 3495881999
	};
	static_assert(UE_ARRAY_COUNT(FuncInfo) < 2048);
	static const UECodeGen_Private::FImplementedInterfaceParams InterfaceParams[];
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<ABeyondCharacterBase>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FObjectPropertyParams Z_Construct_UClass_ABeyondCharacterBase_Statics::NewProp_AbilitySystemComponent = { "AbilitySystemComponent", nullptr, (EPropertyFlags)0x00100000000a001d, UECodeGen_Private::EPropertyGenFlags::Object, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ABeyondCharacterBase, AbilitySystemComponent), Z_Construct_UClass_UAbilitySystemComponent_NoRegister, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_AbilitySystemComponent_MetaData), NewProp_AbilitySystemComponent_MetaData) };
const UECodeGen_Private::FObjectPropertyParams Z_Construct_UClass_ABeyondCharacterBase_Statics::NewProp_AttributeSet = { "AttributeSet", nullptr, (EPropertyFlags)0x00100000000a001d, UECodeGen_Private::EPropertyGenFlags::Object, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ABeyondCharacterBase, AttributeSet), Z_Construct_UClass_UCharacterAttributeSet_NoRegister, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_AttributeSet_MetaData), NewProp_AttributeSet_MetaData) };
const UECodeGen_Private::FBytePropertyParams Z_Construct_UClass_ABeyondCharacterBase_Statics::NewProp_ASCReplicationMode_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UClass_ABeyondCharacterBase_Statics::NewProp_ASCReplicationMode = { "ASCReplicationMode", nullptr, (EPropertyFlags)0x0020080000000005, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ABeyondCharacterBase, ASCReplicationMode), Z_Construct_UEnum_GameplayAbilities_EGameplayEffectReplicationMode, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ASCReplicationMode_MetaData), NewProp_ASCReplicationMode_MetaData) }; // 3979288675
const UECodeGen_Private::FClassPropertyParams Z_Construct_UClass_ABeyondCharacterBase_Statics::NewProp_StartingAbilities_Inner = { "StartingAbilities", nullptr, (EPropertyFlags)0x0004000000000000, UECodeGen_Private::EPropertyGenFlags::Class, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, Z_Construct_UClass_UClass, Z_Construct_UClass_UGameplayAbility_NoRegister, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FArrayPropertyParams Z_Construct_UClass_ABeyondCharacterBase_Statics::NewProp_StartingAbilities = { "StartingAbilities", nullptr, (EPropertyFlags)0x0024080000000005, UECodeGen_Private::EPropertyGenFlags::Array, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ABeyondCharacterBase, StartingAbilities), EArrayPropertyFlags::None, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_StartingAbilities_MetaData), NewProp_StartingAbilities_MetaData) };
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_ABeyondCharacterBase_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ABeyondCharacterBase_Statics::NewProp_AbilitySystemComponent,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ABeyondCharacterBase_Statics::NewProp_AttributeSet,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ABeyondCharacterBase_Statics::NewProp_ASCReplicationMode_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ABeyondCharacterBase_Statics::NewProp_ASCReplicationMode,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ABeyondCharacterBase_Statics::NewProp_StartingAbilities_Inner,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ABeyondCharacterBase_Statics::NewProp_StartingAbilities,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_ABeyondCharacterBase_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_ABeyondCharacterBase_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_ACharacter,
	(UObject* (*)())Z_Construct_UPackage__Script_WorldBeyond,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_ABeyondCharacterBase_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FImplementedInterfaceParams Z_Construct_UClass_ABeyondCharacterBase_Statics::InterfaceParams[] = {
	{ Z_Construct_UClass_UAbilitySystemInterface_NoRegister, (int32)VTABLE_OFFSET(ABeyondCharacterBase, IAbilitySystemInterface), false },  // 1199015870
};
const UECodeGen_Private::FClassParams Z_Construct_UClass_ABeyondCharacterBase_Statics::ClassParams = {
	&ABeyondCharacterBase::StaticClass,
	"Game",
	&StaticCppClassTypeInfo,
	DependentSingletons,
	FuncInfo,
	Z_Construct_UClass_ABeyondCharacterBase_Statics::PropPointers,
	InterfaceParams,
	UE_ARRAY_COUNT(DependentSingletons),
	UE_ARRAY_COUNT(FuncInfo),
	UE_ARRAY_COUNT(Z_Construct_UClass_ABeyondCharacterBase_Statics::PropPointers),
	UE_ARRAY_COUNT(InterfaceParams),
	0x009001A4u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_ABeyondCharacterBase_Statics::Class_MetaDataParams), Z_Construct_UClass_ABeyondCharacterBase_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_ABeyondCharacterBase()
{
	if (!Z_Registration_Info_UClass_ABeyondCharacterBase.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_ABeyondCharacterBase.OuterSingleton, Z_Construct_UClass_ABeyondCharacterBase_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_ABeyondCharacterBase.OuterSingleton;
}
DEFINE_VTABLE_PTR_HELPER_CTOR(ABeyondCharacterBase);
ABeyondCharacterBase::~ABeyondCharacterBase() {}
// ********** End Class ABeyondCharacterBase *******************************************************

// ********** Begin Registration *******************************************************************
struct Z_CompiledInDeferFile_FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_Characters_BeyondCharacterBase_h__Script_WorldBeyond_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_ABeyondCharacterBase, ABeyondCharacterBase::StaticClass, TEXT("ABeyondCharacterBase"), &Z_Registration_Info_UClass_ABeyondCharacterBase, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(ABeyondCharacterBase), 2729334064U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_Characters_BeyondCharacterBase_h__Script_WorldBeyond_2627847112(TEXT("/Script/WorldBeyond"),
	Z_CompiledInDeferFile_FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_Characters_BeyondCharacterBase_h__Script_WorldBeyond_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_Characters_BeyondCharacterBase_h__Script_WorldBeyond_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0);
// ********** End Registration *********************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
