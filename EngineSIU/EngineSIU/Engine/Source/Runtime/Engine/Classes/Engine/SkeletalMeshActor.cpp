#include "SkeletalMeshActor.h"
#include "Components/Mesh/SkeletalMeshComponent.h"
#include "Engine/FLoaderFBX.h"
#include "UObject/Casts.h"

ASkeletalMeshActor::ASkeletalMeshActor()
{
    SkeletalMeshComponent = AddComponent<USkeletalMeshComponent>();
    RootComponent = SkeletalMeshComponent;
    USkeletalMesh* DefaultMesh = FManagerFBX::CreateSkeletalMesh("Contents/riden/source/riden.fbx");
    if (DefaultMesh)
    {
        SkeletalMeshComponent->SetSkeletalMesh(DefaultMesh);
    }
    SetActorTickInEditor(true);
}

UObject* ASkeletalMeshActor::Duplicate(UObject* InOuter)
{
    ThisClass* NewActor = Cast<ThisClass>(Super::Duplicate(InOuter));
    NewActor->SkeletalMeshComponent = Cast<USkeletalMeshComponent>(NewActor->GetComponentByClass<USkeletalMeshComponent>());
    return NewActor;
}

void ASkeletalMeshActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}
