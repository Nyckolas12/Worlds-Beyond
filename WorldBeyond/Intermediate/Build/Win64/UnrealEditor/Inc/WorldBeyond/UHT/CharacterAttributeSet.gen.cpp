// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "WorldBeyond/CharacterAttributeSet.h"
#include "AttributeSet.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

void EmptyLinkFunctionForGeneratedCodeCharacterAttributeSet() {}

// ********** Begin Cross Module References ********************************************************
GAMEPLAYABILITIES_API UClass* Z_Construct_UClass_UAttributeSet();
GAMEPLAYABILITIES_API UScriptStruct* Z_Construct_UScriptStruct_FGameplayAttributeData();
UPackage* Z_Construct_UPackage__Script_WorldBeyond();
WORLDBEYOND_API UClass* Z_Construct_UClass_UCharacterAttributeSet();
WORLDBEYOND_API UClass* Z_Construct_UClass_UCharacterAttributeSet_NoRegister();
// ********** End Cross Module References **********************************************************

// ********** Begin Class UCharacterAttributeSet Function OnRep_CurrentHealth **********************
struct Z_Construct_UFunction_UCharacterAttributeSet_OnRep_CurrentHealth_Statics
{
	struct CharacterAttributeSet_eventOnRep_CurrentHealth_Parms
	{
		FGameplayAttributeData OldCurrentHealth;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "ModuleRelativePath", "CharacterAttributeSet.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_OldCurrentHealth_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStructPropertyParams NewProp_OldCurrentHealth;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UCharacterAttributeSet_OnRep_CurrentHealth_Statics::NewProp_OldCurrentHealth = { "OldCurrentHealth", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(CharacterAttributeSet_eventOnRep_CurrentHealth_Parms, OldCurrentHealth), Z_Construct_UScriptStruct_FGameplayAttributeData, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_OldCurrentHealth_MetaData), NewProp_OldCurrentHealth_MetaData) }; // 1532612004
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UCharacterAttributeSet_OnRep_CurrentHealth_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UCharacterAttributeSet_OnRep_CurrentHealth_Statics::NewProp_OldCurrentHealth,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UCharacterAttributeSet_OnRep_CurrentHealth_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UCharacterAttributeSet_OnRep_CurrentHealth_Statics::FuncParams = { { (UObject*(*)())Z_Construct_UClass_UCharacterAttributeSet, nullptr, "OnRep_CurrentHealth", Z_Construct_UFunction_UCharacterAttributeSet_OnRep_CurrentHealth_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UCharacterAttributeSet_OnRep_CurrentHealth_Statics::PropPointers), sizeof(Z_Construct_UFunction_UCharacterAttributeSet_OnRep_CurrentHealth_Statics::CharacterAttributeSet_eventOnRep_CurrentHealth_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x00480400, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UCharacterAttributeSet_OnRep_CurrentHealth_Statics::Function_MetaDataParams), Z_Construct_UFunction_UCharacterAttributeSet_OnRep_CurrentHealth_Statics::Function_MetaDataParams)},  };
static_assert(sizeof(Z_Construct_UFunction_UCharacterAttributeSet_OnRep_CurrentHealth_Statics::CharacterAttributeSet_eventOnRep_CurrentHealth_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UCharacterAttributeSet_OnRep_CurrentHealth()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UCharacterAttributeSet_OnRep_CurrentHealth_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UCharacterAttributeSet::execOnRep_CurrentHealth)
{
	P_GET_STRUCT_REF(FGameplayAttributeData,Z_Param_Out_OldCurrentHealth);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->OnRep_CurrentHealth(Z_Param_Out_OldCurrentHealth);
	P_NATIVE_END;
}
// ********** End Class UCharacterAttributeSet Function OnRep_CurrentHealth ************************

// ********** Begin Class UCharacterAttributeSet Function OnRep_MaxHealth **************************
struct Z_Construct_UFunction_UCharacterAttributeSet_OnRep_MaxHealth_Statics
{
	struct CharacterAttributeSet_eventOnRep_MaxHealth_Parms
	{
		FGameplayAttributeData OldMaxHealth;
	};
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Function_MetaDataParams[] = {
		{ "ModuleRelativePath", "CharacterAttributeSet.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_OldMaxHealth_MetaData[] = {
		{ "NativeConst", "" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStructPropertyParams NewProp_OldMaxHealth;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static const UECodeGen_Private::FFunctionParams FuncParams;
};
const UECodeGen_Private::FStructPropertyParams Z_Construct_UFunction_UCharacterAttributeSet_OnRep_MaxHealth_Statics::NewProp_OldMaxHealth = { "OldMaxHealth", nullptr, (EPropertyFlags)0x0010000008000182, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(CharacterAttributeSet_eventOnRep_MaxHealth_Parms, OldMaxHealth), Z_Construct_UScriptStruct_FGameplayAttributeData, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_OldMaxHealth_MetaData), NewProp_OldMaxHealth_MetaData) }; // 1532612004
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UFunction_UCharacterAttributeSet_OnRep_MaxHealth_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UFunction_UCharacterAttributeSet_OnRep_MaxHealth_Statics::NewProp_OldMaxHealth,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UFunction_UCharacterAttributeSet_OnRep_MaxHealth_Statics::PropPointers) < 2048);
const UECodeGen_Private::FFunctionParams Z_Construct_UFunction_UCharacterAttributeSet_OnRep_MaxHealth_Statics::FuncParams = { { (UObject*(*)())Z_Construct_UClass_UCharacterAttributeSet, nullptr, "OnRep_MaxHealth", Z_Construct_UFunction_UCharacterAttributeSet_OnRep_MaxHealth_Statics::PropPointers, UE_ARRAY_COUNT(Z_Construct_UFunction_UCharacterAttributeSet_OnRep_MaxHealth_Statics::PropPointers), sizeof(Z_Construct_UFunction_UCharacterAttributeSet_OnRep_MaxHealth_Statics::CharacterAttributeSet_eventOnRep_MaxHealth_Parms), RF_Public|RF_Transient|RF_MarkAsNative, (EFunctionFlags)0x00480400, 0, 0, METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UFunction_UCharacterAttributeSet_OnRep_MaxHealth_Statics::Function_MetaDataParams), Z_Construct_UFunction_UCharacterAttributeSet_OnRep_MaxHealth_Statics::Function_MetaDataParams)},  };
static_assert(sizeof(Z_Construct_UFunction_UCharacterAttributeSet_OnRep_MaxHealth_Statics::CharacterAttributeSet_eventOnRep_MaxHealth_Parms) < MAX_uint16);
UFunction* Z_Construct_UFunction_UCharacterAttributeSet_OnRep_MaxHealth()
{
	static UFunction* ReturnFunction = nullptr;
	if (!ReturnFunction)
	{
		UECodeGen_Private::ConstructUFunction(&ReturnFunction, Z_Construct_UFunction_UCharacterAttributeSet_OnRep_MaxHealth_Statics::FuncParams);
	}
	return ReturnFunction;
}
DEFINE_FUNCTION(UCharacterAttributeSet::execOnRep_MaxHealth)
{
	P_GET_STRUCT_REF(FGameplayAttributeData,Z_Param_Out_OldMaxHealth);
	P_FINISH;
	P_NATIVE_BEGIN;
	P_THIS->OnRep_MaxHealth(Z_Param_Out_OldMaxHealth);
	P_NATIVE_END;
}
// ********** End Class UCharacterAttributeSet Function OnRep_MaxHealth ****************************

// ********** Begin Class UCharacterAttributeSet ***************************************************
void UCharacterAttributeSet::StaticRegisterNativesUCharacterAttributeSet()
{
	UClass* Class = UCharacterAttributeSet::StaticClass();
	static const FNameNativePtrPair Funcs[] = {
		{ "OnRep_CurrentHealth", &UCharacterAttributeSet::execOnRep_CurrentHealth },
		{ "OnRep_MaxHealth", &UCharacterAttributeSet::execOnRep_MaxHealth },
	};
	FNativeFunctionRegistrar::RegisterFunctions(Class, Funcs, UE_ARRAY_COUNT(Funcs));
}
FClassRegistrationInfo Z_Registration_Info_UClass_UCharacterAttributeSet;
UClass* UCharacterAttributeSet::GetPrivateStaticClass()
{
	using TClass = UCharacterAttributeSet;
	if (!Z_Registration_Info_UClass_UCharacterAttributeSet.InnerSingleton)
	{
		GetPrivateStaticClassBody(
			StaticPackage(),
			TEXT("CharacterAttributeSet"),
			Z_Registration_Info_UClass_UCharacterAttributeSet.InnerSingleton,
			StaticRegisterNativesUCharacterAttributeSet,
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
	return Z_Registration_Info_UClass_UCharacterAttributeSet.InnerSingleton;
}
UClass* Z_Construct_UClass_UCharacterAttributeSet_NoRegister()
{
	return UCharacterAttributeSet::GetPrivateStaticClass();
}
struct Z_Construct_UClass_UCharacterAttributeSet_Statics
{
#if WITH_METADATA
	static constexpr UECodeGen_Private::FMetaDataPairParam Class_MetaDataParams[] = {
#if !UE_BUILD_SHIPPING
		{ "Comment", "/**\n * \n */" },
#endif
		{ "IncludePath", "CharacterAttributeSet.h" },
		{ "ModuleRelativePath", "CharacterAttributeSet.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_CurrentHealth_MetaData[] = {
		{ "Category", "Health" },
		{ "ModuleRelativePath", "CharacterAttributeSet.h" },
	};
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_MaxHealth_MetaData[] = {
		{ "Category", "Health" },
		{ "ModuleRelativePath", "CharacterAttributeSet.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FStructPropertyParams NewProp_CurrentHealth;
	static const UECodeGen_Private::FStructPropertyParams NewProp_MaxHealth;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static constexpr FClassFunctionLinkInfo FuncInfo[] = {
		{ &Z_Construct_UFunction_UCharacterAttributeSet_OnRep_CurrentHealth, "OnRep_CurrentHealth" }, // 1392583353
		{ &Z_Construct_UFunction_UCharacterAttributeSet_OnRep_MaxHealth, "OnRep_MaxHealth" }, // 1340648098
	};
	static_assert(UE_ARRAY_COUNT(FuncInfo) < 2048);
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<UCharacterAttributeSet>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FStructPropertyParams Z_Construct_UClass_UCharacterAttributeSet_Statics::NewProp_CurrentHealth = { "CurrentHealth", "OnRep_CurrentHealth", (EPropertyFlags)0x0010000100000034, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UCharacterAttributeSet, CurrentHealth), Z_Construct_UScriptStruct_FGameplayAttributeData, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_CurrentHealth_MetaData), NewProp_CurrentHealth_MetaData) }; // 1532612004
const UECodeGen_Private::FStructPropertyParams Z_Construct_UClass_UCharacterAttributeSet_Statics::NewProp_MaxHealth = { "MaxHealth", "OnRep_MaxHealth", (EPropertyFlags)0x0010000100000034, UECodeGen_Private::EPropertyGenFlags::Struct, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(UCharacterAttributeSet, MaxHealth), Z_Construct_UScriptStruct_FGameplayAttributeData, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_MaxHealth_MetaData), NewProp_MaxHealth_MetaData) }; // 1532612004
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_UCharacterAttributeSet_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UCharacterAttributeSet_Statics::NewProp_CurrentHealth,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_UCharacterAttributeSet_Statics::NewProp_MaxHealth,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UCharacterAttributeSet_Statics::PropPointers) < 2048);
UObject* (*const Z_Construct_UClass_UCharacterAttributeSet_Statics::DependentSingletons[])() = {
	(UObject* (*)())Z_Construct_UClass_UAttributeSet,
	(UObject* (*)())Z_Construct_UPackage__Script_WorldBeyond,
};
static_assert(UE_ARRAY_COUNT(Z_Construct_UClass_UCharacterAttributeSet_Statics::DependentSingletons) < 16);
const UECodeGen_Private::FClassParams Z_Construct_UClass_UCharacterAttributeSet_Statics::ClassParams = {
	&UCharacterAttributeSet::StaticClass,
	nullptr,
	&StaticCppClassTypeInfo,
	DependentSingletons,
	FuncInfo,
	Z_Construct_UClass_UCharacterAttributeSet_Statics::PropPointers,
	nullptr,
	UE_ARRAY_COUNT(DependentSingletons),
	UE_ARRAY_COUNT(FuncInfo),
	UE_ARRAY_COUNT(Z_Construct_UClass_UCharacterAttributeSet_Statics::PropPointers),
	0,
	0x003000A0u,
	METADATA_PARAMS(UE_ARRAY_COUNT(Z_Construct_UClass_UCharacterAttributeSet_Statics::Class_MetaDataParams), Z_Construct_UClass_UCharacterAttributeSet_Statics::Class_MetaDataParams)
};
UClass* Z_Construct_UClass_UCharacterAttributeSet()
{
	if (!Z_Registration_Info_UClass_UCharacterAttributeSet.OuterSingleton)
	{
		UECodeGen_Private::ConstructUClass(Z_Registration_Info_UClass_UCharacterAttributeSet.OuterSingleton, Z_Construct_UClass_UCharacterAttributeSet_Statics::ClassParams);
	}
	return Z_Registration_Info_UClass_UCharacterAttributeSet.OuterSingleton;
}
#if VALIDATE_CLASS_REPS
void UCharacterAttributeSet::ValidateGeneratedRepEnums(const TArray<struct FRepRecord>& ClassReps) const
{
	static FName Name_CurrentHealth(TEXT("CurrentHealth"));
	static FName Name_MaxHealth(TEXT("MaxHealth"));
	const bool bIsValid = true
		&& Name_CurrentHealth == ClassReps[(int32)ENetFields_Private::CurrentHealth].Property->GetFName()
		&& Name_MaxHealth == ClassReps[(int32)ENetFields_Private::MaxHealth].Property->GetFName();
	checkf(bIsValid, TEXT("UHT Generated Rep Indices do not match runtime populated Rep Indices for properties in UCharacterAttributeSet"));
}
#endif
DEFINE_VTABLE_PTR_HELPER_CTOR(UCharacterAttributeSet);
UCharacterAttributeSet::~UCharacterAttributeSet() {}
// ********** End Class UCharacterAttributeSet *****************************************************

// ********** Begin Registration *******************************************************************
struct Z_CompiledInDeferFile_FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_CharacterAttributeSet_h__Script_WorldBeyond_Statics
{
	static constexpr FClassRegisterCompiledInInfo ClassInfo[] = {
		{ Z_Construct_UClass_UCharacterAttributeSet, UCharacterAttributeSet::StaticClass, TEXT("UCharacterAttributeSet"), &Z_Registration_Info_UClass_UCharacterAttributeSet, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(UCharacterAttributeSet), 361217759U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_CharacterAttributeSet_h__Script_WorldBeyond_3441837769(TEXT("/Script/WorldBeyond"),
	Z_CompiledInDeferFile_FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_CharacterAttributeSet_h__Script_WorldBeyond_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_CharacterAttributeSet_h__Script_WorldBeyond_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0);
// ********** End Registration *********************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
