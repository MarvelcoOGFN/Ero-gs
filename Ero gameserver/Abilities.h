#pragma once
#include "ue.h"
#include "Inventory.h"

void GiveAbility(AFortPlayerControllerAthena* PC, UClass* Ability)
{
    if (!PC || !PC->PlayerState || !PC->MyFortPawn) { return; }
    auto AbilitySystemComponent = PC->MyFortPawn->AbilitySystemComponent;

    if (!AbilitySystemComponent)
        return;

    if (!Ability)
        return;

    FGameplayAbilitySpec Spec{};
    FGameplayAbilitySpecCtor(&Spec, (UGameplayAbility*)Ability->DefaultObject, 1, -1, nullptr);
    GiveAbilityOG(AbilitySystemComponent, &Spec.Handle, Spec);
}

FGameplayAbilitySpec* FindAbilitySpecFromHandle(UAbilitySystemComponent* Component, FGameplayAbilitySpecHandle& Handle)
{
    for (int32 i = 0; i < Component->ActivatableAbilities.Items.Num(); i++)
    {
        if (Component->ActivatableAbilities.Items[i].Handle.Handle == Handle.Handle) { return &Component->ActivatableAbilities.Items[i]; }
    }

    return nullptr;
}

std::map<AFortPlayerController*, int> Shots{};
std::map<AFortPlayerController*, UFortWeaponItemDefinition*> Weapons{};
int LastTime = 0;

int64_t(*ApplyCostOG)(UGameplayAbility* arg1, int32_t arg2, void* arg3, void * arg4);
int64_t ApplyCost(UFortGameplayAbility* arg1, int32_t arg2, void* arg3, void * arg4) {
    if (arg1->GetName().starts_with("GA_Athena_AppleSun_Passive_C_")) {
        auto Def = StaticLoadObject<UFortItemDefinition>("/Game/Athena/Items/Consumables/AppleSun/WID_Athena_AppleSun.WID_Athena_AppleSun");
        auto ASC = arg1->GetActivatingAbilityComponent();
        AFortPlayerStateAthena* PS = (AFortPlayerStateAthena*)ASC->GetOwner();
        auto Pawn = PS->GetCurrentPawn();
        AFortPlayerController* PC = nullptr;
        PC = (AFortPlayerController*)Pawn->GetOwner();

        if (!PC->bInfiniteAmmo) {
            Remove(PC, Def);
        }
    }
    else if (arg1->GetName().starts_with("GA_Ranged_") || arg1->GetName().starts_with("GAB_Melee_ImpactCombo_Athena_")) {
        auto ASC = arg1->GetActivatingAbilityComponent();
        AFortPlayerStateAthena* PS = (AFortPlayerStateAthena*)ASC->GetOwner();
        auto Pawn = PS->GetCurrentPawn();
        AFortPlayerController* PC = nullptr;
        PC = (AFortPlayerController*)Pawn->GetOwner();
        if (PC->IsA(ABP_PhoebePlayerController_C::StaticClass())) {
            return ApplyCostOG(arg1, arg2, arg3, arg4);
        }
        auto t = std::floor(GetStatics()->GetTimeSeconds(GetWorld()));
        if (LastTime != (int)t) {
            LastTime = (int)t;
            Shots[PC] = 0;
        }
        if (Weapons[PC] != Pawn->CurrentWeapon->WeaponData) {
            Shots[PC] = 0;
            Weapons[PC] = Pawn->CurrentWeapon->WeaponData;
        }

        auto WSH = Weapons[PC]->WeaponStatHandle;
    }
    return ApplyCostOG(arg1, arg2, arg3, arg4);
}

void (*InternalServerTryActivateAbilityOG)(UFortAbilitySystemComponentAthena* AbilitySystemComponent, FGameplayAbilitySpecHandle Handle, bool InputPressed, FPredictionKey& PredictionKey, FGameplayEventData* TriggerEventData);
void InternalServerTryActivateAbilityHook(UFortAbilitySystemComponentAthena* AbilitySystemComponent, FGameplayAbilitySpecHandle Handle, bool InputPressed, FPredictionKey& PredictionKey, FGameplayEventData* TriggerEventData)
{
    FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(AbilitySystemComponent, Handle);
    if (!Spec)
        return AbilitySystemComponent->ClientActivateAbilityFailed(Handle, PredictionKey.Current);

    // Consume any pending target info, to clear out cancels from old executions
    //ConsumeAllReplicatedData(Handle, PredictionKey);

    UGameplayAbility* AbilityToActivate = Spec->Ability;

    UGameplayAbility* InstancedAbility = nullptr;
    Spec->InputPressed = true;

    // Attempt to activate the ability (server side) and tell the client if it succeeded or failed.
    AFortPlayerStateAthena* PS = (AFortPlayerStateAthena*)AbilitySystemComponent->GetOwner();
    auto Pawn = PS->GetCurrentPawn();
    AFortPlayerController* PC = nullptr;
    if (!Pawn) goto activate;
    PC = (AFortPlayerController*)Pawn->GetOwner();
    if (Spec->Ability->GetName() == "Default__GAB_InterrogatePlayer_Reveal_C") {
        if ((Pawn->GetHealth() + 20.f) >= 100) {
            auto NewShield = (Pawn->GetShield() + 20.f) >= 100 ? 100 : (Pawn->GetShield() + 20.f);
            Pawn->SetShield(NewShield);
        }
        else {
            auto NewHealth = Pawn->GetHealth() + 20.f;
            Pawn->SetHealth(NewHealth);
        }
    }
    else if (Spec->Ability->GetName() == "Default__GAT_Athena_c4_Detonate_C") {
        auto Def = StaticLoadObject<UFortItemDefinition>("/Game/Athena/Items/Consumables/C4/Athena_C4.Athena_C4");
        float MaxStackSize = GetMaxStackSize(Def);
        FFortItemEntry* FoundEntry = nullptr;

        for (int32 i = 0; i < PC->WorldInventory->Inventory.ReplicatedEntries.Num(); i++)
        {
            FFortItemEntry& Entry = PC->WorldInventory->Inventory.ReplicatedEntries[i];

            if (Entry.ItemDefinition == Def && (Entry.Count < MaxStackSize))
            {
                FoundEntry = &PC->WorldInventory->Inventory.ReplicatedEntries[i];
            }
        }

        if (FoundEntry && FoundEntry->Count == 0) {
            Remove(PC, Def);
        }
    }
activate:
    if (!InternalTryActivateAbility(AbilitySystemComponent, Handle, PredictionKey, &InstancedAbility, nullptr, TriggerEventData))
    {
        AbilitySystemComponent->ClientActivateAbilityFailed(Handle, PredictionKey.Current);
        Spec->InputPressed = false;
        AbilitySystemComponent->ActivatableAbilities.MarkItemDirty(*Spec);
    }
}

void GiveAbilitySet(AFortPlayerControllerAthena* PC, UFortAbilitySet* Set)
{
    if (Set)
    {
        for (int32 i = 0; i < Set->GameplayAbilities.Num(); i++)
        {
            UClass* Ability = Set->GameplayAbilities[i].Get();
            if (Ability) { GiveAbility(PC, Ability); }
        }
    }
}

void RemoveAbility(AFortPlayerController* PC, UClass* AbilityClass)
{
    for (int32  i = 0; i < PC->MyFortPawn->AbilitySystemComponent->ActivatableAbilities.Items.Num(); i++)
    {
        if (PC->MyFortPawn->AbilitySystemComponent->ActivatableAbilities.Items[i].Ability->Class == AbilityClass)
        {
            auto& Item = PC->MyFortPawn->AbilitySystemComponent->ActivatableAbilities.Items[i];
            PC->MyFortPawn->AbilitySystemComponent->ServerEndAbility(Item.Handle, Item.ActivationInfo, Item.ActivationInfo.PredictionKeyWhenActivated);
            PC->MyFortPawn->AbilitySystemComponent->ServerCancelAbility(Item.Handle, Item.ActivationInfo);
            PC->MyFortPawn->AbilitySystemComponent->ClientCancelAbility(Item.Handle, Item.ActivationInfo);
            break;
        }
    }
}