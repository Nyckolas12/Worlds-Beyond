// Fill out your copyright notice in the Description page of Project Settings.

#include "Dialogue/BeyondDialogueBridge.h"
#include "WorldBeyond.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/CapsuleComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/PanelWidget.h"
#include "Components/WidgetComponent.h"
#include "Dialogue/BeyondDialogueSettings.h"
#include "Engine/DataTable.h"
#include "GameFramework/Character.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace BeyondDialogue
{
	namespace
	{
		// Pack names C++ relies on (CheckPackBindings verifies them)
		const FName FnStartDialogue(TEXT("StartDialogue"));
		const FName FnCloseWidget(TEXT("CloseWidget"));
		const FName FnStartOverHead(TEXT("StartDialogueReplicated"));
		const FName FnStopOverHead(TEXT("ForceStopDialogueServer"));
		const FName FnSkip(TEXT("Skip"));
		const FName VarIsInDialogue(TEXT("IsInDialogue"));
		const FName VarMainDialogue(TEXT("MainDialogue"));
		const FName VarDialogueWidget(TEXT("WBP_Dialogue"));
		const FName VarCurrentRow(TEXT("CurrentRowName"));
		const FName VarWidgetComponentRef(TEXT("BPACDialogue"));
		const FName VarOptionBox(TEXT("OptionBox"));
		const FName VarOptionButton(TEXT("OptionButton"));
		const FName VarOptionDone(TEXT("IsTheOptionOver"));
		const FName VarRowChanged(TEXT("UpdatedCurrentRow"));
		const FName VarActorRowName(TEXT("Row Name"));
		const FName VarActorInDialogue(TEXT("Is in Dialogue"));

		const UBeyondDialogueSettings* Settings()
		{
			return GetDefault<UBeyondDialogueSettings>();
		}

		template <typename T>
		UClass* LoadSoftClass(const TSoftClassPtr<T>& SoftClass)
		{
			return SoftClass.IsNull() ? nullptr : SoftClass.LoadSynchronous();
		}

		UActorComponent* FindComponentOf(const AActor* Actor, UClass* Class, const TCHAR* FallbackClassName)
		{
			if (!Actor)
			{
				return nullptr;
			}
			for (UActorComponent* Component : Actor->GetComponents())
			{
				if (!Component)
				{
					continue;
				}
				if (Class ? Component->IsA(Class) : Component->GetClass()->GetName() == FallbackClassName)
				{
					return Component;
				}
			}
			return nullptr;
		}

		UObject* GetObjectVar(const UObject* Object, FName Name)
		{
			const FObjectPropertyBase* Prop = Object ? FindFProperty<FObjectPropertyBase>(Object->GetClass(), Name) : nullptr;
			return Prop ? Prop->GetObjectPropertyValue_InContainer(Object) : nullptr;
		}

		void SetObjectVar(UObject* Object, FName Name, UObject* Value)
		{
			if (FObjectPropertyBase* Prop = Object ? FindFProperty<FObjectPropertyBase>(Object->GetClass(), Name) : nullptr)
			{
				if (!Value || Value->IsA(Prop->PropertyClass))
				{
					Prop->SetObjectPropertyValue_InContainer(Object, Value);
				}
			}
		}

		bool GetBoolVar(const UObject* Object, FName Name, bool bDefault = false)
		{
			const FBoolProperty* Prop = Object ? FindFProperty<FBoolProperty>(Object->GetClass(), Name) : nullptr;
			return Prop ? Prop->GetPropertyValue_InContainer(Object) : bDefault;
		}

		FName GetNameVar(const UObject* Object, FName Name)
		{
			const FNameProperty* Prop = Object ? FindFProperty<FNameProperty>(Object->GetClass(), Name) : nullptr;
			return Prop ? Prop->GetPropertyValue_InContainer(Object) : NAME_None;
		}

		bool IsInput(const FProperty* Prop)
		{
			// Arrays and structs come in by const reference, which also marks them as out parameters
			return Prop->HasAnyPropertyFlags(CPF_Parm) && !Prop->HasAnyPropertyFlags(CPF_ReturnParm)
				&& (!Prop->HasAnyPropertyFlags(CPF_OutParm) || Prop->HasAnyPropertyFlags(CPF_ReferenceParm));
		}

		// The input parameter called Name, else the first input of the right type
		template <typename TProp>
		TProp* FindParam(UFunction* Function, const TCHAR* Name)
		{
			for (TFieldIterator<FProperty> It(Function); It; ++It)
			{
				if (IsInput(*It) && It->GetAuthoredName().Equals(Name, ESearchCase::IgnoreCase))
				{
					if (TProp* Typed = CastField<TProp>(*It))
					{
						return Typed;
					}
				}
			}
			for (TFieldIterator<FProperty> It(Function); It; ++It)
			{
				if (IsInput(*It))
				{
					if (TProp* Typed = CastField<TProp>(*It))
					{
						return Typed;
					}
				}
			}
			return nullptr;
		}

		// Calls a Blueprint function / custom event by name; Fill writes its parameters
		bool Call(UObject* Target, FName FunctionName, TFunctionRef<void(UFunction*, uint8*)> Fill)
		{
			UFunction* Function = Target ? Target->FindFunction(FunctionName) : nullptr;
			if (!Function)
			{
				UE_LOG(LogBeyond, Warning, TEXT("Dialogue: %s has no %s (did the dialogue pack change?)"), *GetNameSafe(Target), *FunctionName.ToString());
				return false;
			}
			FStructOnScope Params(Function);
			Fill(Function, Params.GetStructMemory());
			Target->ProcessEvent(Function, Params.GetStructMemory());
			return true;
		}

		const FProperty* FindField(const UStruct* Struct, const TCHAR* FieldName)
		{
			if (!Struct)
			{
				return nullptr;
			}
			// User-defined structs add a GUID to the real name; the authored name is what the table editor shows
			for (TFieldIterator<FProperty> It(Struct); It; ++It)
			{
				if (It->GetAuthoredName().Equals(FieldName, ESearchCase::IgnoreCase))
				{
					return *It;
				}
			}
			return nullptr;
		}

		FName ReadName(const UStruct* Struct, const void* Data, const TCHAR* FieldName)
		{
			const FNameProperty* Prop = CastField<FNameProperty>(FindField(Struct, FieldName));
			return Prop ? Prop->GetPropertyValue_InContainer(Data) : NAME_None;
		}

		FText ReadText(const UStruct* Struct, const void* Data, const TCHAR* FieldName)
		{
			const FTextProperty* Prop = CastField<FTextProperty>(FindField(Struct, FieldName));
			return Prop ? Prop->GetPropertyValue_InContainer(Data) : FText::GetEmpty();
		}

		double ReadNumber(const UStruct* Struct, const void* Data, const TCHAR* FieldName)
		{
			const FNumericProperty* Prop = CastField<FNumericProperty>(FindField(Struct, FieldName));
			if (!Prop)
			{
				return 0.0;
			}
			const void* Value = Prop->ContainerPtrToValuePtr<void>(Data);
			return Prop->IsFloatingPoint() ? Prop->GetFloatingPointPropertyValue(Value) : static_cast<double>(Prop->GetSignedIntPropertyValue(Value));
		}

		// An enum field's display name ("free movement dialogue", "Option")
		FString ReadEnumName(const UStruct* Struct, const void* Data, const TCHAR* FieldName)
		{
			const FProperty* Prop = FindField(Struct, FieldName);
			if (const FByteProperty* ByteProp = CastField<FByteProperty>(Prop))
			{
				const uint8 Value = ByteProp->GetPropertyValue_InContainer(Data);
				return ByteProp->Enum ? ByteProp->Enum->GetDisplayNameTextByValue(Value).ToString() : FString::FromInt(Value);
			}
			if (const FEnumProperty* EnumProp = CastField<FEnumProperty>(Prop))
			{
				const int64 Value = EnumProp->GetUnderlyingProperty()->GetSignedIntPropertyValue(EnumProp->ContainerPtrToValuePtr<void>(Data));
				return EnumProp->GetEnum() ? EnumProp->GetEnum()->GetDisplayNameTextByValue(Value).ToString() : FString::Printf(TEXT("%lld"), Value);
			}
			return FString();
		}

		UPanelWidget* GetOptionBox(const UActorComponent* DialogueComponent)
		{
			return Cast<UPanelWidget>(GetObjectVar(GetDialogueWidget(DialogueComponent), VarOptionBox));
		}

		// The dialogue widget calls back into "its" BP_AC_Dialogue, which it looks up when it is shown. With two
		// demigods each owning one, point it at the component actually running the conversation.
		void PointWidgetAt(UActorComponent* DialogueComponent)
		{
			SetObjectVar(GetDialogueWidget(DialogueComponent), VarWidgetComponentRef, DialogueComponent);
		}
	}

	UActorComponent* FindDialogueComponent(const AActor* Actor)
	{
		return FindComponentOf(Actor, LoadSoftClass(Settings()->DialogueComponentClass), TEXT("BP_AC_Dialogue_C"));
	}

	UActorComponent* FindOverHeadComponent(const AActor* Actor)
	{
		return FindComponentOf(Actor, LoadSoftClass(Settings()->OverHeadComponentClass), TEXT("BP_AC_DialogueOverHead_C"));
	}

	void EnsureParticipant(ACharacter* Character, bool bConversations)
	{
		if (!Character || !Character->GetWorld() || !Character->GetWorld()->IsGameWorld())
		{
			return;
		}

		const UBeyondDialogueSettings* Config = Settings();
		UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		const FName Tag = Config->ComponentTag;

		const float HalfHeight = Capsule ? Capsule->GetUnscaledCapsuleHalfHeight() : 90.0f;
		const FVector TextLocation(0.0f, 0.0f, HalfHeight + Config->OverHeadHeight);
		// In front of the face, looking back at it
		const FRotator CameraRotation = (FVector(0.0f, 0.0f, HalfHeight * 0.72f) - Config->FocusCameraOffset).Rotation();

		// Ones the Blueprint already has (the pack's demo NPC leaves them at the capsule centre and the feet) go where
		// ours would
		bool bHasText = false;
		bool bHasCamera = false;
		for (UActorComponent* Component : Character->GetComponents())
		{
			if (!Component || !Component->ComponentHasTag(Tag))
			{
				continue;
			}
			if (UWidgetComponent* Text = Cast<UWidgetComponent>(Component))
			{
				bHasText = true;
				if (Capsule && Text->GetAttachParent() != Capsule)
				{
					Text->AttachToComponent(Capsule, FAttachmentTransformRules::KeepRelativeTransform);
				}
				Text->SetRelativeLocation(TextLocation);
			}
			else if (UChildActorComponent* Camera = Cast<UChildActorComponent>(Component))
			{
				bHasCamera = true;
				if (Capsule && Camera->GetAttachParent() != Capsule)
				{
					Camera->AttachToComponent(Capsule, FAttachmentTransformRules::KeepRelativeTransform);
				}
				Camera->SetRelativeLocationAndRotation(Config->FocusCameraOffset, CameraRotation);
			}
		}

		TArray<FString> Added;

		// The pack's components look these two up by tag in their BeginPlay, so they come first
		if (!bHasText)
		{
			if (UClass* WidgetClass = LoadSoftClass(Config->OverHeadWidgetClass))
			{
				UWidgetComponent* Text = NewObject<UWidgetComponent>(Character, MakeUniqueObjectName(Character, UWidgetComponent::StaticClass(), TEXT("DialogueText")));
				Text->ComponentTags.Add(Tag);
				Text->SetWidgetSpace(EWidgetSpace::Screen);
				Text->SetWidgetClass(WidgetClass);
				Text->SetDrawAtDesiredSize(true);
				Text->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Text->SetGenerateOverlapEvents(false);
				Text->SetupAttachment(Capsule);
				Text->SetRelativeLocation(TextLocation);
				Character->AddInstanceComponent(Text);
				Text->RegisterComponent();
				Added.Add(TEXT("over-head text"));
			}
		}
		if (!bHasCamera)
		{
			if (UClass* CameraClass = LoadSoftClass(Config->FocusCameraClass))
			{
				UChildActorComponent* Camera = NewObject<UChildActorComponent>(Character, MakeUniqueObjectName(Character, UChildActorComponent::StaticClass(), TEXT("DialogueCamera")));
				Camera->ComponentTags.Add(Tag);
				Camera->SetChildActorClass(CameraClass);
				Camera->SetupAttachment(Capsule);
				Camera->SetRelativeLocationAndRotation(Config->FocusCameraOffset, CameraRotation);
				Character->AddInstanceComponent(Camera);
				Camera->RegisterComponent();
				Added.Add(TEXT("focus camera"));
			}
		}
		if (!FindOverHeadComponent(Character))
		{
			if (UClass* OverHeadClass = LoadSoftClass(Config->OverHeadComponentClass))
			{
				UActorComponent* OverHead = NewObject<UActorComponent>(Character, OverHeadClass, MakeUniqueObjectName(Character, OverHeadClass, TEXT("DialogueOverHead")));
				Character->AddInstanceComponent(OverHead);
				OverHead->RegisterComponent();
				Added.Add(TEXT("over-head dialogue"));
			}
		}
		if (bConversations && !FindDialogueComponent(Character))
		{
			if (UClass* DialogueClass = LoadSoftClass(Config->DialogueComponentClass))
			{
				UActorComponent* Dialogue = NewObject<UActorComponent>(Character, DialogueClass, MakeUniqueObjectName(Character, DialogueClass, TEXT("Dialogue")));
				Character->AddInstanceComponent(Dialogue);
				Dialogue->RegisterComponent();
				Added.Add(TEXT("conversations"));
			}
		}

		if (!Added.IsEmpty())
		{
			UE_LOG(LogBeyond, Log, TEXT("Dialogue: %s got %s"), *Character->GetName(), *FString::Join(Added, TEXT(", ")));
		}
	}

	bool StartConversation(UActorComponent* DialogueComponent, FName Row, AActor* Partner)
	{
		if (!DialogueComponent || Row.IsNone())
		{
			return false;
		}

		PointWidgetAt(DialogueComponent);
		const bool bCalled = Call(DialogueComponent, FnStartDialogue, [Row, Partner](UFunction* Function, uint8* Params)
		{
			if (FNameProperty* RowParam = FindParam<FNameProperty>(Function, TEXT("RowName")))
			{
				RowParam->SetPropertyValue_InContainer(Params, Row);
			}
			if (FObjectPropertyBase* ActorParam = FindParam<FObjectPropertyBase>(Function, TEXT("Actor")))
			{
				ActorParam->SetObjectPropertyValue_InContainer(Params, Partner);
			}
		});
		// Showing the widget runs its Construct, which looks its component up again
		PointWidgetAt(DialogueComponent);
		return bCalled && IsInDialogue(DialogueComponent);
	}

	bool CloseConversation(UActorComponent* DialogueComponent)
	{
		if (!DialogueComponent || !IsInDialogue(DialogueComponent))
		{
			return false;
		}
		return Call(DialogueComponent, FnCloseWidget, [](UFunction*, uint8*) {});
	}

	bool IsInDialogue(const UActorComponent* Component)
	{
		return GetBoolVar(Component, VarIsInDialogue);
	}

	UUserWidget* GetMainWidget(const UActorComponent* DialogueComponent)
	{
		return Cast<UUserWidget>(GetObjectVar(DialogueComponent, VarMainDialogue));
	}

	UUserWidget* GetDialogueWidget(const UActorComponent* DialogueComponent)
	{
		return Cast<UUserWidget>(GetObjectVar(GetMainWidget(DialogueComponent), VarDialogueWidget));
	}

	FName GetCurrentRow(const UActorComponent* DialogueComponent)
	{
		return IsInDialogue(DialogueComponent) ? GetNameVar(GetMainWidget(DialogueComponent), VarCurrentRow) : NAME_None;
	}

	bool Skip(UActorComponent* DialogueComponent)
	{
		UUserWidget* Main = GetMainWidget(DialogueComponent);
		return Main && IsInDialogue(DialogueComponent) && Call(Main, FnSkip, [](UFunction*, uint8*) {});
	}

	int32 GetOptionCount(const UActorComponent* DialogueComponent, bool* bOutReady)
	{
		const UPanelWidget* Box = GetOptionBox(DialogueComponent);
		int32 Count = 0;
		bool bReady = true;
		if (Box && Box->IsVisible())
		{
			for (int32 Index = 0; Index < Box->GetChildrenCount(); ++Index)
			{
				if (const UUserWidget* Option = Cast<UUserWidget>(Box->GetChildAt(Index)))
				{
					++Count;
					bReady &= GetBoolVar(Option, VarOptionDone, true);
				}
			}
		}
		if (bOutReady)
		{
			*bOutReady = Count > 0 && bReady;
		}
		return Count;
	}

	bool ChooseOption(UActorComponent* DialogueComponent, int32 Index)
	{
		bool bReady = false;
		const int32 Count = GetOptionCount(DialogueComponent, &bReady);
		if (!bReady || !FMath::IsWithinInclusive(Index, 0, Count - 1))
		{
			return false;
		}

		UPanelWidget* Box = GetOptionBox(DialogueComponent);
		int32 Seen = 0;
		for (int32 Child = 0; Child < Box->GetChildrenCount(); ++Child)
		{
			UUserWidget* Option = Cast<UUserWidget>(Box->GetChildAt(Child));
			if (!Option)
			{
				continue;
			}
			if (Seen++ == Index)
			{
				UButton* Button = Cast<UButton>(GetObjectVar(Option, VarOptionButton));
				if (!Button)
				{
					UE_LOG(LogBeyond, Warning, TEXT("Dialogue: %s has no %s"), *Option->GetClass()->GetName(), *VarOptionButton.ToString());
					return false;
				}
				Button->OnClicked.Broadcast();
				return true;
			}
		}
		return false;
	}

	bool BindRowChanged(UActorComponent* DialogueComponent, UObject* Listener, FName FunctionName)
	{
		UUserWidget* Widget = GetDialogueWidget(DialogueComponent);
		FMulticastDelegateProperty* Prop = Widget ? FindFProperty<FMulticastDelegateProperty>(Widget->GetClass(), VarRowChanged) : nullptr;
		if (!Prop || !Listener || !Listener->FindFunction(FunctionName))
		{
			return false;
		}
		FScriptDelegate Delegate;
		Delegate.BindUFunction(Listener, FunctionName);
		Prop->AddDelegate(MoveTemp(Delegate), Widget);
		return true;
	}

	bool StartOverHead(UActorComponent* OverHeadComponent, FName Row, const TArray<AActor*>& Others)
	{
		if (!OverHeadComponent || Row.IsNone())
		{
			return false;
		}
		return Call(OverHeadComponent, FnStartOverHead, [Row, &Others](UFunction* Function, uint8* Params)
		{
			if (FNameProperty* RowParam = FindParam<FNameProperty>(Function, TEXT("RowName")))
			{
				RowParam->SetPropertyValue_InContainer(Params, Row);
			}
			if (FArrayProperty* ActorsParam = FindParam<FArrayProperty>(Function, TEXT("DialogueActors")))
			{
				if (const FObjectPropertyBase* Inner = CastField<FObjectPropertyBase>(ActorsParam->Inner))
				{
					FScriptArrayHelper Array(ActorsParam, ActorsParam->ContainerPtrToValuePtr<void>(Params));
					for (AActor* Other : Others)
					{
						const int32 Added = Array.AddValue();
						Inner->SetObjectPropertyValue(Array.GetRawPtr(Added), Other);
					}
				}
			}
		});
	}

	void StopOverHead(UActorComponent* OverHeadComponent)
	{
		if (OverHeadComponent && IsInDialogue(OverHeadComponent))
		{
			Call(OverHeadComponent, FnStopOverHead, [](UFunction*, uint8*) {});
		}
	}

	void SetActorRowName(AActor* Actor, FName Row)
	{
		if (FNameProperty* Prop = Actor ? FindFProperty<FNameProperty>(Actor->GetClass(), VarActorRowName) : nullptr)
		{
			Prop->SetPropertyValue_InContainer(Actor, Row);
		}
	}

	bool IsActorInDialogue(const AActor* Actor)
	{
		return GetBoolVar(Actor, VarActorInDialogue);
	}

	UDataTable* GetDialogueTable()
	{
		return Settings()->DialogueTable.LoadSynchronous();
	}

	UDataTable* GetOverHeadTable()
	{
		return Settings()->OverHeadTable.LoadSynchronous();
	}

	UDataTable* GetSpeakersTable()
	{
		return Settings()->SpeakersTable.LoadSynchronous();
	}

	bool ReadDialogueRow(FName Row, FBeyondDialogueRowView& Out)
	{
		const UDataTable* Table = GetDialogueTable();
		const uint8* Data = Table && !Row.IsNone() ? Table->FindRowUnchecked(Row) : nullptr;
		if (!Data)
		{
			return false;
		}

		const UScriptStruct* Struct = Table->GetRowStruct();
		Out = FBeyondDialogueRowView();
		Out.Row = Row;
		Out.NextRow = ReadName(Struct, Data, TEXT("NextRow"));
		Out.SpeakerRow = ReadName(Struct, Data, TEXT("Speaker_Row"));
		Out.SpecialEvent = ReadName(Struct, Data, TEXT("SpecialEvent"));
		Out.Text = ReadText(Struct, Data, TEXT("Text"));
		Out.bFreeMovement = ReadEnumName(Struct, Data, TEXT("DialogueType")).Contains(TEXT("free"));
		Out.bOptions = ReadEnumName(Struct, Data, TEXT("TextOrOption")).Equals(TEXT("Option"), ESearchCase::IgnoreCase);
		Out.SkipDuration = static_cast<float>(ReadNumber(Struct, Data, TEXT("SkipDialogueWithDuration")));

		if (const FArrayProperty* OptionsProp = CastField<FArrayProperty>(FindField(Struct, TEXT("Option"))))
		{
			if (const FStructProperty* OptionStruct = CastField<FStructProperty>(OptionsProp->Inner))
			{
				FScriptArrayHelper Array(OptionsProp, OptionsProp->ContainerPtrToValuePtr<void>(Data));
				for (int32 Index = 0; Index < Array.Num(); ++Index)
				{
					const uint8* Option = Array.GetRawPtr(Index);
					Out.Options.Emplace(ReadText(OptionStruct->Struct, Option, TEXT("Option")), ReadName(OptionStruct->Struct, Option, TEXT("NextRow")));
				}
			}
		}
		return true;
	}

	FString GetRowFieldText(const UDataTable* Table, FName Row, const TCHAR* FieldName)
	{
		const uint8* Data = Table && !Row.IsNone() ? Table->FindRowUnchecked(Row) : nullptr;
		const FProperty* Prop = Data ? FindField(Table->GetRowStruct(), FieldName) : nullptr;
		if (!Prop)
		{
			return FString();
		}
		if (const FTextProperty* TextProp = CastField<FTextProperty>(Prop))
		{
			return TextProp->GetPropertyValue_InContainer(Data).ToString();
		}
		if (CastField<FByteProperty>(Prop) || CastField<FEnumProperty>(Prop))
		{
			return ReadEnumName(Table->GetRowStruct(), Data, FieldName);
		}
		FString Value;
		Prop->ExportTextItem_Direct(Value, Prop->ContainerPtrToValuePtr<void>(Data), nullptr, nullptr, PPF_None);
		return Value;
	}

	TArray<FString> CheckPackBindings()
	{
		TArray<FString> Missing;
		const UBeyondDialogueSettings* Config = Settings();

		auto RequireFunction = [&Missing](const UClass* Class, FName Name, std::initializer_list<const TCHAR*> Params)
		{
			UFunction* Function = Class ? Class->FindFunctionByName(Name) : nullptr;
			if (!Function)
			{
				Missing.Add(FString::Printf(TEXT("%s.%s()"), *GetNameSafe(Class), *Name.ToString()));
				return;
			}
			for (const TCHAR* Param : Params)
			{
				bool bFound = false;
				for (TFieldIterator<FProperty> It(Function); It; ++It)
				{
					bFound |= IsInput(*It) && It->GetAuthoredName().Equals(Param, ESearchCase::IgnoreCase);
				}
				if (!bFound)
				{
					Missing.Add(FString::Printf(TEXT("%s.%s(%s)"), *GetNameSafe(Class), *Name.ToString(), Param));
				}
			}
		};
		auto RequireVar = [&Missing](const UClass* Class, FName Name)
		{
			if (!Class || !FindFProperty<FProperty>(Class, Name))
			{
				Missing.Add(FString::Printf(TEXT("%s.%s"), *GetNameSafe(Class), *Name.ToString()));
			}
		};

		const UClass* Dialogue = LoadSoftClass(Config->DialogueComponentClass);
		const UClass* OverHead = LoadSoftClass(Config->OverHeadComponentClass);
		if (!Dialogue)
		{
			Missing.Add(TEXT("Dialogue Component Class (BP_AC_Dialogue)"));
		}
		if (!OverHead)
		{
			Missing.Add(TEXT("Over Head Component Class (BP_AC_DialogueOverHead)"));
		}
		if (!LoadSoftClass(Config->OverHeadWidgetClass))
		{
			Missing.Add(TEXT("Over Head Widget Class (WBP_TextOverHead)"));
		}
		if (!LoadSoftClass(Config->FocusCameraClass))
		{
			Missing.Add(TEXT("Focus Camera Class (BP_CameraActor)"));
		}
		if (!GetDialogueTable() || !GetOverHeadTable() || !GetSpeakersTable())
		{
			Missing.Add(TEXT("a dialogue table (DT_Dialogue / DT_TextOverHead / DT_Speakers)"));
		}

		RequireFunction(Dialogue, FnStartDialogue, { TEXT("RowName"), TEXT("Actor") });
		RequireFunction(Dialogue, FnCloseWidget, {});
		RequireVar(Dialogue, VarIsInDialogue);
		RequireVar(Dialogue, VarMainDialogue);
		RequireFunction(OverHead, FnStartOverHead, { TEXT("RowName"), TEXT("DialogueActors") });
		RequireFunction(OverHead, FnStopOverHead, {});
		RequireVar(OverHead, VarIsInDialogue);

		// The widgets, through the component's variable types
		const FObjectPropertyBase* MainProp = Dialogue ? FindFProperty<FObjectPropertyBase>(Dialogue, VarMainDialogue) : nullptr;
		const UClass* Main = MainProp ? MainProp->PropertyClass.Get() : nullptr;
		RequireFunction(Main, FnSkip, {});
		RequireVar(Main, VarCurrentRow);
		RequireVar(Main, VarDialogueWidget);
		const FObjectPropertyBase* WidgetProp = Main ? FindFProperty<FObjectPropertyBase>(Main, VarDialogueWidget) : nullptr;
		const UClass* Widget = WidgetProp ? WidgetProp->PropertyClass.Get() : nullptr;
		RequireVar(Widget, VarRowChanged);
		RequireVar(Widget, VarWidgetComponentRef);
		RequireVar(Widget, VarOptionBox);

		// Row fields C++ reads
		if (const UDataTable* Table = GetDialogueTable())
		{
			for (const TCHAR* Field : { TEXT("NextRow"), TEXT("Speaker_Row"), TEXT("SpecialEvent"), TEXT("Text"), TEXT("DialogueType"),
				TEXT("TextOrOption"), TEXT("Option"), TEXT("SkipDialogueWithDuration") })
			{
				if (!FindField(Table->GetRowStruct(), Field))
				{
					Missing.Add(FString::Printf(TEXT("DT_Dialogue field %s"), Field));
				}
			}
		}
		return Missing;
	}
}
