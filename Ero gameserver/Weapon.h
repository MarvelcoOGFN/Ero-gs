#pragma once
#include "ue.h"
#include "Inventory.h"

__int64 (*OnReloadOG)(AFortWeapon* Weapon, int RemoveCount);
__int64 OnReload(AFortWeapon* Weapon, int RemoveCount)
{
	auto Ret = OnReloadOG(Weapon, RemoveCount);
	auto WeaponDef = Weapon->WeaponData;
	if (!WeaponDef)
		return Ret;

	auto AmmoDef = WeaponDef->GetAmmoWorldItemDefinition_BP();
	if (!AmmoDef)
		return Ret;
	AFortPlayerPawnAthena* Pawn = (AFortPlayerPawnAthena*)Weapon->GetOwner();
	AFortPlayerControllerAthena* PC = (AFortPlayerControllerAthena*)Pawn->Controller;
	if (!PC || !PC->Pawn || !PC->IsA(AFortPlayerControllerAthena::StaticClass()) || &PC->WorldInventory->Inventory == nullptr || GetGameState()->GamePhase >= EAthenaGamePhase::EndGame)
		return Ret;

	if (PC->bInfiniteAmmo) {
		UpdateLoadedAmmo(PC, Weapon, RemoveCount);
		return Ret;
	}

	FFortItemEntry* FoundEntry = nullptr;
	for (int32 i = 0; i < PC->WorldInventory->Inventory.ReplicatedEntries.Num(); i++)
	{
		FFortItemEntry& Entry = PC->WorldInventory->Inventory.ReplicatedEntries[i];

		if (Entry.ItemDefinition == WeaponDef && (Entry.Count < GetMaxStackSize(WeaponDef)))
		{
			FoundEntry = &PC->WorldInventory->Inventory.ReplicatedEntries[i];
		}
	}

	if (WeaponDef->GetName() == "Athena_C4" && FoundEntry && FoundEntry->Count == 1) {
		FoundEntry->Count = 0;
		ModifyEntry(PC, *FoundEntry);
		UpdateInventory(PC, FoundEntry);
		return Ret;
	}

	Remove(PC, AmmoDef, RemoveCount);
	UpdateLoadedAmmo(PC, Weapon, RemoveCount);

	return Ret;
}