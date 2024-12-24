#pragma once
#include "ue.h"
#include "PlayerState.h"
#include "PE.h"


float EvaluateCurveTableRow(UCurveTable* CurveTable, FName RowName, float InXY,
	const FString& ContextString = FString(), EEvaluateCurveTableResult* OutResult = nullptr)
{
	static auto fn = StaticLoadObject<UFunction>("/Script/Engine.DataTableFunctionLibrary.EvaluateCurveTableRow");

	float wtf{};
	EEvaluateCurveTableResult wtf1{};

	struct { UCurveTable* CurveTable; FName RowName; float InXY; EEvaluateCurveTableResult OutResult; float OutXY; FString ContextString; }
	UDataTableFunctionLibrary_EvaluateCurveTableRow_Params{ CurveTable, RowName, InXY, wtf1, wtf, ContextString };

	static auto DefaultClass = UDataTableFunctionLibrary::StaticClass();
	DefaultClass->ProcessEvent(fn, &UDataTableFunctionLibrary_EvaluateCurveTableRow_Params);

	if (OutResult)
		*OutResult = UDataTableFunctionLibrary_EvaluateCurveTableRow_Params.OutResult;

	return UDataTableFunctionLibrary_EvaluateCurveTableRow_Params.OutXY;


	struct FCurveTableRowHandle RH {
		CurveTable, RowName
	};
	return GetFortKismet()->EvaluateCurveTableRow(RH, InXY, &wtf, ContextString);
}


FVector PickSupplyDropLocation(AFortAthenaMapInfo* MapInfo, FVector Center, float Radius)
{
	static FVector* (*PickSupplyDropLocationOriginal)(AFortAthenaMapInfo * MapInfo, FVector * outLocation, __int64 Center, float Radius) = decltype(PickSupplyDropLocationOriginal)(__int64(GetModuleHandleA(0)) + 0x18848f0);

	if (!PickSupplyDropLocationOriginal)
		return FVector(0, 0, 0);

	FVector Out = FVector(0, 0, 0);
	auto ahh = PickSupplyDropLocationOriginal(MapInfo, &Out, __int64(&Center), Radius);
	return Out;
}

static inline int CalcuateCurveMinAndMax(FScalableFloat Min, FScalableFloat Max, float Multiplier = 100.f) // returns 000 not 0.00 (forgot techinal name for this)
{
	float MinSpawnPercent = EvaluateCurveTableRow(Min.Curve.CurveTable, Min.Curve.RowName, 0);
	float MaxSpawnPercent = EvaluateCurveTableRow(Max.Curve.CurveTable, Max.Curve.RowName, 0);
}


void SpawnLlamas()
{
	auto MapInfo = GetGameState()->MapInfo;
	int AmountOfLlamasSpawned = 0;
	auto AmountOfLlamasToSpawn = 5;


	for (int i = 0; i < AmountOfLlamasToSpawn; i++)
	{
		int Radius = 100000;
		FVector Location = PickSupplyDropLocation(MapInfo, FVector(1, 1, 10000), (float)Radius);

		FRotator RandomYawRotator{};
		RandomYawRotator.Yaw = (float)rand() * 0.010986663f;

		FTransform InitialSpawnTransform{};
		InitialSpawnTransform.Translation = Location;
		InitialSpawnTransform.Rotation = FRotToQuat(RandomYawRotator);
		InitialSpawnTransform.Scale3D = FVector(1, 1, 1);

		auto LlamaStart = SpawnActor<AFortAthenaSupplyDrop>(MapInfo->LlamaClass.Get(), Location, RandomYawRotator);

		if (!LlamaStart)
			continue;

		auto GroundLocation = LlamaStart->FindGroundLocationAt(InitialSpawnTransform.Translation);

		LlamaStart->K2_DestroyActor();
		auto Llama = SpawnActor<AFortAthenaSupplyDrop>(MapInfo->LlamaClass.Get(), GroundLocation, RandomYawRotator);

		Llama->bCanBeDamaged = false;

		if (!Llama)
			continue;
		AmountOfLlamasSpawned++;
	}
}