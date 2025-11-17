// EnemyGroupController.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyGroupController.generated.h"

class ABaseEnemyAI;
class AEnemyGroup;

UCLASS()
class ALSV4_CPP_API AEnemyGroupController : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values
	AEnemyGroupController();

	// Called at begin play to find all AEnemyGroup actors in the level
	virtual void BeginPlay() override;

	// Called every frame to check if any groups should merge
	virtual void Tick(float DeltaTime) override;

	// All groups currently managed
	UPROPERTY(EditAnywhere, Category = "Enemies")
	TArray<AEnemyGroup*> Groups;

	/** Called by any group when it “engages” the player */
	void NotifyGroupEngaged(AEnemyGroup* EngagedGroup);

	/** Whenever two groups both have bIsEngaged == true and are near, merge them */
	void MergeGroupsIfNeeded();

	/** Helper to merge B into A (reparent all enemies) */
	void MergeGroupInto(AEnemyGroup* A, AEnemyGroup* B);
};
