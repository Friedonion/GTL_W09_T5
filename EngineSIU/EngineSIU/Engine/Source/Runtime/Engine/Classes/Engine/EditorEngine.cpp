#include "EditorEngine.h"

#include "World/World.h"
#include "Level.h"
#include "GameFramework/Actor.h"
#include "Classes/Engine/AssetManager.h"
#include "Components/Light/DirectionalLightComponent.h"
#include "UnrealEd/EditorConfigManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "tinyfiledialogs/tinyfiledialogs.h"
#include "UnrealEd/SceneManager.h"
#include "Games/LastWar/Characters/PlayerCharacter.h"
#include "Games/LastWar/Characters/EnemyCharacter.h"
#include "Games/LastWar/Characters/Wall.h"
#include "Camera/PlayerCameraManager.h"
#include "Audio/AudioManager.h"

namespace PrivateEditorSelection
{
    static AActor* GActorSelected = nullptr;
    static AActor* GActorHovered = nullptr;

    static USceneComponent* GComponentSelected = nullptr;
    static USceneComponent* GComponentHovered = nullptr;
}

void UEditorEngine::Init()
{
    Super::Init();

    // Initialize the engine
    GEngine = this;

    FWorldContext& EditorWorldContext = CreateNewWorldContext(EWorldType::Editor);

    EditorWorld = UWorld::CreateWorld(this, EWorldType::Editor, FString("EditorWorld"));

    EditorWorldContext.SetCurrentWorld(EditorWorld);
    ActiveWorld = EditorWorld;

    EditorPlayer = FObjectFactory::ConstructObject<UEditorPlayer>(this);
    EditorPlayer->Initialize();

    if (AssetManager == nullptr)
    {
        AssetManager = FObjectFactory::ConstructObject<UAssetManager>(this);
        assert(AssetManager);
        AssetManager->InitAssetManager();
    }

    
    TMap<FString, FString> Config = FEditorConfigManager::GetInstance().Read();
    FString ScenePath = FEditorConfigManager::GetValueFromConfig<std::string>(Config, "ScenePath", "Saved/DefaultLevel.scene");
    
#ifndef _DEBUG_VIEWER
    LoadLevel(ScenePath);
#endif // DEBUG_VIEWER
}

bool UEditorEngine::TryQuit(bool& OutbIsSave)
{
    int response = tinyfd_messageBox(
        "Engine SIUUU",
        "변경 사항을 저장하시겠습니까?",
        "yesnocancel", // 버튼 세 개
        "question",    // 아이콘
        0              // 기본 버튼 (0 = 첫 번째 버튼이 기본)
    );

    if (response == 1)
    {
        // Save
        OutbIsSave = true;
        return true;
    }
    else if (response == 2)
    {
        // Do not Save
        OutbIsSave = false;
        return true;
    }
    else if (response == 0)
    {
        // 취소
        OutbIsSave = false;
        return false;
    }

    OutbIsSave = false;
    return true;   
}

void UEditorEngine::Release()
{
}

void UEditorEngine::LoadLevel(const FString& FilePath) const
{
    SceneManager::LoadSceneFromJsonFile(GetData(FilePath), *ActiveWorld);
}

void UEditorEngine::SaveLevel(const FString& FilePath) const
{
    FString ScenePath;
    if (FilePath.IsEmpty())
    {
        ScenePath = ActiveWorld->GetActiveLevel()->GetLevelPath();
    }
    else
    {
        ScenePath = FilePath;
    }
    SceneManager::SaveSceneToJsonFile(GetData(ScenePath), *ActiveWorld);
}

void UEditorEngine::SaveConfig() const
{
    FString ScenePath = ActiveWorld->GetActiveLevel()->GetLevelPath();
    FEditorConfigManager::GetInstance().AddConfig("ScenePath", ScenePath);
}

void UEditorEngine::Tick(float DeltaTime)
{
    for (FWorldContext* WorldContext : WorldList)
    {
        if (WorldContext->WorldType == EWorldType::Editor)
        {
            if (UWorld* World = WorldContext->World())
            {
                // TODO: World에서 EditorPlayer 제거 후 Tick 호출 제거 필요.
                World->Tick(DeltaTime);
                ULevel* Level = World->GetActiveLevel();
                TArray CachedActors = Level->Actors;
                if (Level)
                {
                    for (AActor* Actor : CachedActors)
                    {
                        if (Actor && Actor->IsActorTickInEditor())
                        {
                            Actor->Tick(DeltaTime);
                        }
                    }
                }
            }
        }
        else if (WorldContext->WorldType == EWorldType::PIE)
        {
            if (UWorld* World = WorldContext->World())
            {
                World->Tick(DeltaTime);
                ULevel* Level = World->GetActiveLevel();
                TArray CachedActors = Level->Actors;
                if (Level)
                {
                    for (AActor* Actor : CachedActors)
                    {
                        if (Actor)
                        {
                            Actor->Tick(DeltaTime);
                        }
                    }
                }
            }
        }
    }
}

void UEditorEngine::StartPIE()
{
    if (PIEWorld)
    {
        UE_LOG(LogLevel::Warning, TEXT("PIEWorld already exists!"));
        return;
    }

    FWorldContext& PIEWorldContext = CreateNewWorldContext(EWorldType::PIE);

    PIEWorld = Cast<UWorld>(EditorWorld->Duplicate(this));
    PIEWorld->WorldType = EWorldType::PIE;

    PIEWorldContext.SetCurrentWorld(PIEWorld);
    ActiveWorld = PIEWorld;

    // GameMode 없으므로 StartPIE에서 바로 PC 생성
    // 1) PlayerController 스폰
    APlayerController* PC = ActiveWorld->SpawnActor<APlayerController>();
    ActiveWorld->AddPlayerController(PC);

    // 2) Character 스폰 및 Possess
    ACharacter* PlayerCharacter = ActiveWorld->SpawnActor<APlayerCharacter>();
    PC->Possess(PlayerCharacter);


    //PIEWorld->BeginPlay();
    PIEWorld->BeginPlay();
    GEngine->ActiveWorld->GetFirstPlayerController()->PlayerCameraManager->ViewTarget.Target = PlayerCharacter;
    //GEngine->ActiveWorld->GetFirstPlayerController()->PlayerCameraManager->StartCameraFade(0.0f, 1.0f, 10.0f, FLinearColor::Red, false, true);

    

    // 나중에 제거하기
}

void UEditorEngine::EndPIE()
{
    if (PIEWorld)
    {
        AudioManager::Get().StopBgm();
        //WorldList.Remove(*GetWorldContextFromWorld(PIEWorld.get()));
        WorldList.Remove(GetWorldContextFromWorld(PIEWorld));
        PIEWorld->Release();
        GUObjectArray.MarkRemoveObject(PIEWorld);
        PIEWorld = nullptr;

        // TODO: PIE에서 EditorWorld로 돌아올 때, 기존 선택된 Picking이 유지되어야 함. 현재는 에러를 막기위해 임시조치.
        
        DeselectActor(GetSelectedActor());
        DeselectComponent(GetSelectedComponent());
        

    }
    // 다시 EditorWorld로 돌아옴.
    ActiveWorld = EditorWorld;
}

FWorldContext& UEditorEngine::GetEditorWorldContext(/*bool bEnsureIsGWorld*/)
{
    for (FWorldContext* WorldContext : WorldList)
    {
        if (WorldContext->WorldType == EWorldType::Editor)
        {
            return *WorldContext;
        }
    }
    return CreateNewWorldContext(EWorldType::Editor);
}

FWorldContext* UEditorEngine::GetPIEWorldContext(/*int32 WorldPIEInstance*/)
{
    for (FWorldContext* WorldContext : WorldList)
    {
        if (WorldContext->WorldType == EWorldType::PIE)
        {
            return WorldContext;
        }
    }
    return nullptr;
}

void UEditorEngine::SelectActor(AActor* InActor)
{
    if (InActor && CanSelectActor(InActor))
    {
        PrivateEditorSelection::GActorSelected = InActor;
    }
}

void UEditorEngine::DeselectActor(AActor* InActor)
{
    if (PrivateEditorSelection::GActorSelected == InActor && InActor)
    {
        PrivateEditorSelection::GActorSelected = nullptr;
        ClearComponentSelection();
    }
}

void UEditorEngine::ClearActorSelection()
{
    PrivateEditorSelection::GActorSelected = nullptr;
}

bool UEditorEngine::CanSelectActor(const AActor* InActor) const
{
    return InActor != nullptr && InActor->GetWorld() == ActiveWorld && !InActor->IsActorBeingDestroyed();
}

AActor* UEditorEngine::GetSelectedActor() const
{
    return PrivateEditorSelection::GActorSelected;
}

void UEditorEngine::HoverActor(AActor* InActor)
{
    if (InActor)
    {
        PrivateEditorSelection::GActorHovered = InActor;
    }
}

void UEditorEngine::NewLevel()
{
    ClearActorSelection();
    ClearComponentSelection();

    if (ActiveWorld->GetActiveLevel())
    {
        ActiveWorld->GetActiveLevel()->Release();
    }
}

void UEditorEngine::SelectComponent(USceneComponent* InComponent) const
{
    if (InComponent && CanSelectComponent(InComponent))
    {
        PrivateEditorSelection::GComponentSelected = InComponent;
    }
}

void UEditorEngine::DeselectComponent(USceneComponent* InComponent)
{
    // 전달된 InComponent가 현재 선택된 컴포넌트와 같다면 선택 해제
    if (PrivateEditorSelection::GComponentSelected == InComponent && InComponent != nullptr)
    {
        PrivateEditorSelection::GComponentSelected = nullptr;
    }
}

void UEditorEngine::ClearComponentSelection()
{
    PrivateEditorSelection::GComponentSelected = nullptr;
}

bool UEditorEngine::CanSelectComponent(const USceneComponent* InComponent) const
{
    return InComponent != nullptr && InComponent->GetOwner() && InComponent->GetOwner()->GetWorld() == ActiveWorld && !InComponent->GetOwner()->IsActorBeingDestroyed();
}

USceneComponent* UEditorEngine::GetSelectedComponent() const
{
    return PrivateEditorSelection::GComponentSelected;
}

void UEditorEngine::HoverComponent(USceneComponent* InComponent)
{
    if (InComponent)
    {
        PrivateEditorSelection::GComponentHovered = InComponent;
    }
}

UEditorPlayer* UEditorEngine::GetEditorPlayer() const
{
    return EditorPlayer;
}
