// Copyright Epic Games, Inc. All Rights Reserved.
/*===========================================================================
	Generated code exported from UnrealHeaderTool.
	DO NOT modify this manually! Edit the corresponding .h files instead!
===========================================================================*/

#include "UObject/GeneratedCppIncludes.h"
#include "WorldBeyond/Characters/BeyondCharacterBase.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

void EmptyLinkFunctionForGeneratedCodeBeyondCharacterBase() {}

// ********** Begin Cross Module References ********************************************************
ENGINE_API UClass* Z_Construct_UClass_ACharacter();
GAMEPLAYABILITIES_API UClass* Z_Construct_UClass_UAbilitySystemComponent_NoRegister();
GAMEPLAYABILITIES_API UClass* Z_Construct_UClass_UAbilitySystemInterface_NoRegister();
GAMEPLAYABILITIES_API UEnum* Z_Construct_UEnum_GameplayAbilities_EGameplayEffectReplicationMode();
UPackage* Z_Construct_UPackage__Script_WorldBeyond();
WORLDBEYOND_API UClass* Z_Construct_UClass_ABeyondCharacterBase();
WORLDBEYOND_API UClass* Z_Construct_UClass_ABeyondCharacterBase_NoRegister();
// ********** End Cross Module References **********************************************************

// ********** Begin Class ABeyondCharacterBase *****************************************************
void ABeyondCharacterBase::StaticRegisterNativesABeyondCharacterBase()
{
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
	static constexpr UECodeGen_Private::FMetaDataPairParam NewProp_ASCReplicationMode_MetaData[] = {
		{ "Category", "AbilitySystem" },
		{ "ModuleRelativePath", "Characters/BeyondCharacterBase.h" },
	};
#endif // WITH_METADATA
	static const UECodeGen_Private::FObjectPropertyParams NewProp_AbilitySystemComponent;
	static const UECodeGen_Private::FBytePropertyParams NewProp_ASCReplicationMode_Underlying;
	static const UECodeGen_Private::FEnumPropertyParams NewProp_ASCReplicationMode;
	static const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];
	static UObject* (*const DependentSingletons[])();
	static const UECodeGen_Private::FImplementedInterfaceParams InterfaceParams[];
	static constexpr FCppClassTypeInfoStatic StaticCppClassTypeInfo = {
		TCppClassTypeTraits<ABeyondCharacterBase>::IsAbstract,
	};
	static const UECodeGen_Private::FClassParams ClassParams;
};
const UECodeGen_Private::FObjectPropertyParams Z_Construct_UClass_ABeyondCharacterBase_Statics::NewProp_AbilitySystemComponent = { "AbilitySystemComponent", nullptr, (EPropertyFlags)0x00100000000a001d, UECodeGen_Private::EPropertyGenFlags::Object, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ABeyondCharacterBase, AbilitySystemComponent), Z_Construct_UClass_UAbilitySystemComponent_NoRegister, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_AbilitySystemComponent_MetaData), NewProp_AbilitySystemComponent_MetaData) };
const UECodeGen_Private::FBytePropertyParams Z_Construct_UClass_ABeyondCharacterBase_Statics::NewProp_ASCReplicationMode_Underlying = { "UnderlyingType", nullptr, (EPropertyFlags)0x0000000000000000, UECodeGen_Private::EPropertyGenFlags::Byte, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, 0, nullptr, METADATA_PARAMS(0, nullptr) };
const UECodeGen_Private::FEnumPropertyParams Z_Construct_UClass_ABeyondCharacterBase_Statics::NewProp_ASCReplicationMode = { "ASCReplicationMode", nullptr, (EPropertyFlags)0x0020080000000005, UECodeGen_Private::EPropertyGenFlags::Enum, RF_Public|RF_Transient|RF_MarkAsNative, nullptr, nullptr, 1, STRUCT_OFFSET(ABeyondCharacterBase, ASCReplicationMode), Z_Construct_UEnum_GameplayAbilities_EGameplayEffectReplicationMode, METADATA_PARAMS(UE_ARRAY_COUNT(NewProp_ASCReplicationMode_MetaData), NewProp_ASCReplicationMode_MetaData) }; // 3979288675
const UECodeGen_Private::FPropertyParamsBase* const Z_Construct_UClass_ABeyondCharacterBase_Statics::PropPointers[] = {
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ABeyondCharacterBase_Statics::NewProp_AbilitySystemComponent,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ABeyondCharacterBase_Statics::NewProp_ASCReplicationMode_Underlying,
	(const UECodeGen_Private::FPropertyParamsBase*)&Z_Construct_UClass_ABeyondCharacterBase_Statics::NewProp_ASCReplicationMode,
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
	nullptr,
	Z_Construct_UClass_ABeyondCharacterBase_Statics::PropPointers,
	InterfaceParams,
	UE_ARRAY_COUNT(DependentSingletons),
	0,
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
		{ Z_Construct_UClass_ABeyondCharacterBase, ABeyondCharacterBase::StaticClass, TEXT("ABeyondCharacterBase"), &Z_Registration_Info_UClass_ABeyondCharacterBase, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(ABeyondCharacterBase), 1245304422U) },
	};
};
static FRegisterCompiledInInfo Z_CompiledInDeferFile_FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_Characters_BeyondCharacterBase_h__Script_WorldBeyond_2920500535(TEXT("/Script/WorldBeyond"),
	Z_CompiledInDeferFile_FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_Characters_BeyondCharacterBase_h__Script_WorldBeyond_Statics::ClassInfo, UE_ARRAY_COUNT(Z_CompiledInDeferFile_FID_GAT360_Worlds_Beyond_WorldBeyond_Source_WorldBeyond_Characters_BeyondCharacterBase_h__Script_WorldBeyond_Statics::ClassInfo),
	nullptr, 0,
	nullptr, 0);
// ********** End Registration *********************************************************************

PRAGMA_ENABLE_DEPRECATION_WARNINGS
