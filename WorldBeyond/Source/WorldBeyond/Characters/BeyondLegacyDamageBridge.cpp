// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/BeyondLegacyDamageBridge.h"
#include "AbilitySystemComponent.h"
#include "BeyondGameplayTags.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

namespace BeyondLegacyDamage
{
	namespace
	{
		const TCHAR* ComponentClassPrefix = TEXT("BPC_DamageSystem");

		void SetNumber(UObject* Object, FName Name, double Value)
		{
			if (FNumericProperty* Prop = FindFProperty<FNumericProperty>(Object->GetClass(), Name))
			{
				void* Ptr = Prop->ContainerPtrToValuePtr<void>(Object);
				if (Prop->IsFloatingPoint())
				{
					Prop->SetFloatingPointPropertyValue(Ptr, Value);
				}
				else
				{
					Prop->SetIntPropertyValue(Ptr, static_cast<int64>(Value));
				}
			}
		}

		double GetNumber(const UObject* Object, FName Name)
		{
			if (const FNumericProperty* Prop = FindFProperty<FNumericProperty>(Object->GetClass(), Name))
			{
				const void* Ptr = Prop->ContainerPtrToValuePtr<void>(Object);
				return Prop->IsFloatingPoint() ? Prop->GetFloatingPointPropertyValue(Ptr) : static_cast<double>(Prop->GetSignedIntPropertyValue(Ptr));
			}
			return 0.0;
		}

		void SetBool(UObject* Object, FName Name, bool bValue)
		{
			if (FBoolProperty* Prop = FindFProperty<FBoolProperty>(Object->GetClass(), Name))
			{
				Prop->SetPropertyValue_InContainer(Object, bValue);
			}
		}

		bool GetBool(const UObject* Object, FName Name, bool bDefault)
		{
			const FBoolProperty* Prop = FindFProperty<FBoolProperty>(Object->GetClass(), Name);
			return Prop ? Prop->GetPropertyValue_InContainer(Object) : bDefault;
		}

		// Calls a Blueprint event dispatcher; Fill writes the parameters by name
		void Broadcast(UObject* Object, FName DelegateName, TFunctionRef<void(UFunction*, uint8*)> Fill)
		{
			const FMulticastDelegateProperty* Prop = FindFProperty<FMulticastDelegateProperty>(Object->GetClass(), DelegateName);
			if (!Prop || !Prop->SignatureFunction)
			{
				return;
			}

			const FMulticastScriptDelegate* Delegate = Prop->GetMulticastDelegate(Prop->ContainerPtrToValuePtr<void>(Object));
			if (!Delegate || !Delegate->IsBound())
			{
				return;
			}

			UFunction* Signature = Prop->SignatureFunction;
			uint8* Params = static_cast<uint8*>(FMemory_Alloca_Aligned(FMath::Max<int32>(Signature->ParmsSize, 1), Signature->GetMinAlignment()));
			FMemory::Memzero(Params, Signature->ParmsSize);
			for (TFieldIterator<FProperty> It(Signature); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
			{
				It->InitializeValue_InContainer(Params);
			}

			Fill(Signature, Params);
			Delegate->ProcessDelegate<UObject>(Params);

			for (TFieldIterator<FProperty> It(Signature); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
			{
				It->DestroyValue_InContainer(Params);
			}
		}

		void SetParamObject(UFunction* Signature, uint8* Params, FName Name, UObject* Value)
		{
			if (FObjectPropertyBase* Prop = FindFProperty<FObjectPropertyBase>(Signature, Name))
			{
				Prop->SetObjectPropertyValue_InContainer(Params, Value);
			}
		}

		void SetParamInt(UFunction* Signature, uint8* Params, FName Name, int64 Value)
		{
			FProperty* Prop = FindFProperty<FProperty>(Signature, Name);
			if (FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
			{
				EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(EnumProp->ContainerPtrToValuePtr<void>(Params), Value);
			}
			else if (FNumericProperty* NumProp = CastField<FNumericProperty>(Prop))
			{
				NumProp->SetIntPropertyValue(NumProp->ContainerPtrToValuePtr<void>(Params), Value);
			}
		}

		void SetParamBool(UFunction* Signature, uint8* Params, FName Name, bool bValue)
		{
			if (FBoolProperty* Prop = FindFProperty<FBoolProperty>(Signature, Name))
			{
				Prop->SetPropertyValue_InContainer(Params, bValue);
			}
		}
	}

	UActorComponent* FindComponent(const AActor* Owner)
	{
		if (!Owner)
		{
			return nullptr;
		}

		for (UActorComponent* Component : Owner->GetComponents())
		{
			if (Component && Component->GetClass()->GetName().StartsWith(ComponentClassPrefix))
			{
				return Component;
			}
		}
		return nullptr;
	}

	float GetLegacyMaxHealth(const UActorComponent* Component)
	{
		return Component ? static_cast<float>(GetNumber(Component, TEXT("MaxHealth"))) : 0.0f;
	}

	void SyncHealth(UActorComponent* Component, float Health, float MaxHealth, bool bDead)
	{
		if (!Component)
		{
			return;
		}
		SetNumber(Component, TEXT("MaxHealth"), MaxHealth);
		SetNumber(Component, TEXT("Health"), Health);
		SetBool(Component, TEXT("IsDead"), bDead);
	}

	void SyncStateTags(const UActorComponent* Component, UAbilitySystemComponent* ASC, FStateMirror& Mirror)
	{
		if (!Component || !ASC)
		{
			return;
		}

		// Adjust by delta so tags added elsewhere (abilities, SetStateTag) are left alone
		auto Apply = [ASC](bool bLegacyActive, const FGameplayTag& Tag, int32& MirroredCount)
		{
			const int32 Wanted = bLegacyActive ? 1 : 0;
			if (Wanted > MirroredCount)
			{
				ASC->AddLooseGameplayTag(Tag, Wanted - MirroredCount);
			}
			else if (Wanted < MirroredCount)
			{
				ASC->RemoveLooseGameplayTag(Tag, MirroredCount - Wanted);
			}
			MirroredCount = Wanted;
		};

		Apply(GetBool(Component, TEXT("IsBlocking"), false), BeyondTags::State_Blocking, Mirror.Blocking);
		Apply(GetBool(Component, TEXT("IsInvincible"), false), BeyondTags::State_Invincible, Mirror.Invincible);
		// The legacy flag is "IsInterruptible"; GAS uses the opposite tag. Default true = interruptible.
		Apply(!GetBool(Component, TEXT("IsInterruptible"), true), BeyondTags::State_Uninterruptible, Mirror.Uninterruptible);
	}

	void BroadcastDamageResponse(UActorComponent* Component, uint8 LegacyDamageResponse, AActor* DamageCauser)
	{
		if (!Component)
		{
			return;
		}
		Broadcast(Component, TEXT("OnDamageResponse"), [&](UFunction* Signature, uint8* Params)
		{
			SetParamInt(Signature, Params, TEXT("DamageResponse"), LegacyDamageResponse);
			SetParamObject(Signature, Params, TEXT("DamageCauser"), DamageCauser);
		});
	}

	void BroadcastBlocked(UActorComponent* Component, bool bCanBeParried, AActor* DamageCauser)
	{
		if (!Component)
		{
			return;
		}
		Broadcast(Component, TEXT("OnBlocked"), [&](UFunction* Signature, uint8* Params)
		{
			SetParamBool(Signature, Params, TEXT("CanBeParried"), bCanBeParried);
			SetParamObject(Signature, Params, TEXT("DamageCauser"), DamageCauser);
		});
	}

	void BroadcastDeath(UActorComponent* Component)
	{
		if (!Component)
		{
			return;
		}
		Broadcast(Component, TEXT("OnDeath"), [](UFunction*, uint8*) {});
	}

	uint8 HitResponseToLegacy(const FGameplayTag& HitResponse)
	{
		if (HitResponse == BeyondTags::Event_Hit_Light) { return 1; }
		if (HitResponse == BeyondTags::Event_Hit_Stagger) { return 2; }
		if (HitResponse == BeyondTags::Event_Hit_Stun) { return 3; }
		if (HitResponse == BeyondTags::Event_Hit_KnockBack) { return 4; }
		return 0;
	}
}
