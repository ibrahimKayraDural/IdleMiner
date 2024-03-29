// Fill out your copyright notice in the Description page of Project Settings.


#include "GameManager.h"
#include "Kismet/GameplayStatics.h"
#include "Building_PlayerController.h"

AGameManager* AGameManager::Instance = nullptr;

// Sets default values
AGameManager::AGameManager() : 
	WasM0Pressed(false)
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	BuildingPreviewMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BuildingPreview"));
	BuildingPreviewMesh->SetupAttachment(RootComponent);
	BuildingPreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultSlotMesh(TEXT("/Engine/BasicShapes/Plane"));

	GridIndicatorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GridIndicator"));
	GridIndicatorMesh->SetupAttachment(RootComponent);
	GridIndicatorMesh->SetStaticMesh(DefaultSlotMesh.Object);
	GridIndicatorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Instance = this;

	//OldGameSlot = nullptr;
}

// Called when the game starts or when spawned
void AGameManager::BeginPlay()
{
	Super::BeginPlay();

	DrillIndex = 0;
	FactoryIndex = 0;
	StoreIndex = 0;

	if (Drills.Num() > 0)
	{
		CurrentBuilding = Drills[0];

		UStaticMesh* tempMesh = CurrentBuilding.GetDefaultObject()->Mesh->GetStaticMesh();

		BuildingPreviewMesh->SetStaticMesh(tempMesh);
		for (int i = 0; i < BuildingPreviewMesh->GetNumMaterials(); i++)
		{
			BuildingPreviewMesh->SetMaterial(i, GhostMaterial);
		}
	}
	else if (Factories.Num() > 0)
	{
		CurrentBuilding = Factories[0];

		UStaticMesh* tempMesh = CurrentBuilding.GetDefaultObject()->Mesh->GetStaticMesh();

		BuildingPreviewMesh->SetStaticMesh(tempMesh);
		BuildingPreviewMesh->SetMaterial(0, GhostMaterial);
	}
	else if (Stores.Num() > 0)
	{
		CurrentBuilding = Stores[0];

		UStaticMesh* tempMesh = CurrentBuilding.GetDefaultObject()->Mesh->GetStaticMesh();

		BuildingPreviewMesh->SetStaticMesh(tempMesh);
		BuildingPreviewMesh->SetMaterial(0, GhostMaterial);
	}
	else CurrentBuilding = nullptr;

	GridIndicatorMesh->SetMaterial(0, GhostTileMaterial);

	GatherResources();

	FindEnvironmentalBuildings();

	RefreshUI((int)CurrentBuilding.GetDefaultObject()->Type.GetIntValue());
}

// Called every frame
void AGameManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AGameManager::OnConstruction(const FTransform& Transform)
{
	if (button_FindEnvironmentalBuildings)
	{
		button_FindEnvironmentalBuildings = false;

		FindEnvironmentalBuildings();
	}
}

void AGameManager::FindEnvironmentalBuildings()
{
	TSubclassOf<ABuildingBase> ClassToFind = ABuildingBase::StaticClass();

	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ClassToFind, FoundActors);

	EnvironmentBuildings = TMap<FSGridPosition, FSPlacedBuilding>();
	PlacedBuildings = TMap<FSGridPosition, FSPlacedBuilding>();

	for (AActor* actor : FoundActors)
	{
		FVector TargetLocation = actor->GetActorLocation();
		TargetLocation.Z = GetActorLocation().Z;

		FVector SnappedLocation = TargetLocation;
		SnappedLocation.X = FMath::GridSnap(SnappedLocation.X, GridSize);
		SnappedLocation.Y = FMath::GridSnap(SnappedLocation.Y, GridSize);

		FSGridPosition gridPos = FSGridPosition::
			GetPositionInGrid(GetActorLocation(), SnappedLocation, GridSize);

		ABuildingBase* building = (ABuildingBase*)actor;

		if (building->GetClass() != BaseBuildingClass)
		{
			if (EnvironmentBuildings.Contains(gridPos))
			{
				actor->Destroy();
				continue;
			}

			building->SetActorLocation(SnappedLocation);
			EnvironmentBuildings.Add(gridPos, FSPlacedBuilding(building, gridPos));
		}
		else
		{
			if (PlacedBuildings.Contains(gridPos))
			{
				actor->Destroy();
				continue;
			}

			building->SetActorLocation(SnappedLocation);
			PlacedBuildings.Add(gridPos, FSPlacedBuilding(building, gridPos));
		}
		
	}
}

void AGameManager::RefreshResourceCountsHelper()
{
	TArray<TEnumAsByte<EResource>> types;
	TArray<int> counts;
	ResourceCounts.GenerateKeyArray(types);
	ResourceCounts.GenerateValueArray(counts);
	RefreshResourceCounts(types, counts);
}

void AGameManager::SendMouseTrace(AActor* HitActor, FVector& Location, bool IsPressed)
{
	FVector TargetLocation = FVector(Location.X, Location.Y, GetActorLocation().Z);

	FVector SnappedLocation = TargetLocation;
	SnappedLocation.X = FMath::GridSnap(SnappedLocation.X, GridSize);
	SnappedLocation.Y = FMath::GridSnap(SnappedLocation.Y, GridSize);

	if (CurrentBuilding != nullptr)
	{
		BuildingPreviewMesh->SetWorldLocation(SnappedLocation);
	}
	GridIndicatorMesh->SetWorldLocation(SnappedLocation);

	if (IsPressed && WasM0Pressed == false)
	{
		DeselectBuilding();

		FSGridPosition gridPos = FSGridPosition::GetPositionInGrid(GetActorLocation(), SnappedLocation, GridSize);

		bool canPlace = true;
		bool replace = false;
		TSubclassOf<ABuildingBase> OccupyingBuildingClass = nullptr;

		if (PlacedBuildings.Contains(gridPos))
		{
			canPlace = false;
		}
		else if (EnvironmentBuildings.Contains(gridPos))
		{
			OccupyingBuildingClass = EnvironmentBuildings[gridPos].Building->GetClass();
		}

		if (CurrentBuilding.GetDefaultObject()->BuildingToPlaceOver != OccupyingBuildingClass)
		{
			canPlace = false;
		}
		else if (OccupyingBuildingClass != nullptr)
		{
			replace = true;
		}

		if (canPlace)//check build cost if still can place
		{
			for (FSBuildingProcess process : CurrentBuilding.GetDefaultObject()->BuildCosts)
			{
				if (DoesHaveResources(process) == false)
				{
					canPlace = false;
					break;
				}
			}
		}

		//GEngine->AddOnScreenDebugMessage(-1, 1, FColor::White, FString::Printf(TEXT(""), );

		if (canPlace)
		{
			for (FSBuildingProcess process : CurrentBuilding.GetDefaultObject()->BuildCosts)
			{
				TryRemoveResources(process);
			}

			FActorSpawnParameters SpawnInfo;
			FVector SpawnLocation = SnappedLocation;
			FRotator SpawnRotation = FRotator(0, 0, 0);

			ABuildingBase* SpawnedRef = GetWorld()->SpawnActor<ABuildingBase>(CurrentBuilding, SpawnLocation, SpawnRotation, SpawnInfo);

			if (replace)
			{
				EnvironmentBuildings[gridPos].Building->SetActorHidden(EnvironmentBuildings[gridPos].Building->DisableOnReplaced);
			}

			PlacedBuildings.Add(gridPos, FSPlacedBuilding(SpawnedRef, gridPos));

			RefreshResourceCountsHelper();
		}
		else if (PlacedBuildings.Contains(gridPos))
		{
			SelectBuilding(PlacedBuildings[gridPos]);

			FString debug = PlacedBuildings[gridPos].Building->IsUpgraded ? TEXT("true") : TEXT("false");
			GEngine->AddOnScreenDebugMessage(-1, 1, FColor::White, debug);
		}
	}

	WasM0Pressed = IsPressed;

	//DrawDebugPoint(GetWorld(), Location, 100, FColor::Red);
}

void AGameManager::GatherResources()
{
	/*int CoinGain = PlacedBuildings.Num();
	Count_Coin += CoinGain;*/

	TArray<FSPlacedBuilding> buildings;
	PlacedBuildings.GenerateValueArray(buildings);

	for (FSPlacedBuilding placedB : buildings)
	{
		ABuildingBase* building = placedB.Building;

		bool doesHaveAllResources = true;
		for (FSBuildingProcess process : building->NeedsPerClock)
		{
			if (DoesHaveResources(process) == false)
			{
				doesHaveAllResources = false;
				break;
			}
		}

		if (doesHaveAllResources == false) continue;

		for (FSBuildingProcess process : building->NeedsPerClock)
		{
			TryRemoveResources(process);
		}

		for (FSBuildingProcess process : building->GainsPerClock)
		{
			AddResources(process);
		}
	}

	RefreshResourceCountsHelper();

	GEngine->AddOnScreenDebugMessage(-1, .8f, FColor::White, TEXT("Gathering resources..."));

	GWorld->GetTimerManager().SetTimer(GatherHandle, this, &AGameManager::GatherResources, ResourceGatherSpeed, false);
}

bool AGameManager::DoesHaveResources(FSBuildingProcess process)
{
	if (process.Type == EResource::R_None) return true;
	if (ResourceCounts.Contains(process.Type) == false) return false;

	return ResourceCounts[process.Type] >= process.Count;
}

bool AGameManager::TryRemoveResources(FSBuildingProcess process)
{
	if (process.Type == EResource::R_None) return true;
	if (DoesHaveResources(process) == false) return false;

	ResourceCounts[process.Type] -= process.Count;
	return true;
}

void AGameManager::DeleteSelectedBuilding()
{
	if (SelectedBuilding.Building == nullptr) return;
	if (SelectedBuilding.Building->IsNotDeletable == true) return;
	if (PlacedBuildings.Contains(SelectedBuilding.Position) == false) return;

	PlacedBuildings.Remove(SelectedBuilding.Position);
	SelectedBuilding.Building->Destroy();

	DeselectBuilding();
}

void AGameManager::UpgradeSelectedBuilding()
{
	if (SelectedBuilding.Building == nullptr) return;
	if (SelectedBuilding.Building->IsUpgraded == true) return;
	if (DoesHaveResources(SelectedBuilding.Building->UpgradeCost) == false) return;

	TryRemoveResources(SelectedBuilding.Building->UpgradeCost);

	SelectedBuilding.Building->Upgrade();

	DeselectBuilding();
}

void AGameManager::SelectBuilding(FSPlacedBuilding buildingToSelect)
{
	SelectedBuilding = buildingToSelect;
	RefreshSelectionMenu(SelectedBuilding.Building->UpgradeCost.Count, SelectedBuilding.Building->IsUpgraded);
}

void AGameManager::DeselectBuilding()
{
	SelectedBuilding = FSPlacedBuilding(nullptr, 0, 0);
	RefreshSelectionMenu(-1, false);
}

void AGameManager::AddResources(FSBuildingProcess process)
{
	if (ResourceCounts.Contains(process.Type))
	{
		ResourceCounts[process.Type] += process.Count;
	}
}

void AGameManager::ChangeSelectedBuilding(EBuilding building)
{
	if (building == B_DrillBasic || building == B_DrillAdvanced || building == B_DrillExceptional)
	{
		if (Drills.Num() <= 0) return;
		DrillIndex = (DrillIndex + 1) % Drills.Num();
		CurrentBuilding = Drills[DrillIndex];
	}
	else if (building == B_FactoryCopperWire || building == B_FactoryIronPan || building == B_FactoryGoldNecklace)
	{
		if (Factories.Num() <= 0) return;
		FactoryIndex = (FactoryIndex + 1) % Factories.Num();
		CurrentBuilding = Factories[FactoryIndex];
	}
	else if(building == B_StoreHardware || building == B_StoreUtensil || building == B_StoreJewelery)
	{
		if (Stores.Num() <= 0) return;
		StoreIndex = (StoreIndex + 1) % Stores.Num();
		CurrentBuilding = Stores[StoreIndex];
	}

	RefreshUI((int)CurrentBuilding.GetDefaultObject()->Type.GetIntValue());

	UStaticMesh* tempMesh = CurrentBuilding.GetDefaultObject()->Mesh->GetStaticMesh();

	BuildingPreviewMesh->SetStaticMesh(tempMesh);
	for (int i = 0; i < BuildingPreviewMesh->GetNumMaterials(); i++)
	{
		BuildingPreviewMesh->SetMaterial(i, GhostMaterial);
	}
}