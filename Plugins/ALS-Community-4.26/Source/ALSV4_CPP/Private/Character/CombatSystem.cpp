// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/CombatSystem.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Controller.h"
#include <Character/AttackLandAnimNotify.h>
#include <Character/ALSPlayerCameraManager.h>
#include "GameFramework/CharacterMovementComponent.h"
#include "Character/Animation/Notify/CounterAnimNotify.h"
#include "Character/Animation/Notify/CounterLandAnimNotify.h"

// Sets default values for this component's properties
UCombatSystem::UCombatSystem()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UCombatSystem::BeginPlay()
{
	Super::BeginPlay();
    InitAnimNotifies();
	// ...
    if (MoveCurve)
    {
        // create delegates
        FOnTimelineFloat OnUpdate;
        OnUpdate.BindUFunction(this, TEXT("OnMoveTimelineUpdate"));

        // bind them into the timeline
        MoveTimeline.AddInterpFloat(MoveCurve, OnUpdate);
        MoveTimeline.SetLooping(false);
    }

    if (CameraOffsetCurve)
    {
        FOnTimelineFloat Update;
        Update.BindUFunction(this, TEXT("OnCameraOffsetUpdate"));
        CameraOffsetTimeline.AddInterpFloat(CameraOffsetCurve, Update);
        CameraOffsetTimeline.SetLooping(false);
    }

    Character->SetDesiredRotationMode(EALSRotationMode::VelocityDirection);

    if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
    {
        if (AALSPlayerCameraManager* CamMgr =
            Cast<AALSPlayerCameraManager>(PC->PlayerCameraManager))
        {
            CamMgr->ManualCameraOffset = DefaultCameraOffset;
        }
    }

    if (Character)
    {
        CachedPC = Cast<APlayerController>(Character->GetController());
    }
}


// Called every frame
void UCombatSystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// ...

    CheckForEnemies();

    //handle if in combat or not
    if (Character)
    {
        const bool bHasEnemies = CombatEnemies.Num() > 0;
        const EALSOverlayState CurrentState = Character->GetOverlayState();

        if (bHasEnemies && CurrentState != EALSOverlayState::Combat && !bBeingHit)
        {
            Character->SetOverlayState(EALSOverlayState::Combat);
            Character->LookingDirectionAction();
            CameraOffsetTimeline.PlayFromStart();
        }
        else if (!bHasEnemies && CurrentState != EALSOverlayState::Default)
        {
            Character->SetOverlayState(EALSOverlayState::Default);
            Character->VelocityDirectionAction();
            CameraOffsetTimeline.ReverseFromEnd();
        }

    }

	//DrawGizmos();
    SetCurrentTarget();
    MoveTimeline.TickTimeline(DeltaTime);
    CameraOffsetTimeline.TickTimeline(DeltaTime);
}

void UCombatSystem::OnCameraOffsetUpdate(float Alpha)
{
    // lerp from default→combat
    FVector NewOffset = FMath::Lerp(DefaultCameraOffset, CombatCameraOffset, Alpha);

    if (!Character) return;
    if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
    {
        if (AALSPlayerCameraManager* CamMgr =
            Cast<AALSPlayerCameraManager>(PC->PlayerCameraManager))
        {
            CamMgr->ManualCameraOffset = NewOffset;
        }
    }
}

void UCombatSystem::DrawGizmos() const
{
    if (!Character) return;
    UWorld* W = GetWorld();
    if (!W) return;

    const FVector Origin = Character->GetActorLocation();
    DrawDebugSphere(
        W,
        Origin,
        DetectionRadius,
        16,               // segments
        FColor::Blue,
        false,            // persistent
        0.0f,             // life time ? single frame only
        0,                // depth priority
        2.f               // thickness
    );

    for (AALSBaseCharacter* Enemy : CombatEnemies)
    {
        if (Enemy)
        {
            DrawDebugLine(
                W,
                Origin,
                Enemy->GetActorLocation(),
                FColor::Red,
                false,    // persistent
                0.0f,     // life time ? single frame only
                0,        // depth priority
                2.f       // thickness
            );
        }
    }
}


void UCombatSystem::SetPlayer(TObjectPtr<AALSBaseCharacter> player)
{
	Character = player;
}

void UCombatSystem::CheckForEnemies()
{
    CombatEnemies.Empty();

    if (!Character) return;

    UWorld* World = GetWorld();
    if (!World) return;

    const FVector Origin = Character->GetActorLocation();
    const FCollisionShape Sphere = FCollisionShape::MakeSphere(DetectionRadius);

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Character);

    TArray<FOverlapResult> Overlaps;
    const bool bHit = World->OverlapMultiByObjectType(
        Overlaps,
        Origin,
        FQuat::Identity,
        FCollisionObjectQueryParams(ECollisionChannel::ECC_Pawn),
        Sphere,
        Params
    );

    if (!bHit) return;

    for (const FOverlapResult& R : Overlaps)
    {
        AActor* Hit = R.GetActor();
        if (!Hit) continue;

        if (ABaseEnemyAI* ALSHit = Cast<ABaseEnemyAI>(Hit))
        {
            // Optional: also check for the "Enemy" tag 
            if (ALSHit->ActorHasTag(TEXT("Enemy")))
            {
                CombatEnemies.AddUnique(ALSHit);
            }
        }
    }
}

void UCombatSystem::SetCurrentTarget()
{
    if (!Character || CombatEnemies.Num() == 0)
    {
        CurrentTarget = nullptr;
        return;
    }

    // 1) Get camera viewpoint (location + rotation)
    FVector CamLoc;
    FRotator CamRot;
    if (APlayerController* PC = Cast<APlayerController>(Character->GetController()))
    {
        PC->GetPlayerViewPoint(CamLoc, CamRot);
    }
    else
    {
        CamLoc = Character->GetActorLocation();
        CamRot = Character->GetActorRotation();
    }
    const FVector AimDir = CamRot.Vector();

    UWorld* World = GetWorld();
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Character);

    float BestDot = -1.f;
    ABaseEnemyAI* BestTarget = nullptr;

    for (ABaseEnemyAI* Enemy : CombatEnemies)
    {
        if (!Enemy) continue;

        // 2) Dot between camera forward and direction to enemy
        FVector ToEnemy = Enemy->GetActorLocation() - CamLoc;
        float DistSqr = ToEnemy.SizeSquared();
        ToEnemy /= FMath::Sqrt(DistSqr);  // normalize

        float Dot = FVector::DotProduct(AimDir, ToEnemy);
        if (Dot <= BestDot)
            continue;  // not better than current best

        // 3) Line trace to see if anything blocks the view
        FHitResult Hit;
        const FVector EnemyLoc = Enemy->GetActorLocation();
        bool bHit = World->LineTraceSingleByChannel(
            Hit,
            CamLoc,
            EnemyLoc,
            ECC_Visibility,
            Params
        );

        // Only accept if trace hit *this* enemy (or if it hit nothing)
        if (!bHit || Hit.GetActor() == Enemy)
        {
            BestDot = Dot;
            BestTarget = Enemy;
        }
    }

    CurrentTarget = BestTarget;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
    if (CurrentTarget && GEngine)
    {
        GEngine->AddOnScreenDebugMessage(
            -1, 0.1f, FColor::Green,
            FString::Printf(TEXT("Locked onto: %s"), *CurrentTarget->GetName())
        );
    }
#endif
}

void UCombatSystem::OnSuccessfulHit()
{
    // called right after you call TargetOnAttack->GetHit(...)
    ComboCount++;
    // push to HUD
    if (Character)
    {
        if (auto* C = Cast<AALSCharacter>(Character))
            C->UpdateComboMeter(ComboCount);
    }
    // restart expire timer
    ResetComboTimer();
}

void UCombatSystem::ResetComboTimer()
{
    // cancel any existing
    GetWorld()->GetTimerManager().ClearTimer(ComboResetTimer);
    // schedule a new expire
    GetWorld()->GetTimerManager().SetTimer(
        ComboResetTimer,
        this, &UCombatSystem::ExpireCombo,
        ComboResetDelay, false
    );
}

void UCombatSystem::ExpireCombo()
{
    ComboCount = 0;
    if (Character)
    {
        if (auto* C = Cast<AALSCharacter>(Character))
            C->UpdateComboMeter(0);
    }
}

void UCombatSystem::Attack()
{
    if (!Character || Character->GetOverlayState() != EALSOverlayState::Combat || Attacking || bBeingHit || countering)
        return;

    // start of a new cycle?
    if (AttackIndex == 0)
    {
        ShufflePunchOrder();
        ShuffleKickOrder();
    }

    int32 MontageSlot = INDEX_NONE;
    if (AttackIndex < 2)
    {
        MontageSlot = PunchOrder[AttackIndex];
    }
    else // AttackIndex 2 or 3
    {
        MontageSlot = KickOrder[AttackIndex - 2];
    }

    if (AttackMontages.IsValidIndex(MontageSlot) && AttackMontages[MontageSlot])
    {
        Attacking = true;

        UAnimMontage* Chosen = AttackMontages[MontageSlot];

        Character->SetMovementState(EALSMovementState::Attacking);
        Character->Replicated_PlayMontage(Chosen, 1.8f);

        const float PlayRate = 1.2f;
        Character->Replicated_PlayMontage(Chosen, PlayRate);

        // Get the precise moment we want to finish moving
        float NotifyTime = GetMoveCompleteTime(Chosen);
        float MoveDuration = NotifyTime / PlayRate;

        TargetOnAttack = CurrentTarget;
        TargetOnAttack->bCanMove = false;
        SmoothTransitionToCurrentTarget(MoveDuration);

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Yellow,
                FString::Printf(TEXT("Combo slot %d"), MontageSlot));
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid combo slot %d"), MontageSlot);
    }

    AttackIndex = (AttackIndex + 1) % 4;
}

void UCombatSystem::Counter()
{
    if (bBeingHit)
        return;

    // 1) pick the first valid counter target
    CounterTarget = nullptr;
    for (ABaseEnemyAI* E : CombatEnemies)
        if (E && E->CanPlayerCounter())
        {
            CounterTarget = E; break;
        }
    if (!CounterTarget || !CounterMontage)
        return;

    countering = true;
    Character->countering = true;
    CachedPC->SetIgnoreMoveInput(true);
    Character->VelocityDirectionAction();

    // 1) Force the pawn to use controller yaw, not its movement component:
    if (auto* MoveComp = Character->GetCharacterMovement())
    {
        Character->bUseControllerRotationYaw = false;
        MoveComp->bOrientRotationToMovement = false;
    }

    // 2) immediately cancel their attack
    CounterTarget->bAttacking = false;
    CounterTarget->bIsApproaching = false;
    if (auto* Inst = CounterTarget->GetMesh()->GetAnimInstance())
        Inst->StopAllMontages(0.f);

    // 3) cancel ours if we were mid‐combo
    Attacking = false;
    MoveTimeline.Stop();

    // 4) play our counter montage
    Character->SetMovementState(EALSMovementState::Attacking);
    Character->Replicated_PlayMontage(CounterMontage, 1.f);

    // 5) set the pointer that the timeline and anim notify will use
    TargetOnAttack = CounterTarget;

    // 6) figure out when the “CounterHit” notify fires
    float NotifyTime = 0.f;
    for (auto& Ev : CounterMontage->Notifies)
        if (Ev.NotifyName == TEXT("CounterHit"))
            NotifyTime = Ev.GetTriggerTime();
    if (NotifyTime <= 0.f)
        NotifyTime = CounterMontage->GetPlayLength() * 0.5f;

    // 7) only lunge if we’re farther than ApproachDistance
    const float DistSq = FVector::DistSquared(
        Character->GetActorLocation(),
        CounterTarget->GetActorLocation()
    );

    if (DistSq > FMath::Square(ApproachDistanceCounter))
    {
        // compute start/end of the lunge
        MoveStart = Character->GetActorLocation();
        FVector TLoc = CounterTarget->GetActorLocation();

        FRotator CR = CachedPC
            ? CachedPC->GetControlRotation()
            : Character->GetActorRotation();
        FVector Dir = CR.Vector(); Dir.Z = 0.f; Dir.Normalize();

        MoveEnd = TLoc - Dir * ApproachDistanceCounter
            + FVector(0, 0, ApproachHeightOffset);

        // drive the timeline so we arrive exactly at NotifyTime
        MoveTimeline.SetPlayRate(1.f / NotifyTime);
        MoveTimeline.PlayFromStart();
    }
    else
    {
        // we’re already “in range” → just snap to face them
        FVector FaceDir = (CounterTarget->GetActorLocation() - Character->GetActorLocation()).GetSafeNormal();
        Character->SetActorRotation(FaceDir.Rotation());
        // anim notify will still fire CounterHit at the right time
    }
}

void UCombatSystem::OnCounterNotify()
{
    if (!TargetOnAttack)
        return;

    // 2) Enemy → Character
    FVector ToPlayer = (Character->GetActorLocation() - TargetOnAttack->GetActorLocation()).GetSafeNormal2D();
    FRotator EnemyRot = ToPlayer.Rotation();

    // and their tweak:
    EnemyRot.Yaw += EnemyLookAtYawOffset;

    TargetOnAttack->SetActorRotation(EnemyRot);

    //-------------------------------------
    const FVector PlayerLoc = Character->GetActorLocation();
    const FVector EnemyLoc = TargetOnAttack->GetActorLocation();
    const FVector ToEnemy2 = (EnemyLoc - PlayerLoc).GetSafeNormal2D();

    // 2) Compute the midpoint in world-space
    FVector Midpoint = (PlayerLoc + EnemyLoc) * 0.5f;

    // 3) Build the exact offset vector
    const float HalfSep = CounterSeparationDistance * 0.5f;
    FVector OffsetVec = ToEnemy2 * HalfSep;

    // 4) Compute new positions, preserving original Z
    FVector NewPlayerLoc = Midpoint - OffsetVec;
    NewPlayerLoc.Z = PlayerLoc.Z;

    FVector NewEnemyLoc = Midpoint + OffsetVec;
    NewEnemyLoc.Z = EnemyLoc.Z;

    // 5) Snap them into place
    Character->SetActorLocation(NewPlayerLoc, false);
    TargetOnAttack->SetActorLocation(NewEnemyLoc, false);
    //-------------------------------------

    // cancel any leftover AI approach/attack
    TargetOnAttack->bAttacking = false;
    TargetOnAttack->bIsApproaching = false;
    TargetOnAttack->bCanMove = false;

    // play “you got countered” montage
    if (TargetOnAttack->BeingCounteredMontage)
        TargetOnAttack->Replicated_PlayMontage(
            TargetOnAttack->BeingCounteredMontage, 1.f);

    // grab a strong local pointer
    ABaseEnemyAI* HitEnemy = TargetOnAttack;

    // how long the montage is
    float Duration = HitEnemy->BeingCounteredMontage->GetPlayLength();


    // schedule re‐enable + group advance
    FTimerHandle TmpHandle;
    GetWorld()->GetTimerManager().SetTimer(
        TmpHandle,
        FTimerDelegate::CreateLambda([this, HitEnemy]()
            {
                if (!HitEnemy) return;

                // 1) allow them to move again
                HitEnemy->bCanMove = true;

                // 2) tell the group they’re done so the next attacker fires
                HitEnemy->OnAttackComplete.Broadcast(HitEnemy);

                // 3) clear our own pointers
                CounterTarget = nullptr;
                TargetOnAttack = nullptr;
                countering = false;
                Character->countering = false;
                CachedPC->SetIgnoreMoveInput(false);
                Character->bUseControllerRotationYaw = true;
                Character->LookingDirectionAction();
            }),
        Duration,
        false
    );
}

void UCombatSystem::SmoothTransitionToCurrentTarget(float Duration)
{
    if (!CurrentTarget) return;

    MoveStart = Character->GetActorLocation();
    const FVector TargetLoc = CurrentTarget->GetActorLocation();

    // 1) compute dynamic offset based on view
    FRotator ControlRot = Character->GetController()
        ? Character->GetController()->GetControlRotation()
        : Character->GetActorRotation();
    FVector ViewDir = ControlRot.Vector();
    ViewDir.Z = 0.f;
    ViewDir.Normalize();

    FVector Offset = -ViewDir * ApproachDistance;
    Offset.Z = ApproachHeightOffset;

    MoveEnd = TargetLoc + Offset;

    // 2) short-circuit if already close
    if (FVector::DistSquared(MoveStart, MoveEnd) <= MinMoveDistance * MinMoveDistance)
    {
        return;
    }

    // 3) start timeline as before
    MoveTimeline.SetTimelineLengthMode(ETimelineLengthMode::TL_LastKeyFrame);
    MoveTimeline.SetPlayRate(1.f / Duration);
    MoveTimeline.PlayFromStart();
}



float UCombatSystem::GetMoveCompleteTime(UAnimMontage* Montage)
{
    if (!Montage) return 0.f;

    // Scan the notify list instead of BranchingPoints
    for (const FAnimNotifyEvent& Ev : Montage->Notifies)
    {
        if (Ev.NotifyName == TEXT("MoveComplete"))
        {
            // TriggerTimeOffset + DisplayTime gives you the time in seconds
            return Ev.GetTriggerTime();
        }
    }

    // Fallback to the full length
    return Montage->GetPlayLength();
}

void UCombatSystem::InitAnimNotifies()
{
    UAttackLandAnimNotify::OnNotified.AddUObject(this, &UCombatSystem::OnAttackLanded);
    UCounterAnimNotify::OnNotified.AddUObject(this, &UCombatSystem::OnCounterNotify);
    UCounterLandAnimNotify::OnNotified.AddUObject(this, &UCombatSystem::OnCounterAttackLanded);
}

void UCombatSystem::OnAttackLanded()
{
    // e.g.:
    if (TargetOnAttack)
    {
        TargetOnAttack->GetHit(Character->GetActorLocation(), ComboCount); //when this happens we should also pass a value so the enemy knows if the player has a high combo, cuz if they do they will ragdoll on hit instead of it taking 3 hits or whatever...
        OnSuccessfulHit();
        GetWorld()->GetFirstPlayerController()->PlayerCameraManager->StartCameraShake(Character->CameraShake, 25.0f);
    }
}

void UCombatSystem::OnCounterAttackLanded()
{
    // e.g.:
    OnSuccessfulHit();
    GetWorld()->GetFirstPlayerController()->PlayerCameraManager
        ->StartCameraShake(Character->CameraShake, 25.f);
}

void UCombatSystem::OnMoveTimelineUpdate(float Alpha)
{

    if (!TargetOnAttack)
        return;

    // 1) Curve remapping
    const float CurveAlpha = MoveCurve ? MoveCurve->GetFloatValue(Alpha) : Alpha;

    // 2) Compute the interpolated location
    const FVector NewLoc = FMath::Lerp(MoveStart, MoveEnd, CurveAlpha);

    // 3) Compute a look?at rotation so we face the target
    FVector ToEnemy = (TargetOnAttack->GetActorLocation() - NewLoc);
    ToEnemy.Z = 0.f; // keep only yaw; remove pitch if you want level feet
    FRotator LookAtRot = ToEnemy.Rotation();

    // 4) Snap the character into place and rotate toward the enemy
    Character->SetActorLocationAndRotation(NewLoc, LookAtRot);
}

void UCombatSystem::ShufflePunchOrder()
{
	// pick 0 or 1 at random as the first punch
	int32 First = FMath::RandRange(0, 1);
	PunchOrder[0] = First;
	PunchOrder[1] = 1 - First;
}

void UCombatSystem::ShuffleKickOrder()
{
	// pick 0 or 1; then map to slot 2 or 3
	const int32 First = FMath::RandRange(0, 1);
	KickOrder[0] = 2 + First;        // either 2 or 3
	KickOrder[1] = 2 + (1 - First);  // the other one
}

void UCombatSystem::ReceiveHit(const FVector& HitFromLocation)
{
    if (!HitMontage || !Character) return;

    bBeingHit = true;
    Attacking = false;

    // Stop whatever montage was playing, then play the hit one.
    Character->StopAnimMontage();
    Character->PlayAnimMontage(HitMontage);

    // Disable movement
    if (CachedPC)
        CachedPC->SetIgnoreMoveInput(true);

    // Bind to the montage end callback
    if (UAnimInstance* AnimI = Character->GetMesh()->GetAnimInstance())
    {
        FOnMontageEnded EndDel;
        EndDel.BindUObject(this, &UCombatSystem::OnHitMontageEnded);
        AnimI->Montage_SetEndDelegate(EndDel, HitMontage);
    }

    // knockback
    FVector Dir = (Character->GetActorLocation() - HitFromLocation).GetSafeNormal();
    Character->LaunchCharacter(
        Dir * HitKnockbackStrength + FVector(0, 0, HitKnockbackUpStrength),
        true, true
    );

    ExpireCombo();
}

void UCombatSystem::OnHitMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    // only handle our hit montage
    if (Montage != HitMontage)
        return;

    // clear the flag
    bBeingHit = false;

    // re-enable input
    if (CachedPC)
        CachedPC->SetIgnoreMoveInput(false);

    // restore movement state
    Character->SetMovementState(EALSMovementState::Grounded);

    // if no enemies, reset overlay
    if (CombatEnemies.Num() == 0)
        Character->SetOverlayState(EALSOverlayState::Default);
}

void UCombatSystem::OnHitReactionFinished()
{
    // 1) re-enable normal movement
    if (Character)
    {
        if (CachedPC)
        {
            CachedPC->SetIgnoreMoveInput(false);
        }
    }

    // 2) clear our “being hit” blocker
    bBeingHit = false;

    // 3) restore overlay to Default (so your tick logic won’t immediately force us back into Combat unless there really are enemies)
    if (Character && CombatEnemies.Num() == 0)
    {
        Character->SetOverlayState(EALSOverlayState::Default);
    }
}
