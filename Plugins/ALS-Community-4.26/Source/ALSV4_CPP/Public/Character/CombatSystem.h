// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ALSBaseCharacter.h"
#include "NiagaraFunctionLibrary.h" 
#include "NiagaraSystem.h"
#include "Components/TimelineComponent.h"  // for FTimeline
#include "Animation/AnimMontage.h"        // for UAnimMontage
#include "Curves/CurveFloat.h"            // for UCurveFloat
#include <AI/BaseEnemyAI.h>
#include "CombatSystem.generated.h"



UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class ALSV4_CPP_API UCombatSystem : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UCombatSystem();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

		
	/** All combo attack montages */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	TArray<UAnimMontage*> AttackMontages;

	/** Current combo count */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	int32 ComboCount = 0;

	UPROPERTY(EditAnywhere, Category = "Combat|Combo")
	float ComboResetDelay = 3.0f;

	FTimerHandle ComboResetTimer;

	void OnSuccessfulHit();

	/*UFUNCTION()
	void OnPlayerDamaged(AActor* DamagedActor, float Damage,
		const UDamageType* DamageType,
		AController* InstigatedBy, AActor* DamageCauser);*/

	void ExpireCombo();

	/** Helper to restart the expire?timer */
	void ResetComboTimer();

	UPROPERTY(BlueprintReadOnly, Category = "ALS")
	TObjectPtr<AALSBaseCharacter> Character = nullptr;

	void SetPlayer(TObjectPtr<AALSBaseCharacter> player);

	/** Enemies currently in attack range */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	TArray<ABaseEnemyAI*> CombatEnemies;

	/** How far out to detect enemies (world units) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float DetectionRadius = 600.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	bool Attacking = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	bool countering = false;

	void Attack();

	void Counter();

	UPROPERTY(EditAnywhere, Category = "Combat|Counter")
	UAnimMontage* CounterMontage;

	/** Scan world for enemies within DetectionRadius */
	void CheckForEnemies();

	/** Pick or update the CurrentTarget from CombatEnemies */
	void SetCurrentTarget();

	UPROPERTY(EditAnywhere, Category = "Combat|Hit")
	UAnimMontage* HitMontage;

	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ReceiveHit(const FVector& HitFromLocation);

	UPROPERTY(EditAnywhere, Category = "Combat|Hit")
	float HitKnockbackStrength = 600.f;

	/** How much upwards impulse */
	UPROPERTY(EditAnywhere, Category = "Combat|Hit")
	float HitKnockbackUpStrength = 300.f;

	bool bBeingHit = false;

	/** handle to clear bBeingHit and re-enable movement */
	FTimerHandle HitReactionTimer;

	/** called when our hit reaction is finished */
	void OnHitReactionFinished();

	UPROPERTY(EditAnywhere, Category = "Combat|Counter")
	float PlayerLookAtYawOffset = 0.f;

	/** How much to yaw the enemy on counter look-at */
	UPROPERTY(EditAnywhere, Category = "Combat|Counter")
	float EnemyLookAtYawOffset = 0.f;

private:

	UFUNCTION()
	void OnHitMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	APlayerController* CachedPC = nullptr;

	/** Which step we’re on (0…3) */
	int32 AttackIndex = 0;

	/** Holds a two-element randomized order for punches each cycle */
	int32 PunchOrder[2];
	int32 KickOrder[2];

	ABaseEnemyAI* CurrentTarget = nullptr;
	ABaseEnemyAI* TargetOnAttack = nullptr;
	ABaseEnemyAI* CounterTarget = nullptr;

	/** Called from your custom CounterAnimNotify */
	void OnCounterNotify();

	/** Bind both notifies at startup */


	/** Fill PunchOrder with {0,1} or {1,0} at the start of each combo cycle */
	void ShufflePunchOrder();
	void ShuffleKickOrder();
	void DrawGizmos() const;

	// ????????????????????????????????????????????????????????????????????????????
	//  MOVEMENT INTO TARGET
	// ???

	UPROPERTY(EditAnywhere, Category = "Combat|Movement")
	UCurveFloat* MoveCurve = nullptr;

	UPROPERTY(EditAnywhere, Category = "Combat|Movement")
	float ApproachDistance = 50.0f;

	UPROPERTY(EditAnywhere, Category = "Combat|Movement")
	float ApproachDistanceCounter = 80.0f;

	UPROPERTY(EditAnywhere, Category = "Combat|Movement")
	float CounterSeparationDistance = 80.0f;

	/** Vertical lift above the enemy’s origin (e.g. to match foot height) */
	UPROPERTY(EditAnywhere, Category = "Combat|Movement")
	float ApproachHeightOffset = 0.f;

	/** Offset from the target’s origin where you want the character to end up */
	UPROPERTY(EditAnywhere, Category = "Combat|Movement")
	FVector TargetOffset = FVector(-75.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, Category = "Combat|Movement")
	float MinMoveDistance = 50.f;

	/** Timeline to drive the interpolation */
	FTimeline MoveTimeline;

	/** Recorded start/end positions for the current lunge */
	FVector MoveStart;
	FVector MoveEnd;

	/** Called each tick of MoveTimeline with a normalized 0?1 alpha */
	UFUNCTION()
	void OnMoveTimelineUpdate(float Alpha);

	/** Prepares and kicks off MoveTimeline for the given duration (seconds) */
	void SmoothTransitionToCurrentTarget(float Duration);

	static float GetMoveCompleteTime(UAnimMontage* Montage);

	void InitAnimNotifies();
	void OnAttackLanded();
	void OnCounterAttackLanded();


	///
	//Camera Offset
	///

	// camera?offset curve (0?1)
	UPROPERTY(EditAnywhere, Category = "Combat|Camera")
	UCurveFloat* CameraOffsetCurve = nullptr;

	// timeline to drive that offset
	FTimeline CameraOffsetTimeline;

	// default vs combat offsets
	UPROPERTY(EditAnywhere, Category = "Combat|Camera")
	FVector DefaultCameraOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Combat|Camera")
	FVector CombatCameraOffset = FVector(-200.f, 0.f, 75.f);

	// callback for timeline
	UFUNCTION()
	void OnCameraOffsetUpdate(float Alpha);

};
