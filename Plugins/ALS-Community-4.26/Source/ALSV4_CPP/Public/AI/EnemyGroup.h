// EnemyGroup.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AI/EnemyGroupController.h"
#include "Components/SplineComponent.h"
#include "Components/BillboardComponent.h"
#include "EnemyGroup.generated.h"

class ABaseEnemyAI;

UCLASS()
class ALSV4_CPP_API AEnemyGroup : public AActor
{
	GENERATED_BODY()

public:
	AEnemyGroup();

	virtual void OnConstruction(const FTransform& Transform) override;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** All enemies in this group */
	UPROPERTY(EditAnywhere, Category = "Group")
	TArray<ABaseEnemyAI*> Members;

	UPROPERTY(VisibleAnywhere, Category = "Debug")
	USplineComponent* GroupCircleSpline;

#if WITH_EDITORONLY_DATA
	/** editor‐only gizmo at the group center */
	UPROPERTY()
	UBillboardComponent* CenterGizmo;
#endif

	void UpdateDebugShape();

	/** When true, this group is actively engaging the player */
	UPROPERTY(BlueprintReadOnly, Category = "Group")
	bool bIsEngaged = false;

	/** Called by the controller to mark this group engaged */
	void Engage();

	/** Called by the controller to mark this group disengaged */
	void Disengage();

	/** Which member is currently attacking (index into Members) */
	int32 CurrentAttackerIndex = 0;

	UPROPERTY()
	AEnemyGroupController* OwningController = nullptr;

	UPROPERTY(EditAnywhere, Category = "Group")
	float FormationRadius = 600.f;  // how far from the player to stand

	/** How far the player must move before the formation “snaps” to the new center */
	UPROPERTY(EditAnywhere, Category = "Combat|Formation")
	float CenterMoveThreshold = 250.0f;

	/** Cached center used to gate updates */
	FVector LastFormationCenter;

	/** Call any time Members.Num() changes to rebuild the evenly‐spaced offsets. */
	void RebuildFormationOffsets();

	void HandleMemberRecovered(ABaseEnemyAI* Enemy);

	ABaseEnemyAI* CurrentAttacker = nullptr;

	/** Kick off the next attack cycle */
	void RequestNextAttacker();

	UPROPERTY(EditAnywhere, Category = "Idle", meta = (ClampMin = "0"))
	float IdleRotationSpeedMin = 5.0f;

	UPROPERTY(EditAnywhere, Category = "Idle", meta = (ClampMin = "0"))
	float IdleRotationSpeedMax = 15.0f;

	/** How often (seconds) two random members swap slots when idle */
	UPROPERTY(EditAnywhere, Category = "Idle", meta = (ClampMin = "0"))
	float MinIdleSwapInterval = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Idle", meta = (ClampMin = "0"))
	float MaxIdleSwapInterval = 8.0f;

	/** Angles (in radians) around the circle for each slot, maintained at runtime */
	UPROPERTY(Transient)
	TArray<float> FormationAngles;

	TArray<float> IdleSpeeds;

	/** Timer handle for scheduling idle swaps */
	FTimerHandle IdleSwapTimer;

	void DoIdleSwap();

	TArray<float> IdlePulsePhases;
	UPROPERTY(EditAnywhere, Category = "Idle")
	float IdlePulseSpeed = 5.0f;
	UPROPERTY(EditAnywhere, Category = "Idle")
	float IdlePulseAmount = 300.f;

	UPROPERTY(EditAnywhere, Category = "Idle", meta = (ClampMin = "0"))
	float MaxIdleStartDelay = 2.0f;        // max seconds to wait before each one starts

	TArray<float> IdleStartDelays;         // per‐enemy random offset

	// how long an enemy swirls when “active”
	UPROPERTY(EditAnywhere, Category = "Idle", meta = (ClampMin = "0"))
	float MinIdleActiveDuration = 3.0f;
	UPROPERTY(EditAnywhere, Category = "Idle", meta = (ClampMin = "0"))
	float MaxIdleActiveDuration = 6.0f;

	/** How long (seconds) an enemy spends paused between swirls */
	UPROPERTY(EditAnywhere, Category = "Idle", meta = (ClampMin = "0"))
	float MinIdleInactiveDuration = 2.0f;
	UPROPERTY(EditAnywhere, Category = "Idle", meta = (ClampMin = "0"))
	float MaxIdleInactiveDuration = 5.0f;

protected:
	/** Radius around player that triggers engagement */
	UPROPERTY(EditAnywhere, Category = "Group")
	float EngagementRadius = 1500.f;

	/** How fast members move into formation around the player */
	UPROPERTY(EditAnywhere, Category = "Group")
	float FormationInterpSpeed = 5.f;

	/** Patterns of offsets around the player for positioning */
	UPROPERTY(EditAnywhere, Category = "Group")
	TArray<FVector> FormationOffsets;

	/** Time between each member’s turn to attack */
	UPROPERTY(EditAnywhere, Category = "Group")
	float AttackTurnInterval = 3.f;

private:

	/** Called whenever we enter bIsEngaged=true */
	void OnEngaged();

	float IdleTime = 0.f;
	TArray<float> IdleTimeOffsets;

	/** Called whenever we go back to bIsEngaged=false */
	void OnDisengaged();

	/** Move all Members into their assigned formation slots */
	void UpdateFormation(float DeltaTime);

	FTimerHandle AttackTurnTimer;

	UFUNCTION()
	void OnMemberFinishedAttack(ABaseEnemyAI* Enemy);

	TArray<bool>  IdleIsSwirling;
	TArray<float> IdleTimers;
	TArray<float> IdleSwirlDurations;
	TArray<float> IdlePauseDurations;
};
