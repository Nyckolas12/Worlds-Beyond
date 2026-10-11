// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BeyondWorldSubsystem.generated.h"

class ABeyondBossArena;
class ABeyondCharacterBase;
class ABeyondPlayerController;
class ABeyondPointOfInterest;
class ABeyondRegionVolume;
class ABeyondWaystone;
class ABeyondWorldInfo;
class UBeyondPartyComponent;
class UBeyondRegionDefinition;

UENUM(BlueprintType)
enum class EBeyondTravelReason : uint8
{
	// From the world map to a waystone
	FastTravel,
	// Back at the respawn point after a party wipe
	Respawn,
	// Loading a save in the open world: at the last waystone used
	Resume,
	// The first moments in an open-world map: wait for the ground under the player start
	Start,
	// Deep water sent the party back
	SafeGround,
	// Console / tests
	Cheat
};

UENUM(BlueprintType)
enum class EBeyondMapMarkerKind : uint8
{
	Region,
	Village,
	Area,
	Waystone,
	Place,
	Arena,
	Leader,
	Companion
};

/** One thing the world map draws */
USTRUCT(BlueprintType)
struct WORLDBEYOND_API FBeyondMapMarker
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Map")
	EBeyondMapMarkerKind Kind = EBeyondMapMarkerKind::Place;

	UPROPERTY(BlueprintReadOnly, Category = "Map")
	FName Id;

	UPROPERTY(BlueprintReadOnly, Category = "Map")
	FText Label;

	// Level band, kind of place, the boss's title
	UPROPERTY(BlueprintReadOnly, Category = "Map")
	FText Detail;

	UPROPERTY(BlueprintReadOnly, Category = "Map")
	FVector Location = FVector::ZeroVector;

	// Facing (the party arrows)
	UPROPERTY(BlueprintReadOnly, Category = "Map")
	float Yaw = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Map")
	bool bDiscovered = false;

	// Waystones: attuned (can travel there); arenas: the boss is beaten
	UPROPERTY(BlueprintReadOnly, Category = "Map")
	bool bActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Map")
	FLinearColor Colour = FLinearColor::White;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBeyondRegionChangedSignature, UBeyondRegionDefinition*, NewRegion, UBeyondRegionDefinition*, OldRegion);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FBeyondDiscoveredSignature, FName, Id, FText, DisplayName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBeyondWaystoneSignature, ABeyondWaystone*, Waystone);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBeyondTravelSignature, EBeyondTravelReason, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FBeyondPartyRestedSignature);

/**
 * The open world's hub (Plan 5). Knows every region volume, waystone, place and boss arena in the world and checks four
 * times a second where the party leader is:
 * - the region and the village (or area) underfoot: name banner, first-visit discovery (EXP, saved), region banter,
 *   weather, music and ambience (through the map's ABeyondWorldInfo); a sealed boss arena plays the boss's music;
 * - places and waystones within their discovery radius (toast, EXP, on the map);
 * - the last safe ground (deep water sends the party back there).
 * Waystones: F attunes / rests (heal, respawn point, save). Fast travel from the world map to attuned waystones outside
 * fights, and every big move of the party (fast travel, a respawn, resuming a save, the start of an open-world map) goes
 * through the streaming hold: fade out, move, wait until the ground and the navmesh are there, snap to the ground, fade in.
 * Open-world maps also get their navmesh built around the demigods (navigation invokers).
 * Idle in maps without any of these actors.
 * Console: Beyond.Travel, Beyond.Discover, Beyond.ResetWorld, Beyond.Region, Beyond.Map, Beyond.Weather, Beyond.WorldDemo.
 */
UCLASS()
class WORLDBEYOND_API UBeyondWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UBeyondWorldSubsystem* Get(const UObject* WorldContext);

	// The party's members (the first player controller's party), empty without one
	static TArray<ABeyondCharacterBase*> GetPartyMembers(const UObject* WorldContext);
	static UBeyondPartyComponent* GetParty(const UObject* WorldContext);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	//~ Registration (the actors do this themselves)
	void RegisterRegionVolume(ABeyondRegionVolume* Volume);
	void UnregisterRegionVolume(ABeyondRegionVolume* Volume);
	void RegisterWaystone(ABeyondWaystone* Waystone);
	void UnregisterWaystone(ABeyondWaystone* Waystone);
	void RegisterPointOfInterest(ABeyondPointOfInterest* Place);
	void UnregisterPointOfInterest(ABeyondPointOfInterest* Place);
	void RegisterArena(ABeyondBossArena* Arena);
	void UnregisterArena(ABeyondBossArena* Arena);
	void RegisterWorldInfo(ABeyondWorldInfo* Info);
	void UnregisterWorldInfo(ABeyondWorldInfo* Info);

	// Called by the party once it has formed and read its save
	void HandlePartyFormed(UBeyondPartyComponent* Party);

	//~ Where things are

	UFUNCTION(BlueprintPure, Category = "Beyond|World")
	ABeyondWorldInfo* GetWorldInfo() const { return WorldInfo.Get(); }

	// The map has an ABeyondWorldInfo
	UFUNCTION(BlueprintPure, Category = "Beyond|World")
	bool IsOpenWorld() const { return WorldInfo.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "Beyond|World")
	UBeyondRegionDefinition* GetCurrentRegion() const { return CurrentRegion.Get(); }

	// The village or area the leader is in (null outside them)
	UFUNCTION(BlueprintPure, Category = "Beyond|World")
	UBeyondRegionDefinition* GetCurrentVillage() const { return CurrentVillage.Get(); }

	// The region (bSubLocation false) or village / area (true) with the highest priority at Location
	UFUNCTION(BlueprintPure, Category = "Beyond|World")
	UBeyondRegionDefinition* FindRegionAt(const FVector& Location, bool bSubLocation = false) const;

	// The enemy level band at Location (the village's if it has its own, else the region's); false outside every region
	UFUNCTION(BlueprintPure, Category = "Beyond|World")
	bool GetLevelBandAt(const FVector& Location, int32& OutMin, int32& OutMax) const;

	UFUNCTION(BlueprintPure, Category = "Beyond|World")
	UBeyondRegionDefinition* FindRegionById(FName RegionId) const;

	UFUNCTION(BlueprintPure, Category = "Beyond|World")
	ABeyondWaystone* FindWaystone(FName WaystoneId) const;

	UFUNCTION(BlueprintPure, Category = "Beyond|World")
	TArray<ABeyondWaystone*> GetWaystones() const;

	UFUNCTION(BlueprintPure, Category = "Beyond|World")
	TArray<ABeyondPointOfInterest*> GetPlaces() const;

	UFUNCTION(BlueprintPure, Category = "Beyond|World")
	TArray<ABeyondRegionVolume*> GetRegionVolumes() const;

	UFUNCTION(BlueprintPure, Category = "Beyond|World")
	TArray<ABeyondBossArena*> GetArenas() const;

	//~ Waystones

	// The waystone F would use: the nearest one in its Interact Range of the leader
	UFUNCTION(BlueprintPure, Category = "Beyond|World")
	ABeyondWaystone* FindWaystoneInReach() const;

	UFUNCTION(BlueprintPure, Category = "Beyond|World")
	bool IsWaystoneAttuned(const ABeyondWaystone* Waystone) const;

	UFUNCTION(BlueprintPure, Category = "Beyond|World")
	bool IsWaystoneDiscovered(const ABeyondWaystone* Waystone) const;

	// F at a waystone: attunes it, or rests if it already is
	UFUNCTION(BlueprintCallable, Category = "Beyond|World")
	bool UseWaystone(ABeyondWaystone* Waystone);

	UFUNCTION(BlueprintCallable, Category = "Beyond|World")
	bool AttuneWaystone(ABeyondWaystone* Waystone);

	// Heal everyone (the downed get up), move the respawn point here, save, refill On Rest camps
	UFUNCTION(BlueprintCallable, Category = "Beyond|World")
	void RestAt(ABeyondWaystone* Waystone);

	//~ Travel

	// Why not (in a fight, talking, a demigod down, sealed in an arena, not attuned...) when false
	UFUNCTION(BlueprintPure, Category = "Beyond|World")
	bool CanFastTravel(FName WaystoneId, FText& OutWhyNot) const;

	UFUNCTION(BlueprintCallable, Category = "Beyond|World")
	bool FastTravelTo(FName WaystoneId);

	// Moves the whole party through the streaming hold (false if a move is already under way)
	UFUNCTION(BlueprintCallable, Category = "Beyond|World")
	bool TravelPartyTo(const FTransform& Destination, EBeyondTravelReason Reason, bool bHeal = false);

	UFUNCTION(BlueprintPure, Category = "Beyond|World")
	bool IsTravelling() const { return Travel.bActive; }

	// Partitioned and open-world maps move the party through the hold (respawns too)
	UFUNCTION(BlueprintPure, Category = "Beyond|World")
	bool ShouldHoldOnTravel() const;

	// Deep water: the leader takes the party back to the last safe ground; the companion just rejoins the leader
	void ReturnToSafeGround(ABeyondCharacterBase* Member);

	UFUNCTION(BlueprintPure, Category = "Beyond|World")
	FVector GetLastSafeGround() const { return LastSafeGround; }

	//~ Discovery and the map

	// Discover (and attune) everything (console Beyond.Discover all)
	UFUNCTION(BlueprintCallable, Category = "Beyond|World")
	void DiscoverEverything();

	UFUNCTION(BlueprintCallable, Category = "Beyond|World")
	TArray<FBeyondMapMarker> GetMapMarkers() const;

	// Applies the weather / music for where the leader is now (bInstant: no blend)
	void RefreshAmbient(bool bInstant);

	// Checks where the leader is right away (tests; it also runs on a timer)
	UFUNCTION(BlueprintCallable, Category = "Beyond|World")
	void ScanNow();

	// Banners show again at once (tests)
	void ForgetBanners() { BannerShownTimes.Reset(); }

	UPROPERTY(BlueprintAssignable, Category = "Beyond|World")
	FBeyondRegionChangedSignature OnRegionChanged;

	UPROPERTY(BlueprintAssignable, Category = "Beyond|World")
	FBeyondRegionChangedSignature OnVillageChanged;

	// A region, village, place or waystone was found for the first time
	UPROPERTY(BlueprintAssignable, Category = "Beyond|World")
	FBeyondDiscoveredSignature OnDiscovered;

	UPROPERTY(BlueprintAssignable, Category = "Beyond|World")
	FBeyondWaystoneSignature OnWaystoneAttuned;

	UPROPERTY(BlueprintAssignable, Category = "Beyond|World")
	FBeyondTravelSignature OnTravelStarted;

	UPROPERTY(BlueprintAssignable, Category = "Beyond|World")
	FBeyondTravelSignature OnTravelFinished;

	UPROPERTY(BlueprintAssignable, Category = "Beyond|World")
	FBeyondPartyRestedSignature OnPartyRested;

private:
	struct FTravelState
	{
		bool bActive = false;
		EBeyondTravelReason Reason = EBeyondTravelReason::Cheat;
		FTransform Destination;
		bool bHeal = false;
		bool bTeleported = false;
		bool bTriedFallback = false;
		double TeleportTime = 0.0;
		double GroundReadyTime = -1.0;
	};

	ABeyondPlayerController* GetPlayerController() const;
	ABeyondCharacterBase* GetLeader() const;
	bool HasAnythingToTrack() const;
	void UpdateScanning();
	void Scan();
	void UpdateRegions(const ABeyondCharacterBase* Leader);
	// First visit: saved, EXP, On Discovered; false if it was already known
	bool DiscoverRegionVisit(UBeyondRegionDefinition* Region, float& OutExp);
	void TryShowBanner(const UBeyondRegionDefinition* Region, bool bFirstVisit, float Exp);
	void UpdateDiscoveries(const ABeyondCharacterBase* Leader);
	void UpdateSafeGround(const ABeyondCharacterBase* Leader);
	void UpdateArenaMusic();
	void UpdateWaystoneLooks();
	void RegisterInvokers();
	void ShowBanner(const UBeyondRegionDefinition* Region, bool bFirstVisit, float Exp);
	void ShowToast(const FText& Title, const FText& Detail, const FLinearColor& Colour);
	float GetDiscoveryExp(const UBeyondRegionDefinition* Region) const;
	void SetCheckpoint(const FTransform& Transform);

	void DecideStart();
	void BeginTeleport();
	void TickTravel();
	void FinishTravel();
	bool IsStreamingReadyAt(const FVector& Location) const;
	bool FindGroundUnder(const FVector& Location, const AActor* Ignore, FVector& OutGround) const;
	bool IsNavigationReadyAt(const FVector& Location) const;
	void FreezeParty(bool bFreeze);
	void FadeCamera(float From, float To, float Time, bool bHold) const;

	TArray<TWeakObjectPtr<ABeyondRegionVolume>> RegionVolumes;
	TArray<TWeakObjectPtr<ABeyondWaystone>> Waystones;
	TArray<TWeakObjectPtr<ABeyondPointOfInterest>> Places;
	TArray<TWeakObjectPtr<ABeyondBossArena>> Arenas;
	TWeakObjectPtr<ABeyondWorldInfo> WorldInfo;

	TWeakObjectPtr<UBeyondRegionDefinition> CurrentRegion;
	TWeakObjectPtr<UBeyondRegionDefinition> CurrentVillage;
	TMap<FName, double> BannerShownTimes;

	FVector LastSafeGround = FVector::ZeroVector;
	bool bHasSafeGround = false;

	TWeakObjectPtr<ABeyondBossArena> MusicArena;
	TSet<TWeakObjectPtr<AActor>> Invokers;

	FTravelState Travel;
	FTimerHandle ScanTimer;
	FTimerHandle TravelTimer;
	FTimerHandle StartTimer;
	bool bStartDecided = false;
};
