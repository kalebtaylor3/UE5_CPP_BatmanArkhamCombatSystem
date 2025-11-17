// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/ALSCharacter.h"
#include "EnemyGroup.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "AIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BrainComponent.h"
#include "BaseEnemyAI.generated.h"

/**
 * 
 */

//DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGotHit);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAttackComplete, ABaseEnemyAI*, Enemy);

UCLASS()
class ALSV4_CPP_API ABaseEnemyAI : public AALSCharacter
{
	GENERATED_BODY()
	
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	TArray<UAnimMontage*> AttackMontages;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	UAnimMontage* HitMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Hit")
	float KnockbackStrength = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Hit")
	float RagdollDuration = 3.f;

	/** How much Z impulse on hit */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Hit")
	float KnockbackUpStrength = 150.f;

	void GetHit(const FVector& HitFromLocation, int32 PlayerComboCount);
	void Recover();

	UPROPERTY(BlueprintReadOnly, Category = "AI")
	AEnemyGroup* OwningGroup = nullptr;

	bool bAttacking = false;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Counter")
	UParticleSystem* CounterEffectTemplate;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|Counter", meta = (ClampMin = "0.1"))
	FVector CounterEffectScale = FVector(1.0f, 1.0f, 1.0f);

	/** Called by group to move into a given world position */
	void MoveToPosition(const FVector& TargetPos);

	/** Called by group when it’s this enemy’s turn to attack */
	void PerformAttackTurn();

	void EndRagDoll();

	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bCanMove = true;

	/** How close we need to get before we actually execute our attack */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attack")
	float AttackDistance = 80.0f;

	UPROPERTY(BlueprintAssignable, Category = "Combat|Attack")
	FOnAttackComplete OnAttackComplete;

	/** Called by the group controller to start an approach+attack cycle */
	void StartApproachAttack();

	UFUNCTION()
	void HandleAttackCompleteNotify();

	void NotifyRagdollFinished();

	void InitAnimNotifies();

	void AttackLanded();

	bool bIsApproaching = false;
	bool    bMovingToTarget = false;
	/** Returns true if right now the player can counter this approach */
	bool CanPlayerCounter() const;

	UPROPERTY(EditAnywhere, Category = "Combat|Counter")
	UAnimMontage* BeingCounteredMontage;

	UFUNCTION(BlueprintCallable, Category = "AI")
	void StopAILogic(const FString& Reason = TEXT("Stopped by ABaseEnemyAI"));

protected:
	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;

private:

	void CacheAIRefs();

	UPROPERTY(Transient)
	AAIController* CachedAIController = nullptr;

	UPROPERTY(Transient)
	UBrainComponent* BrainComp = nullptr;

	UPROPERTY(Transient)
	UBehaviorTreeComponent* BTComp = nullptr;

	bool bAILogicStopped = false;

	// “manual” move‐to‐target state
	FVector MoveTarget = FVector::ZeroVector;
	float   AcceptanceRadius = 25.f;  // how close is “close enough”

	int32 HitTimes = 0;

	UPROPERTY(EditAnywhere, Category = "Combat|Hit")
	float HitResetDelay = 2.f;

	FTimerHandle HitResetTimerHandle;

	void ResetHitTimes();

	FVector PendingImpulse;

	/** Called one frame after RagdollStart to actually apply the impulse */
	void ApplyDeferredRagdollImpulse();

	static const FName PelvisBoneName;

	/** Timer handle for ending ragdoll */
	FTimerHandle RagdollEndTimerHandle;

	/** Timer handle for ending ragdoll */
	FTimerHandle RecoveryTimer;

	UPROPERTY()
	UParticleSystemComponent* CounterEffectComponent = nullptr;

};
