#pragma once
#include "ue.h"
#include <intrin.h>

vector<class Bot2*> Bots2{};

class Bot2
{
public:
	ABP_PhoebePlayerController_C* PC;
	AFortPlayerPawnAthena* Pawn;
	bool bTickEnabled = false;
	AActor* CurrentTarget = nullptr;
	uint64_t tick_counter = 0;
	UFortWeaponItemDefinition* Weapon = nullptr;
	FGuid WeaponGuid;
	bool jumping = false;
	int shotCounter = 0;
	AActor* FollowTarget;
	bool follow = true;
	EAlertLevel AlertLevel = EAlertLevel::Unaware;
	double MovementDistance = 0;
	FVector lastLocation{};
	std::string CID = "";

public:
	Bot2(AFortPlayerPawnAthena* Pawn)
	{
		this->Pawn = Pawn;
		PC = (ABP_PhoebePlayerController_C*)Pawn->Controller;
		Bots2.push_back(this);
	}

public:
	FVector FindllamaSpawn(AFortAthenaMapInfo* MapInfo, FVector Center, float Radius)
	{
		static FVector* (*PickSupplyDropLocationOriginal)(AFortAthenaMapInfo * MapInfo, FVector * outLocation, __int64 Center, float Radius) = decltype(PickSupplyDropLocationOriginal)(__int64(GetModuleHandleA(0)) + 0x18848f0);

		if (!PickSupplyDropLocationOriginal)
			return FVector(0, 0, 0);


		FVector Out = FVector(0, 0, 0);
		auto ahh = PickSupplyDropLocationOriginal(MapInfo, &Out, __int64(&Center), Radius);
		return Out;
	}
	bool spawned = false;

	void Spawnllama() {
		auto MI = GetGameState()->MapInfo;
		int numPlayers = GetGameState()->GameMemberInfoArray.Members.Num();

		FFortAthenaAIBotRunTimeCustomizationData runtimeData;
		runtimeData.CustomSquadId = 3 + numPlayers;

		FRotator RandomYawRotator{};
		RandomYawRotator.Yaw = (float)rand() * 0.010986663f;

		int Radius = 100000;
		FVector Location = FindllamaSpawn(MI, FVector(1, 1, 10000), (float)Radius);

		auto Llama = SpawnActor<AFortAthenaSupplyDrop>(MI->LlamaClass.Get(), Location, RandomYawRotator);

		auto GroundLocation = Llama->FindGroundLocationAt(Location);

		Llama->K2_DestroyActor();

		spawned = true;
	}

	FGuid GetItem(UFortItemDefinition* Def)
	{
		for (int32 i = 0; i < PC->Inventory->Inventory.ReplicatedEntries.Num(); i++)
		{
			if (PC->Inventory->Inventory.ReplicatedEntries[i].ItemDefinition == Def)
				return PC->Inventory->Inventory.ReplicatedEntries[i].ItemGuid;
		}
		return FGuid();
	}

	void GiveItem(UFortItemDefinition* ODef, int Count = 1, bool equip = true)
	{
		UFortItemDefinition* Def = ODef;
		if (Def->GetName() == "AGID_Boss_Tina") {
			Def = StaticLoadObject<UFortItemDefinition>("/Game/Athena/Items/Weapons/Boss/WID_Boss_Tina.WID_Boss_Tina");
		}
		UFortWorldItem* Item = (UFortWorldItem*)Def->CreateTemporaryItemInstanceBP(Count, 0);
		Item->OwnerInventory = PC->Inventory;

		if (Def->IsA(UFortWeaponRangedItemDefinition::StaticClass()))
		{
			Item->ItemEntry.LoadedAmmo = GetAmmoForDef((UFortWeaponItemDefinition*)Def);
		}

		PC->Inventory->Inventory.ReplicatedEntries.Add(Item->ItemEntry);
		PC->Inventory->Inventory.ItemInstances.Add(Item);
		PC->Inventory->Inventory.MarkItemDirty(Item->ItemEntry);
		PC->Inventory->HandleInventoryLocalUpdate();
		if (Def->IsA(UFortWeaponRangedItemDefinition::StaticClass()) && equip)
		{
			this->Weapon = (UFortWeaponItemDefinition*)ODef;
			this->WeaponGuid = GetItem(Def);
			Pawn->EquipWeaponDefinition((UFortWeaponItemDefinition*)Def, GetItem(Def));
		}
	}

	virtual void OnPerceptionSensed(AActor* SourceActor, FAIStimulus& Stimulus)
	{
		if (Stimulus.bSuccessfullySensed == 1 && PC->LineOfSightTo(SourceActor, FVector(), true) && Pawn->GetDistanceTo(SourceActor) < 5000)
		{
			CurrentTarget = SourceActor;
			cout << "bSuccessfullySensed" << endl;
		}
	}
};

void TickBots()
{
	auto block = [](Bot2* bot, std::function<void(Bot2* bot)> const& SetUnaware, bool Alerted, bool Threatened, bool LKP) {
		if (!bot->CurrentTarget && !bot->follow) {
			bot->follow = true;
		}
		if (bot->bTickEnabled && bot->Pawn && bot->FollowTarget && bot->follow) {
			auto BotPos = bot->Pawn->K2_GetActorLocation();
			auto TargetPos = bot->FollowTarget->K2_GetActorLocation();
			auto TestRot = GetMath()->FindLookAtRotation(BotPos, TargetPos);

			bot->PC->SetControlRotation(TestRot);
			bot->PC->K2_SetActorRotation(TestRot, true);
			bot->PC->MoveToActor(bot->FollowTarget, 50, true, false, true, nullptr, true);

			if (!bot->CurrentTarget
				&& bot->PC->PlayerBotPawn
				&& bot->FollowTarget
				&& bot->PC->PlayerBotPawn->PlayerState
				&& (AFortPlayerStateAthena*)((AFortPlayerPawnAthena*)bot->FollowTarget)->PlayerState
				&& ((AFortPlayerStateAthena*)bot->PC->PlayerBotPawn->PlayerState)->PlayerTeam
				!= ((AFortPlayerStateAthena*)((AFortPlayerPawnAthena*)bot->FollowTarget)->PlayerState)->PlayerTeam) {
				bot->CurrentTarget = bot->FollowTarget;
			}
		}
		if (bot->CurrentTarget
			&& bot->PC->PlayerBotPawn
			&& bot->PC->PlayerBotPawn->PlayerState
			&& ((AFortPlayerPawnAthena*)bot->CurrentTarget)->PlayerState
			&& ((AFortPlayerStateAthena*)bot->PC->PlayerBotPawn->PlayerState)->PlayerTeam
			== ((AFortPlayerStateAthena*)((AFortPlayerPawnAthena*)bot->CurrentTarget)->PlayerState)->PlayerTeam) {
			bot->CurrentTarget = nullptr;
		}
		if (bot->CurrentTarget && ((AFortPlayerPawnAthena*)bot->CurrentTarget)->IsDead()) {
			bot->CurrentTarget = nullptr;
			bot->Pawn->PawnStopFire(0);
		}
		if (bot->bTickEnabled
			&& bot->Pawn
			&& !bot->Pawn->bIsDBNO
			&& bot->CurrentTarget
			&& !((AFortPlayerPawnAthena*)bot->CurrentTarget)->IsDead())
		{
			if (bot->Pawn->CurrentWeapon && bot->Pawn->CurrentWeapon->WeaponData->IsA(UFortWeaponItemDefinition::StaticClass()))
			{
				auto BotPos = bot->Pawn->K2_GetActorLocation();
				auto TargetPos = bot->CurrentTarget->K2_GetActorLocation();
				float Distance = bot->Pawn->GetDistanceTo(bot->CurrentTarget);

				if (Alerted) {
					bot->Pawn->PawnStopFire(0);
					bot->PC->StopMovement();
					auto Rot = GetMath()->FindLookAtRotation(BotPos, TargetPos);
					bot->PC->SetControlRotation(Rot);
					bot->PC->K2_SetActorRotation(Rot, true);
					bot->tick_counter++;
					if (bot->tick_counter % 150 == 0 && bot->PC->LineOfSightTo(bot->CurrentTarget, FVector(), true) && Distance < 2500) {
						bot->PC->CurrentAlertLevel = EAlertLevel::Threatened;
						bot->PC->OnAlertLevelChanged(bot->AlertLevel, EAlertLevel::Threatened);
					}
					return;
				}
				else if (Threatened) {
					if (Distance < 300 && bot->PC->LineOfSightTo(bot->CurrentTarget, FVector(), true)) bot->PC->StopMovement();
					if (!bot->Pawn->bIsCrouched && GetMath()->RandomBoolWithWeight(0.01f)) {
						bot->Pawn->Crouch(false);
					}
					else if (GetMath()->RandomBoolWithWeight(0.01f)) {
						bot->Pawn->Jump();
						bot->jumping = true;
					}
					else if (bot->Pawn->bIsCrouched && (bot->tick_counter % 30) == 0) {
						bot->Pawn->UnCrouch(false);
					}
					else if (bot->jumping && (bot->tick_counter % 10) == 0) {
						bot->Pawn->StopJumping();
						bot->jumping = false;
					}
				}
				else if (LKP) {
					auto LKPRun = [bot, BotPos, TargetPos]() {
						bot->Pawn->UnCrouch(false);
						bot->Pawn->StopJumping();
						bot->jumping = false;
						bot->PC->MoveToActor(bot->CurrentTarget, 150, true, false, true, nullptr, true);
						auto Rot = GetMath()->FindLookAtRotation((FVector&)BotPos, (FVector&)TargetPos);
						bot->PC->SetControlRotation(Rot);
						bot->PC->K2_SetActorRotation(Rot, true);
						};
					bot->tick_counter++;

					if (bot->tick_counter % 30 == 0) 
					{

					  if (bot->Weapon)
					  {
						bot->Pawn->EquipWeaponDefinition(bot->Weapon, bot->WeaponGuid);
						bot->Pawn->PawnStopFire(0);
					  }

						bot->MovementDistance = 0;
					}
					else {
						FVector diff = BotPos - bot->lastLocation;
						double mchange = diff.X + diff.Y + diff.Z;
						bot->MovementDistance += mchange;
					}
					bot->lastLocation = BotPos;
					LKPRun();
					return;
				}
				else {
					bot->CurrentTarget = nullptr;
					SetUnaware(bot);
					bot->tick_counter++;
					return;
				}

				std::string WeaponName = bot->Pawn->CurrentWeapon->Name.ToString();
				if (WeaponName.starts_with("B_Assault_MidasDrum_Athena_")) 
				{
					if (bot->shotCounter >= 19) {
						bot->Pawn->PawnStopFire(0);
						bot->shotCounter = -27;
					}
				}
				else if (WeaponName.starts_with("B_Minigun_Athena_")) 
				{
					if (bot->shotCounter >= 103) {
						bot->Pawn->PawnStopFire(0);
						bot->shotCounter = -27;
					}
				}
				else if (WeaponName.starts_with("B_Pistol_RapidFireSMG_Athena_")) 
				{
					if (bot->shotCounter >=15) {
						bot->Pawn->PawnStopFire(0);
						bot->shotCounter = -27;
					}
				}
				else if (WeaponName.starts_with("B_Assault_Auto_Zoom_SR_Child_Athena_")) 
				{
					if (bot->shotCounter >= 44) {
						bot->Pawn->PawnStopFire(0);
						bot->shotCounter = -27;
					}
				}
				else if (WeaponName.starts_with("B_Assault_Heavy_Athena_"))
				{
					if (bot->shotCounter >= 147) {
						bot->Pawn->PawnStopFire(0);
						bot->shotCounter = -27;
					}
				}
				else if (WeaponName.starts_with("B_Assault_Heavy_SR_Athena_"))
				{
					if (bot->shotCounter >= 147) {
						bot->Pawn->PawnStopFire(0);
						bot->shotCounter = -27;
					}
				}
				else if (WeaponName.starts_with("B_Assault_Suppressed_Athena_"))
				{
					//skip
				}
				else if (WeaponName.starts_with("B_Pistol_AutoHeavy_Athena_Supp_Child_"))
				{
					if (bot->shotCounter >= 23)
					{ 
						bot->Pawn->PawnStopFire(0);
						bot->shotCounter = -27;
					}
				}
				else if (WeaponName.starts_with("B_Pistol_Vigilante_Athena_")) 
				{
                  //skip
				}
				else if (WeaponName.starts_with("B_TnTinaBow_Athena_"))
				{
					//skip
				}
				else if (WeaponName.starts_with("B_DualPistol_Donut_Athena_")) 
				{
					if (bot->shotCounter >= 25) {
						bot->Pawn->PawnStopFire(0);
						bot->shotCounter = -35;
					}
				}
				else if (bot->shotCounter >= 19) {
					bot->Pawn->PawnStopFire(0);
					bot->shotCounter = -27;
				}

				if (Threatened) {
					FRotator TestRot;
					if (bot->tick_counter % 30 == 0) {
						float RandomXmod = (rand() % 120000 - 60000); 
						float RandomYmod = (rand() % 120000 - 60000);  
						float RandomZmod = (rand() % 120000 - 60000);

						FVector TargetPosMod{ TargetPos.X + RandomXmod, TargetPos.Y + RandomYmod, TargetPos.Z + RandomZmod };

						TestRot = GetMath()->FindLookAtRotation(BotPos, TargetPosMod);
					}

					bot->follow = true;
					if (bot->tick_counter % 30 == 0) {
						bot->PC->SetControlRotation(TestRot);
						bot->PC->K2_SetActorRotation(TestRot, true);
					}
					bot->PC->MoveToActor(bot->CurrentTarget, 150, true, false, true, nullptr, true);
					if (bot->tick_counter % 30 == 0) {
					if (bot->Weapon)
					{
						bot->Pawn->EquipWeaponDefinition(bot->Weapon, bot->WeaponGuid);

					    if (bot->shotCounter >= 0) bot->Pawn->PawnStartFire(0);
					}

						bot->MovementDistance = 0;
					}
					else {
						FVector diff = BotPos - bot->lastLocation;
						double mchange = diff.X + diff.Y + diff.Z;
						bot->MovementDistance += mchange;
					}
					bot->lastLocation = BotPos;
					if (bot->Weapon) bot->Pawn->EquipWeaponDefinition(bot->Weapon, bot->WeaponGuid);
					bot->shotCounter++;
				}
			}
		}
		else if (bot->bTickEnabled && bot->Pawn && bot->Pawn->bIsDBNO)
		{
			bot->CurrentTarget = nullptr;
		}
		bot->tick_counter++;
		};
	auto SetUnaware = [](Bot2* bot) {
		bot->PC->CurrentAlertLevel = EAlertLevel::Unaware;
		bot->PC->OnAlertLevelChanged(bot->AlertLevel, EAlertLevel::Unaware);
		bot->Pawn->PawnStopFire(0);
		bot->PC->StopMovement();
		};
	for (auto bot2 : Bots2)
	{
		auto Alerted = bot2->PC->CurrentAlertLevel == EAlertLevel::Alerted;
		auto Threatened = bot2->PC->CurrentAlertLevel == EAlertLevel::Threatened;
		auto LKP = bot2->PC->CurrentAlertLevel == EAlertLevel::LKP;
		block(bot2, SetUnaware, Alerted, Threatened, LKP);
	}
}

wchar_t* (*OnPerceptionSensedOG)(ABP_PhoebePlayerController_C* PC, AActor* SourceActor, FAIStimulus& Stimulus);
wchar_t* OnPerceptionSensed(ABP_PhoebePlayerController_C* PC, AActor* SourceActor, FAIStimulus& Stimulus)
{
	if (SourceActor->IsA(AFortPlayerPawnAthena::StaticClass()) && Cast<AFortPlayerPawnAthena>(SourceActor)->Controller && !Cast<AFortPlayerPawnAthena>(SourceActor)->Controller->IsA(ABP_PhoebePlayerController_C::StaticClass()) /*!Cast<AFortPlayerPawnAthena>(SourceActor)->Controller->IsA(ABP_PhoebePlayerController_C::StaticClass())*/)
	{
		for (auto bot : Bots2)
		{
			if (bot->PC == PC)
			{
				bot->OnPerceptionSensed(SourceActor, Stimulus);
			}
		}
	}
	return OnPerceptionSensedOG(PC, SourceActor, Stimulus);
}
void (*OnPossessedPawnDiedOG)(ABP_PhoebePlayerController_C* PC, AActor* DamagedActor, float Damage, AController* InstigatedBy, AActor* DamageCauser, FVector HitLocation, UPrimitiveComponent* HitComp, FName BoneName, FVector Momentum);
void OnPossessedPawnDied(ABP_PhoebePlayerController_C* PC, AActor* DamagedActor, float Damage, AController* InstigatedBy, AActor* DamageCauser, FVector HitLocation, UPrimitiveComponent* HitComp, FName BoneName, FVector Momentum)
{
	Bot2* KilledBot = nullptr;
	for (size_t i = 0; i < Bots2.size(); i++)
	{
		auto bot = Bots2[i];
		if (bot && bot->PC == PC)
		{
			if (bot->Pawn->GetName().starts_with("BP_Pawn_DangerGrape_")) {
				goto nodrop;
			}
			else {
				KilledBot = bot;
			}
		}
	}
	PC->PlayerBotPawn->SetMaxShield(0);
	for (int32 i = 0; i < PC->Inventory->Inventory.ReplicatedEntries.Num(); i++)
	{
		if (PC->Inventory->Inventory.ReplicatedEntries[i].ItemDefinition->IsA(UFortWeaponMeleeItemDefinition::StaticClass()) || PC->Inventory->Inventory.ReplicatedEntries[i].ItemDefinition->IsA(UFortAmmoItemDefinition::StaticClass()))
			continue;
		auto Def = PC->Inventory->Inventory.ReplicatedEntries[i].ItemDefinition;
		if (Def->GetName() == "AGID_Athena_Keycard_Yacht") {
			goto nodrop;
		}
		if (Def->GetName() == "WID_Boss_Tina") {
			Def = KilledBot->Weapon;
		}
		SpawnPickup(PC->Pawn->K2_GetActorLocation(), Def, PC->Inventory->Inventory.ReplicatedEntries[i].Count, PC->Inventory->Inventory.ReplicatedEntries[i].LoadedAmmo, EFortPickupSourceTypeFlag::Other, EFortPickupSpawnSource::PlayerElimination);
		int Ammo = 0;
		int AmmoC = 0;
		if (Def->GetName() == "WID_Boss_Hos_MG") {
			Ammo = 60;
			AmmoC = 60;
		}
		else if (Def->GetName().starts_with("WID_Assault_LMG_Athena_")) {
			Ammo = 45;
			AmmoC = 45;
		}
		if (Def->IsA(UFortWeaponRangedItemDefinition::StaticClass()))
		{
			UFortWeaponItemDefinition* Def2 = (UFortWeaponItemDefinition*)Def;
			SpawnPickup(PC->Pawn->K2_GetActorLocation(), Def2->GetAmmoWorldItemDefinition_BP(), AmmoC != 0 ? AmmoC : GetAmmoForDef(Def2), Ammo != 0 ? Ammo : GetAmmoForDef(Def2), EFortPickupSourceTypeFlag::Other, EFortPickupSpawnSource::PlayerElimination);
		}
	}

nodrop:
	for (size_t i = 0; i < Bots2.size(); i++)
	{
		auto bot = Bots2[i];
		if (bot && bot->PC == PC)
		{
			Bots2.erase(Bots2.begin() + i);
			break;
		}
	}

	return OnPossessedPawnDiedOG(PC, DamagedActor, Damage, InstigatedBy, DamageCauser, HitLocation, HitComp, BoneName, Momentum);
}

void spawnMeowscles();

AFortPlayerPawnAthena* (*SpawnBotOG)(UFortServerBotManagerAthena* BotManager, FVector SpawnLoc, FRotator SpawnRot, UFortAthenaAIBotCustomizationData* BotData, FFortAthenaAIBotRunTimeCustomizationData RuntimeBotData);
AFortPlayerPawnAthena* SpawnBot(UFortServerBotManagerAthena* BotManager, FVector SpawnLoc, FRotator SpawnRot, UFortAthenaAIBotCustomizationData* BotData, FFortAthenaAIBotRunTimeCustomizationData RuntimeBotData)
{
	if (__int64(_ReturnAddress()) - __int64(GetModuleHandleA(0)) == 0x1A4153F) {
		return SpawnBotOG(BotManager, SpawnLoc, SpawnRot, BotData, RuntimeBotData);
	}

	spawnMeowscles();

	std::string BotName = BotData->Name.ToString();

	if (BotData->GetFullName().contains("MANG_POI_Yacht"))
	{
		BotData = StaticLoadObject<UFortAthenaAIBotCustomizationData>("/Game/Athena/AI/MANG/BotData/BotData_MANG_POI_HDP.BotData_MANG_POI_HDP");
	}

	if (BotData->CharacterCustomization->CustomizationLoadout.Character->GetName() == "CID_556_Athena_Commando_F_RebirthDefaultA")
	{
		std::string Tag = RuntimeBotData.PredefinedCosmeticSetTag.TagName.ToString();
		if (Tag == "Athena.Faction.Alter") {
			BotData->CharacterCustomization->CustomizationLoadout.Character = StaticLoadObject<UAthenaCharacterItemDefinition>("/Game/Athena/Items/Cosmetics/Characters/CID_NPC_Athena_Commando_M_HenchmanBad.CID_NPC_Athena_Commando_M_HenchmanBad");
		}
		else if (Tag == "Athena.Faction.Ego") {
			BotData->CharacterCustomization->CustomizationLoadout.Character = StaticLoadObject<UAthenaCharacterItemDefinition>("/Game/Athena/Items/Cosmetics/Characters/CID_NPC_Athena_Commando_M_HenchmanGood.CID_NPC_Athena_Commando_M_HenchmanGood");
		}
	}

	AActor* SpawnLocator = SpawnActor<ADefaultPawn>(SpawnLoc, SpawnRot);
	AFortPlayerPawnAthena* Ret = BotMutator->SpawnBot(BotData->PawnClass, SpawnLocator, SpawnLoc, SpawnRot, true);
	AFortAthenaAIBotController* PC = (AFortAthenaAIBotController*)Ret->Controller;
	PC->CosmeticLoadoutBC = BotData->CharacterCustomization->CustomizationLoadout;
	for (int32 i = 0; i < BotData->CharacterCustomization->CustomizationLoadout.Character->HeroDefinition->Specializations.Num(); i++)
	{
		UFortHeroSpecialization* Spec = StaticLoadObject<UFortHeroSpecialization>(Conv_NameToString(BotData->CharacterCustomization->CustomizationLoadout.Character->HeroDefinition->Specializations[i].ObjectID.AssetPathName).ToString());

		if (Spec)
		{
			for (int32 i = 0; i < Spec->CharacterParts.Num(); i++)
			{
				UCustomCharacterPart* Part = StaticLoadObject<UCustomCharacterPart>(Conv_NameToString(Spec->CharacterParts[i].ObjectID.AssetPathName).ToString());
				Ret->ServerChoosePart(Part->CharacterPartType, Part);
			}
		}
	}

	Ret->CosmeticLoadout = BotData->CharacterCustomization->CustomizationLoadout;
	Ret->OnRep_CosmeticLoadout();

	SpawnLocator->K2_DestroyActor();
	DWORD CustomSquadId = RuntimeBotData.CustomSquadId;
	BYTE TrueByte = 1;
	BYTE FalseByte = 0;
	BotManagerSetupStuffIdk(__int64(BotManager), __int64(Ret), __int64(BotData->BehaviorTree), 0, &CustomSquadId, 0, __int64(BotData->StartupInventory), __int64(BotData->BotNameSettings), 0, &FalseByte, 0, &TrueByte, RuntimeBotData);

	Bot2* bot = new Bot2(Ret);

	bot->CID = BotData->CharacterCustomization->CustomizationLoadout.Character->GetName();

	for (int32 i = 0; i < BotData->StartupInventory->Items.Num(); i++)
	{
		bool equip = true;

		if (BotData->StartupInventory->Items[i]->GetName().starts_with("WID_Athena_FloppingRabbit") || BotData->StartupInventory->Items[i]->GetName().starts_with("WID_Boss_Adventure_GH")) {
			equip = false;
		}
		bot->GiveItem(BotData->StartupInventory->Items[i], 1, equip);
		if (auto Data = Cast<UFortWeaponItemDefinition>(BotData->StartupInventory->Items[i]))
		{
			if (Data->GetAmmoWorldItemDefinition_BP() && Data->GetAmmoWorldItemDefinition_BP() != Data)
			{
				bot->GiveItem(Data->GetAmmoWorldItemDefinition_BP(), 99999);
			}
		}
	}

	if ((BotName == "BD_DangerGrape_Default" || BotName.starts_with("BD_FrenchYedoc"))) {
		auto GM = GetGameMode();
		AFortPlayerPawn* closestPawn = nullptr;
		float closestDistance = 0;
		bool distanceSet = false;
		for (int32 i = 0; i < GM->AlivePlayers.Num(); i++)
		{
			auto PPC = GM->AlivePlayers[i];
			if (PPC && PPC->MyFortPawn)
			{
				float Distance = bot->Pawn->GetDistanceTo(PPC->MyFortPawn);
				if (!distanceSet) {
					distanceSet = true;
					closestDistance = Distance;
					closestPawn = PPC->MyFortPawn;
					continue;
				}
				else {
					if (Distance < closestDistance) {
						closestDistance = Distance;
						closestPawn = PPC->MyFortPawn;
					}
				}
			}
		}

		if (closestPawn && distanceSet) {
			bot->FollowTarget = closestPawn;
			bot->CurrentTarget = closestPawn;
		}
	}

	TArray<AFortAthenaPatrolPath*> PatrolPaths;

	GetStatics()->GetAllActorsOfClass(GetWorld(), AFortAthenaPatrolPath::StaticClass(), (TArray<AActor*>*) & PatrolPaths);

	for (int i = 0; i < PatrolPaths.Num(); i++) {
		if (PatrolPaths[i]->PatrolPoints[0]->K2_GetActorLocation() == SpawnLoc) {
			bot->PC->CachedPatrollingComponent->SetPatrolPath(PatrolPaths[i]);
		}
	}

	bot->bTickEnabled = true;
	return Ret;
}

void spawnMeowscles() {
	static bool meowsclesSpawned = false;
	if (meowsclesSpawned) return;
	meowsclesSpawned = true;

	UFortAthenaAIBotCustomizationData* customization = StaticLoadObject<UFortAthenaAIBotCustomizationData>("/Game/Athena/AI/MANG/BotData/BotData_MANG_POI_HMW.BotData_MANG_POI_HMW");
	FFortAthenaAIBotRunTimeCustomizationData runtimeData{};
	runtimeData.CustomSquadId = 0;

	FVector SpawnLocs[] = {
		// First spawn location
		{
			-69728.0,
			81376.0,
			5684.0
		},
		// Second spawn location
		{
			-70912.0,
			81376.0,
			5684.0
		},
		// Third spawn location
		{
			-67696.0,
			81540.0,
			5672.0
		},
	};

	FRotator Rotation = {
		0.0,
		-179.9999f,
		0.0
	};

	int32 SpawnCount = sizeof(SpawnLocs) / sizeof(FVector);
	int32 randomIndex = static_cast<int32>(GetMath()->RandomFloatInRange(0.f, static_cast<float>(SpawnCount) - 1));

	auto Meowscles = SpawnBot(GetGameMode()->ServerBotManager, SpawnLocs[randomIndex], Rotation, customization, runtimeData);
	if (!Meowscles) {
		return;
	}
	Meowscles->SetMaxShield(400);
	Meowscles->SetShield(400);
}

