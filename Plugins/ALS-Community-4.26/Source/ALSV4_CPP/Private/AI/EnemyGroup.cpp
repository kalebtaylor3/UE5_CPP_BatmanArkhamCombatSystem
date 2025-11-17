// EnemyGroup.cpp

#include "AI/EnemyGroup.h"
#include "AI/BaseEnemyAI.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"

AEnemyGroup::AEnemyGroup()
{
    PrimaryActorTick.bCanEverTick = true;

    GroupCircleSpline = CreateDefaultSubobject<USplineComponent>("GroupCircleSpline");
    GroupCircleSpline->SetupAttachment(RootComponent);
    GroupCircleSpline->bDrawDebug = true;           // shows the curve in editor
    GroupCircleSpline->SetClosedLoop(true);         // make it wrap around
    GroupCircleSpline->SetMobility(EComponentMobility::Movable);


#if WITH_EDITORONLY_DATA
    CenterGizmo = CreateDefaultSubobject<UBillboardComponent>("CenterGizmo");
    CenterGizmo->SetupAttachment(RootComponent);
    // pick any tiny sprite or leave default; ensure it’s hidden in-game:
    CenterGizmo->bHiddenInGame = true;
    CenterGizmo->bIsEditorOnly = true;
#endif
}

void AEnemyGroup::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    UpdateDebugShape();
}

void AEnemyGroup::BeginPlay()
{
    Super::BeginPlay();
}

void AEnemyGroup::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bIsEngaged)
    {
        // check distance to player to auto-engage
        APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
        if (Player && (GetActorLocation() - Player->GetActorLocation()).Size() <= EngagementRadius)
        {
            Engage();
            if (OwningController) OwningController->NotifyGroupEngaged(this);
        }
    }
    else
    {
        UpdateFormation(DeltaTime);
    }
}

void AEnemyGroup::Engage()
{
    if (bIsEngaged)
        return;

    // If there's already an engaged group, fold this one into it and bail.
    if (OwningController)
    {
        for (AEnemyGroup* Other : OwningController->Groups)
        {
            if (Other != this && Other->bIsEngaged)
            {
                // Merge *this* into Other—will destroy this actor.
                OwningController->MergeGroupInto(Other, this);
                return;
            }
        }
    }

    // No existing engaged group, so we become the master
    bIsEngaged = true;
    for (ABaseEnemyAI* E : Members)
        if (E)
            E->SetOverlayState(EALSOverlayState::Combat);
    OnEngaged();
}

void AEnemyGroup::Disengage()
{
    if (!bIsEngaged) return;
    bIsEngaged = false;
    OnDisengaged();
}

void AEnemyGroup::OnEngaged()
{
    // clear any old bindings
    for (auto* E : Members)
        if (E) E->OnAttackComplete.RemoveAll(this);

    CurrentAttacker = nullptr;

    // start exactly one queue
    RequestNextAttacker();

    float FirstDelay = FMath::FRandRange(MinIdleSwapInterval, MaxIdleSwapInterval);
    GetWorld()->GetTimerManager().SetTimer(
        IdleSwapTimer,
        this, &AEnemyGroup::DoIdleSwap,
        FirstDelay,
        false
    );
}

void AEnemyGroup::OnDisengaged()
{
    // stop the pending next-attacker
    GetWorld()->GetTimerManager().ClearTimer(AttackTurnTimer);

    // unbind the current attacker
    if (CurrentAttacker)
        CurrentAttacker->OnAttackComplete.RemoveAll(this);

    CurrentAttacker = nullptr;

    GetWorld()->GetTimerManager().ClearTimer(IdleSwapTimer);
}


void AEnemyGroup::DoIdleSwap()
{
    int32 N = Members.Num();
    if (N < 2 || CurrentAttacker) return;  // don’t disturb mid?attack or tiny groups

    // pick two distinct slots
    int32 A = FMath::RandRange(0, N - 1);
    int32 B = FMath::RandRange(0, N - 2);
    if (B >= A) ++B;  // ensures B != A

    // swap everything for just those two
    FormationAngles.Swap(A, B);
    FormationOffsets.Swap(A, B);
    IdleSpeeds.Swap(A, B);
    IdlePulsePhases.Swap(A, B);

    // now give those two *slightly different* speeds so they don’t stay in lock?step
    IdleSpeeds[A] *= FMath::FRandRange(0.9f, 1.1f);
    IdleSpeeds[B] *= FMath::FRandRange(0.9f, 1.1f);

    float NextDelay = FMath::FRandRange(MinIdleSwapInterval, MaxIdleSwapInterval);
    GetWorld()->GetTimerManager().SetTimer(
        IdleSwapTimer,
        this, &AEnemyGroup::DoIdleSwap,
        NextDelay,
        /*bLoop=*/false
    );
}

void AEnemyGroup::RequestNextAttacker()
{
    if (Members.Num() == 0) return;

    // pick a random member who can move
    TArray<ABaseEnemyAI*> Valid;
    for (auto* E : Members)
        if (E && E->bCanMove)
            Valid.Add(E);

    if (Valid.Num() == 0) return;

    CurrentAttacker = Valid[FMath::RandRange(0, Valid.Num() - 1)];
    // bind to their finish event
    CurrentAttacker->OnAttackComplete.AddDynamic(this, &AEnemyGroup::OnMemberFinishedAttack);

    // tell them to approach + attack
    CurrentAttacker->StartApproachAttack();
}

void AEnemyGroup::UpdateFormation(float DeltaTime)
{
    APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
    if (!Player || Members.Num() == 0) return;

    const FVector CurrentCenter = Player->GetActorLocation();
    const float DistSq = FVector::DistSquaredXY(CurrentCenter, LastFormationCenter);

    if (FormationAngles.Num() != Members.Num())
        RebuildFormationOffsets();

    if (!CurrentAttacker)
    {
        LastFormationCenter = CurrentCenter;

        for (int32 i = 0; i < Members.Num(); ++i)
        {
            auto* E = Members[i];
            if (!E || !E->bCanMove) continue;

            // advance this member’s personal timer
            IdleTimers[i] += DeltaTime;

            if (IdleIsSwirling[i])
            {
                // end swirling?
                if (IdleTimers[i] >= IdleSwirlDurations[i])
                {
                    IdleIsSwirling[i] = false;
                    IdleTimers[i] = 0.f;
                    continue;
                }

                // actually spin & pulse **per-member**
                FormationAngles[i] += IdleSpeeds[i] * DeltaTime;
                FormationAngles[i] = FMath::Fmod(FormationAngles[i], 2.f * PI);

                float pulse = FMath::Sin(IdlePulsePhases[i] + IdlePulseSpeed * IdleTimers[i])
                    * IdlePulseAmount;
                float r = FormationRadius + pulse;

                FVector Off(
                    FMath::Cos(FormationAngles[i]) * r,
                    FMath::Sin(FormationAngles[i]) * r,
                    0.f
                );
                E->MoveToPosition(CurrentCenter + Off);
            }
            else
            {
                // end pause?
                if (IdleTimers[i] >= IdlePauseDurations[i])
                {
                    IdleIsSwirling[i] = true;
                    IdleTimers[i] = 0.f;
                }
                // else do nothing (stay at last spot)
            }
        }
    }
    else
    {
        // your existing “snap back into formation except attacker” code...
        if (LastFormationCenter.IsNearlyZero() ||
            DistSq > FMath::Square(CenterMoveThreshold))
        {
            LastFormationCenter = CurrentCenter;
            if (FormationOffsets.Num() != Members.Num())
                RebuildFormationOffsets();

            for (int32 i = 0; i < Members.Num(); ++i)
            {
                auto* E = Members[i];
                if (!E || E == CurrentAttacker) continue;
                E->MoveToPosition(CurrentCenter + FormationOffsets[i]);
            }
        }
    }
}

void AEnemyGroup::OnMemberFinishedAttack(ABaseEnemyAI* Enemy)
{
    Enemy->OnAttackComplete.RemoveAll(this);
    CurrentAttacker = nullptr;

    // send them home...
    const int32 Idx = Members.IndexOfByKey(Enemy);
    if (Idx != INDEX_NONE && FormationOffsets.IsValidIndex(Idx))
    {
        Enemy->MoveToPosition(LastFormationCenter + FormationOffsets[Idx]);
    }

    // schedule the next turn via our single handle
    GetWorld()->GetTimerManager().SetTimer(
        AttackTurnTimer,
        this,
        &AEnemyGroup::RequestNextAttacker,
        0.05f,
        false
    );
}

void AEnemyGroup::UpdateDebugShape()
{
    if (Members.Num() == 0)
    {
        GroupCircleSpline->ClearSplinePoints();
#if WITH_EDITORONLY_DATA
        CenterGizmo->SetHiddenInGame(true);
#endif
        return;
    }

    // 1) find the center
    FVector Sum = FVector::ZeroVector;
    for (auto* E : Members)
        if (E) Sum += E->GetActorLocation();
    FVector Center = Sum / Members.Num();

    // 2) compute radius
    float MaxD2 = 0.f;
    for (auto* E : Members)
        if (E)
            MaxD2 = FMath::Max(MaxD2, FVector::DistSquaredXY(E->GetActorLocation(), Center));
    float Radius = FMath::Sqrt(MaxD2);

    // 3) rebuild spline circle
    const int32 Segments = 32;
    GroupCircleSpline->ClearSplinePoints(false);
    for (int32 i = 0; i <= Segments; ++i)
    {
        float Angle = 2 * PI * i / Segments;
        FVector P = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;
        GroupCircleSpline->AddSplinePoint(P, ESplineCoordinateSpace::World, false);
    }
    GroupCircleSpline->UpdateSpline();

    // 4) move the billboard gizmo there
#if WITH_EDITORONLY_DATA
    CenterGizmo->SetWorldLocation(Center);
    CenterGizmo->SetHiddenInGame(true);
#endif
}

void AEnemyGroup::RebuildFormationOffsets()
{
    FormationOffsets.Reset();
    FormationAngles.Reset();
    IdleSpeeds.Reset();
    IdlePulsePhases.Reset();

    IdleIsSwirling.Reset();
    IdleTimers.Reset();
    IdleSwirlDurations.Reset();
    IdlePauseDurations.Reset();

    const int32 Count = Members.Num();
    if (Count == 0) return;

    const float TwoPI = 2.f * PI;
    for (int32 i = 0; i < Count; ++i)
    {
        // 1) base formation
        float baseAngle = TwoPI * (float(i) / float(Count))
            + FMath::FRandRange(-PI / 8.f, PI / 8.f);
        FormationAngles.Add(baseAngle);
        FormationOffsets.Add(FVector(
            FMath::Cos(baseAngle) * FormationRadius,
            FMath::Sin(baseAngle) * FormationRadius,
            0.f
        ));

        // 2) random spin speed & pulse phase
        float speed = FMath::FRandRange(IdleRotationSpeedMin, IdleRotationSpeedMax)
            * (FMath::RandBool() ? 1.f : -1.f);
        IdleSpeeds.Add(speed);
        IdlePulsePhases.Add(FMath::FRandRange(0.f, TwoPI));

        // 3) each member starts either swirling or pausing
        bool startSwirl = FMath::RandBool();
        IdleIsSwirling.Add(startSwirl);

        // 4) pick random durations
        float swirlDur = FMath::FRandRange(MinIdleActiveDuration, MaxIdleActiveDuration);
        float pauseDur = FMath::FRandRange(MinIdleInactiveDuration, MaxIdleInactiveDuration);
        IdleSwirlDurations.Add(swirlDur);
        IdlePauseDurations.Add(pauseDur);

        // 5) random initial offset into that state
        float t0 = FMath::FRandRange(0.f, startSwirl ? swirlDur : pauseDur);
        IdleTimers.Add(t0);
    }
}

void AEnemyGroup::HandleMemberRecovered(ABaseEnemyAI* Enemy)
{
    if (!bIsEngaged || !Enemy) return;

    // find its slot index
    int32 Idx = Members.IndexOfByKey(Enemy);
    if (Idx == INDEX_NONE || !FormationOffsets.IsValidIndex(Idx))
        return;

    // send it back home
    const FVector Home = LastFormationCenter + FormationOffsets[Idx];
    Enemy->MoveToPosition(Home);

    // if nobody is currently attacking, kick off the next turn
    if (!CurrentAttacker)
    {
        RequestNextAttacker();
    }
}
