#pragma once
#include "ue.h"
#include "Inventory.h"

bool IsPrimaryQuickbar(UFortItemDefinition* Def)
{
	return Def->IsA(UFortConsumableItemDefinition::StaticClass()) || Def->IsA(UFortWeaponRangedItemDefinition::StaticClass()) || Def->IsA(UFortGadgetItemDefinition::StaticClass());
}

static bool IsPrimaryQuickbar2(UFortItemDefinition* ItemDefinition)
{
	if (ItemDefinition->GetFullName().contains("UncleBrolly") || ItemDefinition->GetFullName().contains("D_Athena_Keycard")) return true;

	static auto FortWeaponMeleeItemDefinitionClass = StaticFindObject<UClass>("/Script/FortniteGame.FortWeaponMeleeItemDefinition");
	static auto FortEditToolItemDefinitionClass = StaticFindObject<UClass>("/Script/FortniteGame.FortEditToolItemDefinition");
	static auto FortBuildingItemDefinitionClass = StaticFindObject<UClass>("/Script/FortniteGame.FortBuildingItemDefinition");
	static auto FortAmmoItemDefinitionClass = StaticFindObject<UClass>("/Script/FortniteGame.FortAmmoItemDefinition");
	static auto FortResourceItemDefinitionClass = StaticFindObject<UClass>("/Script/FortniteGame.FortResourceItemDefinition");
	static auto FortTrapItemDefinitionClass = StaticFindObject<UClass>("/Script/FortniteGame.FortTrapItemDefinition");

	if (!ItemDefinition->IsA(FortWeaponMeleeItemDefinitionClass) && !ItemDefinition->IsA(FortEditToolItemDefinitionClass) &&
		!ItemDefinition->IsA(FortBuildingItemDefinitionClass) && !ItemDefinition->IsA(FortAmmoItemDefinitionClass)
		&& !ItemDefinition->IsA(FortResourceItemDefinitionClass) && !ItemDefinition->IsA(FortTrapItemDefinitionClass))
		return true;

	return false;
}

#define PrimQB(x) (IsPrimaryQuickbar(x) || IsPrimaryQuickbar2(x))

void (*ServerHandlePickupOG)(AFortPlayerPawn* Pawn, AFortPickup* Pickup, float InFlyTime, FVector InStartDirection, bool bPlayPickupSound);
void ServerHandlePickup(AFortPlayerPawn* Pawn, AFortPickup* Pickup, float InFlyTime, FVector InStartDirection, bool bPlayPickupSound)
{
	if (!Pawn || !Pickup || Pickup->bPickedUp)
		return ServerHandlePickupOG(Pawn, Pickup, InFlyTime, InStartDirection, bPlayPickupSound);

	AFortPlayerControllerAthena* PC = (AFortPlayerControllerAthena*)Pawn->Controller;
	if (PC->IsA(ABP_PhoebePlayerController_C::StaticClass())) // AAIController
		return;
	UFortItemDefinition* Def = Pickup->PrimaryPickupItemEntry.ItemDefinition;
	int PickupCount = Pickup->PrimaryPickupItemEntry.Count;
	int PickupLoadedAmmo = Pickup->PrimaryPickupItemEntry.LoadedAmmo;
	FFortItemEntry* FoundEntry = nullptr;
	FFortItemEntry* MiscItem = nullptr;
	int Items = 0;
	float MaxStackSize = GetMaxStackSize(Def);
	bool Stackable = Def->IsStackable();

	if (!Pawn->CurrentWeapon)
		return;

	std::string CWN = Pawn->CurrentWeapon->GetName();

	if (CWN.starts_with("B_Keycard_Athena_")) {
		CWN = CWN.substr(17);
	}
	else if (CWN.starts_with("B_UncleBrolly_") || CWN.starts_with("B_CoolCarpet_")) {
		CWN = CWN.substr(2);
	}
	else if (CWN.starts_with("B_Ranged_Lotus_Mustache")) {
		CWN = CWN.substr(9);
	}
	if (CWN.starts_with("TheAgency")) {
		CWN = CWN.substr(3);
	}

	for (int32 i = 0; i < PC->WorldInventory->Inventory.ReplicatedEntries.Num(); i++)
	{
		FFortItemEntry& Entry = PC->WorldInventory->Inventory.ReplicatedEntries[i];

		if (PrimQB(Def) && PrimQB(Entry.ItemDefinition))
		{
			if (Entry.ItemDefinition->GetName() == "AGID_Lotus_Mustache") {
				Items += 2;
			}
			else {
				Items++;
			}
			if (Items > 5)
			{
				if (!Entry.ItemDefinition->bAllowMultipleStacks) {
					Remove(PC, Entry.ItemDefinition);
				}
				else {
					Remove(PC, &Entry);
				}
			}
		}

		std::string WN = Entry.ItemDefinition->GetName();
		if (WN.starts_with("AGID_Athena_Keycard_")) {
			WN = WN.substr(20);
		}
		else if (WN.starts_with("WID_UncleBrolly")) {
			WN = WN.substr(4);
		}
		else if (WN.starts_with("AGID_Lotus_Mustache")) {
			WN = WN.substr(5);
		}
		else if (WN.starts_with("AGID_CoolCarpet")) {
			WN = WN.substr(5);
		}
		if (WN == "Base") {
			WN = "UndergroundBase";
		}
		if (WN == "HDP") {
			WN = "Yacht";
		}
		if (CWN.starts_with(WN)) {
			MiscItem = &PC->WorldInventory->Inventory.ReplicatedEntries[i];
		}

		if (Entry.ItemDefinition == Def && (Entry.Count < MaxStackSize))
		{
			FoundEntry = &PC->WorldInventory->Inventory.ReplicatedEntries[i];
		}
	}

	if (Items == 5 && PrimQB(Pawn->CurrentWeapon->WeaponData))
	{
		FFortItemEntry* SwapEntry = MiscItem ? MiscItem : FindEntry(PC, Pawn->CurrentWeapon->WeaponData);
		if (!SwapEntry || !SwapEntry->ItemDefinition)
			return;
		float mMaxStackSize = GetMaxStackSize(SwapEntry->ItemDefinition);
		if (MiscItem || (!FoundEntry && !Pawn->CurrentWeapon->WeaponData->IsStackable()) || SwapEntry->ItemDefinition != Def || SwapEntry->Count >= mMaxStackSize)
		{
			if (MiscItem || (!(Pickup->PrimaryPickupItemEntry.ItemDefinition->IsStackable() && FoundEntry && FoundEntry->Count > 0) && SwapEntry->ItemDefinition->IsA(UFortWeaponRangedItemDefinition::StaticClass())))
			{
				SpawnPickup(PC->GetViewTarget()->K2_GetActorLocation(), SwapEntry, EFortPickupSourceTypeFlag::Tossed, EFortPickupSpawnSource::Unset, Pawn);
				Remove(PC, Pawn->CurrentWeapon->GetInventoryGUID());
			}
		}
	}

	if (FoundEntry)
	{
		if (Stackable)
		{
			FoundEntry->Count += PickupCount;
			if (FoundEntry->Count > MaxStackSize)
			{
				int Count = FoundEntry->Count;
				FoundEntry->Count = (int)MaxStackSize;

				if (Def->bAllowMultipleStacks)
				{
					if (Items >= 5)
					{
						SpawnPickup(PC->GetViewTarget()->K2_GetActorLocation(), FoundEntry, EFortPickupSourceTypeFlag::Tossed, EFortPickupSpawnSource::Unset, Pawn, Count - (int)MaxStackSize);
					}
					else
					{
						GiveItem(PC, Def, Count - (int)MaxStackSize);
					}
				}
				else
				{
					SpawnPickup(PC->GetViewTarget()->K2_GetActorLocation(), FoundEntry, EFortPickupSourceTypeFlag::Tossed, EFortPickupSpawnSource::Unset, Pawn, Count - (int)MaxStackSize);
				}
			}
			ModifyEntry(PC, *FoundEntry);
			UpdateInventory(PC, FoundEntry);
		}
		else if (Def->bAllowMultipleStacks)
		{
			if (Items == 6) {
				if (PrimQB(FoundEntry->ItemDefinition)) {
					Def = FoundEntry->ItemDefinition;
					FFortItemEntry CW;
					for (int32 i = 0; i < PC->WorldInventory->Inventory.ReplicatedEntries.Num(); i++)
					{
						if (PC->WorldInventory->Inventory.ReplicatedEntries[i].ItemGuid == Pawn->CurrentWeapon->ItemEntryGuid) {
							CW = PC->WorldInventory->Inventory.ReplicatedEntries[i];
						}
					}
					Remove(PC, Pawn->CurrentWeapon->ItemEntryGuid);
					GiveItem(PC, Def, PickupCount, PickupLoadedAmmo);
				}
			}
			else {
				GiveItem(PC, Def, PickupCount, PickupLoadedAmmo);
			}
		}
	}
	else
	{
		if (Items == 6) {
			Def = PC->WorldInventory->Inventory.ReplicatedEntries[4].ItemDefinition;
			FFortItemEntry* CW = nullptr;
			for (int32 /*size_t*/ i = 0; i < PC->WorldInventory->Inventory.ReplicatedEntries.Num(); i++)
			{
				if (PC->WorldInventory->Inventory.ReplicatedEntries[i].ItemGuid == Pawn->CurrentWeapon->ItemEntryGuid) {
					CW = &PC->WorldInventory->Inventory.ReplicatedEntries[i];
				}
			}
			if (CW != nullptr) Remove(PC, Pawn->CurrentWeapon->ItemEntryGuid);
			GiveItem(PC, Def, PickupCount, PickupLoadedAmmo);
		}
		else {
			GiveItem(PC, Def, PickupCount, PickupLoadedAmmo);
		}
	}

	Pickup->PickupLocationData.bPlayPickupSound = bPlayPickupSound;
	Pickup->PickupLocationData.FlyTime = 0.4f;
	Pickup->PickupLocationData.ItemOwner = Pawn;
	Pickup->PickupLocationData.PickupGuid = Pickup->PrimaryPickupItemEntry.ItemGuid;
	Pickup->PickupLocationData.PickupTarget = Pawn;
	Pickup->OnRep_PickupLocationData();

	Pickup->bPickedUp = true;
	Pickup->OnRep_bPickedUp();
	return ServerHandlePickupOG(Pawn, Pickup, InFlyTime, InStartDirection, bPlayPickupSound);
}

void (*NetMulticast_Athena_BatchedDamageCuesOG)(AFortPawn* Pawn, FAthenaBatchedDamageGameplayCues_Shared SharedData, FAthenaBatchedDamageGameplayCues_NonShared NonSharedData);
void NetMulticast_Athena_BatchedDamageCues(AFortPawn* Pawn, FAthenaBatchedDamageGameplayCues_Shared SharedData, FAthenaBatchedDamageGameplayCues_NonShared NonSharedData)
{
	if (!Pawn || Pawn->Controller->IsA(ABP_PhoebePlayerController_C::StaticClass()))
		return;

	if (Pawn->CurrentWeapon)
		UpdateLoadedAmmo((AFortPlayerController*)Pawn->Controller, ((AFortPlayerPawn*)Pawn)->CurrentWeapon);

	return NetMulticast_Athena_BatchedDamageCuesOG(Pawn, SharedData, NonSharedData);
}

void ServerReviveFromDBNO(AFortPlayerPawnAthena* Pawn, AFortPlayerControllerAthena* Instigator)
{
	ReviveCounts[Instigator]++;
	Pawn->bIsDBNO = false;
	Pawn->OnRep_IsDBNO();
	auto PC = ((AFortPlayerControllerAthena*)Pawn->Controller);
	PC->RespawnPlayerAfterDeath(false);
	Pawn->SetHealth(30);
	RemoveAbility(PC, UGAB_AthenaDBNO_C::StaticClass());
}

void ServerSendZiplineState(AFortPlayerPawnAthena* Pawn, FZiplinePawnState State)
{
	if (!Pawn)
		return;

	Pawn->ZiplineState = State;
	OnRep_ZiplineState(Pawn);

	if (State.bJumped)
	{
		Pawn->LaunchCharacter(FVector{ 0,0,1200 }, false, false);
	}
}

void (*OnCapsuleBeginOverlapOG)(AFortPlayerPawn* Pawn, UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, FHitResult SweepResult);
void OnCapsuleBeginOverlap(AFortPlayerPawn* Pawn, UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, FHitResult SweepResult)
{
	if (OtherActor->IsA(AFortPickup::StaticClass()))
	{
		AFortPickup* Pickup = (AFortPickup*)OtherActor;

		if (Pickup->PawnWhoDroppedPickup == Pawn)
			return OnCapsuleBeginOverlapOG(Pawn, OverlappedComp, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);

		UFortWorldItemDefinition* Def = (UFortWorldItemDefinition*)Pickup->PrimaryPickupItemEntry.ItemDefinition;

		if (!Def) {
			return OnCapsuleBeginOverlapOG(Pawn, OverlappedComp, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
		}

		auto PC = (AFortPlayerControllerAthena*)Pawn->GetOwner();
		FFortItemEntry* FoundEntry = nullptr;
		auto HighestCount = 0;

		if (PC->IsA(ABP_PhoebePlayerController_C::StaticClass())) return OnCapsuleBeginOverlapOG(Pawn, OverlappedComp, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
		for (int32 i = 0; i < PC->WorldInventory->Inventory.ReplicatedEntries.Num(); i++)
		{
			FFortItemEntry& Entry = PC->WorldInventory->Inventory.ReplicatedEntries[i];

			if (Entry.ItemDefinition == Def && (Entry.Count <= GetMaxStackSize(Def)))
			{
				FoundEntry = &PC->WorldInventory->Inventory.ReplicatedEntries[i];
				if (Entry.Count > HighestCount)
					HighestCount = Entry.Count;
			}
		}

		if (FoundEntry && (HighestCount <= GetMaxStackSize(Def)) && (Def->IsA(UFortAmmoItemDefinition::StaticClass()) || Def->IsA(UFortResourceItemDefinition::StaticClass()) || Def->IsA(UFortTrapItemDefinition::StaticClass()))) {

		}
		if (Def->IsA(UFortAmmoItemDefinition::StaticClass()) || Def->IsA(UFortResourceItemDefinition::StaticClass()) || Def->IsA(UFortTrapItemDefinition::StaticClass())) {
			if (FoundEntry && HighestCount < GetMaxStackSize(Def)) {
				Pawn->ServerHandlePickup(Pickup, 0.40f, FVector(), true);
			}
			else if (!FoundEntry) {
				Pawn->ServerHandlePickup(Pickup, 0.40f, FVector(), true);
			}
		}
		else if (FoundEntry)
		{
			if (FoundEntry->Count < GetMaxStackSize(Def)) {
				Pawn->ServerHandlePickup(Pickup, 0.40f, FVector(), true);
			}
		}
	}

	return OnCapsuleBeginOverlapOG(Pawn, OverlappedComp, OtherActor, OtherComp, OtherBodyIndex, bFromSweep, SweepResult);
}

void ServerHandlePickupWithSwap(AFortPlayerPawnAthena* Pawn, AFortPickup* Pickup, FGuid Swap, float InFlyTime, FVector InStartDirection, bool bPlayPickupSound)
{
	if (!Pawn || !Pickup || Pickup->bPickedUp)
		return;

	AFortPlayerControllerAthena* PC = (AFortPlayerControllerAthena*)Pawn->Controller;

	FFortItemEntry* Entry = FindEntry(PC, Swap);
	if (!Entry || !Entry->ItemDefinition || Entry->ItemDefinition->IsA(UFortWeaponMeleeItemDefinition::StaticClass()))
		return;

	SpawnPickup(Pawn->K2_GetActorLocation(), Entry, EFortPickupSourceTypeFlag::Player, EFortPickupSpawnSource::Unset, Pawn);
	Remove(PC, Swap);
	Pawn->ServerHandlePickup(Pickup, InFlyTime, InStartDirection, bPlayPickupSound);
}

void PickUpAction(UFortControllerComponent_Interaction* Comp, UFortItemDefinition* C4) {
	AFortPlayerControllerAthena* PC = (AFortPlayerControllerAthena*)Comp->GetOwner();
	auto Pawn = PC->MyFortPawn;
	auto Def = C4;
	int Items = 0;
	FFortItemEntry* FoundEntry = nullptr;
	FFortItemEntry* MiscItem = nullptr;
	float MaxStackSize = GetMaxStackSize(Def);
	bool Stackable = Def->IsStackable();

	std::string CWN = Pawn->CurrentWeapon->GetName();

	for (int32 i = 0; i < PC->WorldInventory->Inventory.ReplicatedEntries.Num(); i++)
	{
		FFortItemEntry& Entry = PC->WorldInventory->Inventory.ReplicatedEntries[i];

		if (IsPrimaryQuickbar2(Def) && IsPrimaryQuickbar2(Entry.ItemDefinition))
		{
			Items++;
			if (Items > 5)
			{
				Remove(PC, Entry.ItemGuid);
			}
		}

		std::string WN = Entry.ItemDefinition->GetName();

		if (WN == "Base") {
			WN = "UndergroundBase";
		}
		if (CWN.starts_with(WN)) {
			MiscItem = &PC->WorldInventory->Inventory.ReplicatedEntries[i];
		}

		if (Entry.ItemDefinition == Def && (Entry.Count < MaxStackSize))
		{
			FoundEntry = &PC->WorldInventory->Inventory.ReplicatedEntries[i];
		}
	}

	if (Items == 5 && IsPrimaryQuickbar2(Pawn->CurrentWeapon->WeaponData))
	{
		FFortItemEntry* SwapEntry = MiscItem ? MiscItem : FindEntry(PC, Pawn->CurrentWeapon->WeaponData);
		if (!SwapEntry || !SwapEntry->ItemDefinition)
			return;
		float mMaxStackSize = GetMaxStackSize(SwapEntry->ItemDefinition);

		if (!FoundEntry && !Pawn->CurrentWeapon->WeaponData->IsStackable() || SwapEntry->ItemDefinition != Def || SwapEntry->Count >= mMaxStackSize)
		{
			if (MiscItem || (!(Def->IsStackable() && FoundEntry && FoundEntry->Count > 0) && SwapEntry->ItemDefinition->IsA(UFortWeaponItemDefinition::StaticClass())))
			{

				SpawnPickup(PC->GetViewTarget()->K2_GetActorLocation(), SwapEntry, EFortPickupSourceTypeFlag::Tossed, EFortPickupSpawnSource::Unset, Pawn);
				Remove(PC, Pawn->CurrentWeapon->GetInventoryGUID());
			}
		}
	}

	if (FoundEntry)
	{
		if (Stackable)
		{
			FoundEntry->Count += 1;
			if (FoundEntry->Count > MaxStackSize)
			{
				int Count = FoundEntry->Count;
				FoundEntry->Count = (int)MaxStackSize;

				if (Def->bAllowMultipleStacks)
				{
					if (Items == 5)
					{
						SpawnPickup(PC->GetViewTarget()->K2_GetActorLocation(), FoundEntry, EFortPickupSourceTypeFlag::Tossed, EFortPickupSpawnSource::Unset, Pawn, Count - (int)MaxStackSize);
					}
					else
					{
						GiveItem(PC, Def, Count - (int)MaxStackSize);
					}
				}
				else
				{
					SpawnPickup(PC->GetViewTarget()->K2_GetActorLocation(), FoundEntry, EFortPickupSourceTypeFlag::Tossed, EFortPickupSpawnSource::Unset, Pawn, Count - (int)MaxStackSize);
				}
			}
			ModifyEntry(PC, *FoundEntry);
			UpdateInventory(PC, FoundEntry);
		}
		else if (Def->bAllowMultipleStacks)
		{
			GiveItem(PC, Def, 1, 0);
		}
	}
	else
	{
		GiveItem(PC, Def, 1, 0);
	}
	return;
}