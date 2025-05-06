#include "EditorViewportClient.h"

#include "EditorConfigManager.h"
#include "fstream"
#include "sstream"
#include "ostream"
#include "Math/JungleMath.h"
#include "UnrealClient.h"
#include "WindowsCursor.h"
#include "GameFramework/Actor.h"

#include "UObject/ObjectFactory.h"
#include "BaseGizmos/TransformGizmo.h"
#include "Engine/EditorEngine.h"
#include "Engine/Engine.h"
#include "LevelEditor/SLevelEditor.h"
#include "SlateCore/Input/Events.h"
#include "GameFramework/PlayerController.h"
#include "Components/PrimitiveComponent.h"

FEditorViewportClient::FEditorViewportClient() : FViewportClient()
{
}

void FEditorViewportClient::Initialize(EViewScreenLocation InViewportIndex, const FRect& InRect)
{
    ViewportIndex = static_cast<uint32>(InViewportIndex);
    
    PerspectiveCamera.SetLocation(FVector(8.0f, 8.0f, 8.f));
    PerspectiveCamera.SetRotation(FVector(0.0f, 45.0f, -135.0f));
    
    Viewport = new FViewport(InViewportIndex);
    Viewport->Initialize(InRect);

    GizmoActor = FObjectFactory::ConstructObject<ATransformGizmo>(GEngine); // TODO : EditorEngine 외의 다른 Engine 형태가 추가되면 GEngine 대신 다른 방식으로 넣어주어야 함.
    GizmoActor->Initialize(this);
}

void FEditorViewportClient::Tick(const float DeltaTime)
{
    FViewportClient::Tick(DeltaTime);
    
    if(GEngine->ActiveWorld->WorldType == EWorldType::Editor)
    {
        UpdateEditorCameraMovement(DeltaTime);
        GizmoActor->Tick(DeltaTime);
    }

    UpdateViewMatrix();
    UpdateProjectionMatrix();
}

void FEditorViewportClient::UpdateEditorCameraMovement(const float DeltaTime)
{
    if (PressedKeys.Contains(EKeys::A))
    {
        CameraMoveRight(-100.f * DeltaTime);
    }

    if (PressedKeys.Contains(EKeys::D))
    {
        CameraMoveRight(100.f * DeltaTime);
    }

    if (PressedKeys.Contains(EKeys::W))
    {
        CameraMoveForward(100.f * DeltaTime);
    }

    if (PressedKeys.Contains(EKeys::S))
    {
        CameraMoveForward(-100.f * DeltaTime);
    }

    if (PressedKeys.Contains(EKeys::E))
    {
        CameraMoveUp(100.f * DeltaTime);
    }

    if (PressedKeys.Contains(EKeys::Q))
    {
        CameraMoveUp(-100.f * DeltaTime);
    }
}

void FEditorViewportClient::InputKey(const FKeyEvent& InKeyEvent)
{
    // TODO: 나중에 InKeyEvent.GetKey();로 가져오는걸로 수정하기
    // TODO: 나중에 PIEViewportClient에서 처리하는걸로 수정하기
    if (GEngine->ActiveWorld->WorldType == EWorldType::PIE)
    {
        // PIE 모드 → 게임 플레이 입력 처리
        UWorld* PlayWorld = GEngine->ActiveWorld;
        if (PlayWorld)
        {
            // 첫 번째 플레이어 컨트롤러에게 전달
            if (APlayerController* PC = PlayWorld->GetFirstPlayerController())
            {
                // FKeyEvent → EKeys, EInputEvent로 변환 후 호출

                switch (InKeyEvent.GetCharacter())
                {
                case 'A':
                {
                    if (InKeyEvent.GetInputEvent() == IE_Pressed)
                    {
                        PC->InputKey(EKeys::A, IE_Pressed);
                    }
                    else if (InKeyEvent.GetInputEvent() == IE_Released)
                    {
                        PC->InputKey(EKeys::A, IE_Released);
                    }
                    break;
                }
                case 'D':
                {
                    if (InKeyEvent.GetInputEvent() == IE_Pressed)
                    {
                        PC->InputKey(EKeys::D, IE_Pressed);
                    }
                    else if (InKeyEvent.GetInputEvent() == IE_Released)
                    {
                        PC->InputKey(EKeys::D, IE_Released);
                    }
                    break;
                }
                case 'W':
                {
                    if (InKeyEvent.GetInputEvent() == IE_Pressed)
                    {
                        PC->InputKey(EKeys::W, IE_Pressed);
                    }
                    else if (InKeyEvent.GetInputEvent() == IE_Released)
                    {
                        PC->InputKey(EKeys::W, IE_Released);
                    }
                    break;
                }
                case 'S':
                {
                    if (InKeyEvent.GetInputEvent() == IE_Pressed)
                    {
                        PC->InputKey(EKeys::S, IE_Pressed);
                    }
                    else if (InKeyEvent.GetInputEvent() == IE_Released)
                    {
                        PC->InputKey(EKeys::S, IE_Released);
                    }
                    break;
                }
                }
            }
        }
    }
    // 에디터 모드
    else
    {
        // TODO: 나중에 InKeyEvent.GetKey();로 가져오는걸로 수정하기
        // 마우스 우클릭이 되었을때만 실행되는 함수
        if (GetKeyState(VK_RBUTTON) & 0x8000)
        {
            switch (InKeyEvent.GetCharacter())
            {
            case 'A':
            {
                if (InKeyEvent.GetInputEvent() == IE_Pressed)
                {
                    PressedKeys.Add(EKeys::A);
                }
                else if (InKeyEvent.GetInputEvent() == IE_Released)
                {
                    PressedKeys.Remove(EKeys::A);
                }
                break;
            }
            case 'D':
            {
                if (InKeyEvent.GetInputEvent() == IE_Pressed)
                {
                    PressedKeys.Add(EKeys::D);
                }
                else if (InKeyEvent.GetInputEvent() == IE_Released)
                {
                    PressedKeys.Remove(EKeys::D);
                }
                break;
            }
            case 'W':
            {
                if (InKeyEvent.GetInputEvent() == IE_Pressed)
                {
                    PressedKeys.Add(EKeys::W);
                }
                else if (InKeyEvent.GetInputEvent() == IE_Released)
                {
                    PressedKeys.Remove(EKeys::W);
                }
                break;
            }
            case 'S':
            {
                if (InKeyEvent.GetInputEvent() == IE_Pressed)
                {
                    PressedKeys.Add(EKeys::S);
                }
                else if (InKeyEvent.GetInputEvent() == IE_Released)
                {
                    PressedKeys.Remove(EKeys::S);
                }
                break;
            }
            case 'E':
            {
                if (InKeyEvent.GetInputEvent() == IE_Pressed)
                {
                    PressedKeys.Add(EKeys::E);
                }
                else if (InKeyEvent.GetInputEvent() == IE_Released)
                {
                    PressedKeys.Remove(EKeys::E);
                }
                break;
            }
            case 'Q':
            {
                if (InKeyEvent.GetInputEvent() == IE_Pressed)
                {
                    PressedKeys.Add(EKeys::Q);
                }
                else if (InKeyEvent.GetInputEvent() == IE_Released)
                {
                    PressedKeys.Remove(EKeys::Q);
                }
                break;
            }
            default:
                break;
            }
        }
        else
        {
            UEditorPlayer* EdPlayer = CastChecked<UEditorEngine>(GEngine)->GetEditorPlayer();
            switch (InKeyEvent.GetCharacter())
            {
            case 'W':
            {
                EdPlayer->SetMode(CM_TRANSLATION);
                break;
            }
            case 'E':
            {
                EdPlayer->SetMode(CM_ROTATION);
                break;
            }
            case 'R':
            {
                EdPlayer->SetMode(CM_SCALE);
                break;
            }
            default:
                break;
            }
            PressedKeys.Empty();
        }


        // 일반적인 단일 키 이벤트
        if (InKeyEvent.GetInputEvent() == IE_Pressed)
        {
            switch (InKeyEvent.GetCharacter())
            {
            case 'F':
            {
                UEditorEngine* Engine = Cast<UEditorEngine>(GEngine);
                USceneComponent* SelectedComponent = Engine->GetSelectedComponent();
                AActor* SelectedActor = Engine->GetSelectedActor();

                USceneComponent* TargetComponent = nullptr;

                if (SelectedComponent != nullptr)
                {
                    TargetComponent = SelectedComponent;
                }
                else if (SelectedActor != nullptr)
                {
                    TargetComponent = SelectedActor->GetRootComponent();
                }

                if (UPrimitiveComponent* primitive = Cast<UPrimitiveComponent>(TargetComponent))
                {
                    FViewportCamera& ViewTransform = PerspectiveCamera;
                    float fov = FieldOfView;

                    // AABB 중심 및 크기
                    FBoundingBox Box = primitive->GetBoundingBox();
                    FVector Center = (Box.min + Box.max) * 0.5f;
                    FVector Extents = (Box.max - Box.min) * 0.5f;
                    float Radius = Extents.Length();

                    // FOV 기반 카메라 거리 계산
                    float VerticalFOV = FMath::DegreesToRadians(fov);
                    float Distance = Radius / FMath::Tan(VerticalFOV * 0.5f);

                    // 위치 이동
                    ViewTransform.SetLocation(Center - ViewTransform.GetForwardVector() * Distance);
                }

                break;
            }
            case 'M':
            {
                FEngineLoop::GraphicDevice.Resize(GEngineLoop.AppWnd);
                SLevelEditor* LevelEd = GEngineLoop.GetLevelEditor();
                LevelEd->SetEnableMultiViewport(!LevelEd->IsMultiViewport());
                break;
            }
            default:
                break;
            }

            // Virtual Key
            UEditorEngine* EdEngine = CastChecked<UEditorEngine>(GEngine);
            switch (InKeyEvent.GetKeyCode())
            {
            case VK_DELETE:
            {
                UEditorEngine* Engine = Cast<UEditorEngine>(GEngine);
                if (Engine)
                {
                    USceneComponent* SelectedComponent = Engine->GetSelectedComponent();
                    AActor* SelectedActor = Engine->GetSelectedActor();

                    if (SelectedComponent)
                    {
                        AActor* Owner = SelectedComponent->GetOwner();

                        if (Owner && Owner->GetRootComponent() != SelectedComponent)
                        {
                            UE_LOG(LogLevel::Display, "Delete Component - %s", *SelectedComponent->GetName());
                            Engine->DeselectComponent(SelectedComponent);
                            SelectedComponent->DestroyComponent();
                        }
                        else if (SelectedActor)
                        {
                            UE_LOG(LogLevel::Display, "Delete Component - %s", *SelectedActor->GetName());
                            SelectedActor->Destroy();
                        }
                    }
                    else if (SelectedActor)
                    {
                        UE_LOG(LogLevel::Display, "Delete Component - %s", *SelectedActor->GetName());
                        SelectedActor->Destroy();
                    }
                }
                break;
            }
            case VK_SPACE:
            {
                EdEngine->GetEditorPlayer()->SetControlMode();
                break;
            }
            default:
                break;
            }
        }
        return;
    }
}

void FEditorViewportClient::MouseMove(const FPointerEvent& InMouseEvent)
{
    const auto& [DeltaX, DeltaY] = InMouseEvent.GetCursorDelta();

    // Yaw(좌우 회전) 및 Pitch(상하 회전) 값 변경
    if (IsPerspective())
    {
        CameraRotateYaw(DeltaX * 0.1f);  // X 이동에 따라 좌우 회전
        CameraRotatePitch(DeltaY * 0.1f);  // Y 이동에 따라 상하 회전
    }
    else
    {
        PivotMoveRight(DeltaX);
        PivotMoveUp(DeltaY);
    }
}

void FEditorViewportClient::ResizeViewport(FRect Top, FRect Bottom, FRect Left, FRect Right)
{
    if (Viewport)
    {
        Viewport->ResizeViewport(Top, Bottom, Left, Right);
    }
    else
    {
        UE_LOG(LogLevel::Error, "Viewport is nullptr");
    }
    UpdateProjectionMatrix();
    UpdateViewMatrix();
}

bool FEditorViewportClient::IsSelected(const FVector2D& InPoint) const
{
    return GetViewport()->bIsHovered(InPoint);
}

void FEditorViewportClient::DeprojectFVector2D(const FVector2D& ScreenPos, FVector& OutWorldOrigin, FVector& OutWorldDir) const
{
    const float TopLeftX = Viewport->GetD3DViewport().TopLeftX;
    const float TopLeftY = Viewport->GetD3DViewport().TopLeftY;
    const float Width = Viewport->GetD3DViewport().Width;
    const float Height = Viewport->GetD3DViewport().Height;

    // 뷰포트가 유효한 위치에 있는지?
    assert(0.0f <= Width && 0.0f <= Height);

    // 뷰포트의 NDC 위치
    const FVector2D NDCPos = {
        ((ScreenPos.X - TopLeftX) / Width * 2.0f) - 1.0f,
        1.0f - ((ScreenPos.Y - TopLeftY) / Height * 2.0f)
    };

    FVector RayOrigin = { NDCPos.X, NDCPos.Y, 0.0f};
    FVector RayEnd = { NDCPos.X, NDCPos.Y, 1.0f};

    // 스크린 좌표계에서 월드 좌표계로 변환
    const FMatrix InvProjView = FMatrix::Inverse(Projection) * FMatrix::Inverse(View);
    RayOrigin = InvProjView.TransformPosition(RayOrigin);
    RayEnd = InvProjView.TransformPosition(RayEnd);

    OutWorldOrigin = RayOrigin;
    OutWorldDir = (RayEnd - RayOrigin).GetSafeNormal();
}

void FEditorViewportClient::CameraMoveForward(const float InValue)
{
    if (IsPerspective())
    {
        FVector CurCameraLoc = PerspectiveCamera.GetLocation();
        CurCameraLoc = CurCameraLoc + PerspectiveCamera.GetForwardVector() * GetCameraSpeedScalar() * InValue;
        PerspectiveCamera.SetLocation(CurCameraLoc);
    }
    else
    {
        OrthoPivot.X += InValue * 0.1f;
    }
}

void FEditorViewportClient::CameraMoveRight(const float InValue)
{
    if (IsPerspective())
    {
        FVector CurCameraLoc = PerspectiveCamera.GetLocation();
        CurCameraLoc = CurCameraLoc + PerspectiveCamera.GetRightVector() * GetCameraSpeedScalar() * InValue;
        PerspectiveCamera.SetLocation(CurCameraLoc);
    }
    else
    {
        OrthoPivot.Y += InValue * 0.1f;
    }
}

void FEditorViewportClient::CameraMoveUp(const float InValue)
{
    if (IsPerspective())
    {
        FVector CurCameraLoc = PerspectiveCamera.GetLocation();
        CurCameraLoc.Z = CurCameraLoc.Z + GetCameraSpeedScalar() * InValue;
        PerspectiveCamera.SetLocation(CurCameraLoc);
    }
    else
    {
        OrthoPivot.Z += InValue * 0.1f;
    }
}

void FEditorViewportClient::CameraRotateYaw(const float InValue)
{
    FVector CurCameraRot = PerspectiveCamera.GetRotation();
    CurCameraRot.Z += InValue ;
    PerspectiveCamera.SetRotation(CurCameraRot);
}

void FEditorViewportClient::CameraRotatePitch(const float InValue)
{
    FVector CurCameraRot = PerspectiveCamera.GetRotation();
    CurCameraRot.Y = FMath::Clamp(CurCameraRot.Y + InValue, -89.f, 89.f);
    PerspectiveCamera.SetRotation(CurCameraRot);
}

void FEditorViewportClient::PivotMoveRight(const float InValue) const
{
    OrthoPivot = OrthoPivot + OrthogonalCamera.GetRightVector() * InValue * -0.05f;
}

void FEditorViewportClient::PivotMoveUp(const float InValue) const
{
    OrthoPivot = OrthoPivot + OrthogonalCamera.GetUpVector() * InValue * 0.05f;
}

void FEditorViewportClient::LoadConfig(const TMap<FString, FString>& Config)
{
    FString ViewportNum = std::to_string(ViewportIndex);
    CameraSpeedSetting = FEditorConfigManager::GetValueFromConfig(Config, "CameraSpeedSetting" + ViewportNum, 1);
    CameraSpeed = FEditorConfigManager::GetValueFromConfig(Config, "CameraSpeedScalar" + ViewportNum, 1.0f);
    GridSize = FEditorConfigManager::GetValueFromConfig(Config, "GridSize"+ ViewportNum, 10.0f);
    PerspectiveCamera.ViewLocation.X = FEditorConfigManager::GetValueFromConfig(Config, "PerspectiveCameraLocX" + ViewportNum, 0.0f);
    PerspectiveCamera.ViewLocation.Y = FEditorConfigManager::GetValueFromConfig(Config, "PerspectiveCameraLocY" + ViewportNum, 0.0f);
    PerspectiveCamera.ViewLocation.Z = FEditorConfigManager::GetValueFromConfig(Config, "PerspectiveCameraLocZ" + ViewportNum, 0.0f);
    PerspectiveCamera.ViewRotation.X = FEditorConfigManager::GetValueFromConfig(Config, "PerspectiveCameraRotX" + ViewportNum, 0.0f);
    PerspectiveCamera.ViewRotation.Y = FEditorConfigManager::GetValueFromConfig(Config, "PerspectiveCameraRotY" + ViewportNum, 0.0f);
    PerspectiveCamera.ViewRotation.Z = FEditorConfigManager::GetValueFromConfig(Config, "PerspectiveCameraRotZ" + ViewportNum, 0.0f);
    ShowFlag = FEditorConfigManager::GetValueFromConfig(Config, "ShowFlag" + ViewportNum, 63.0f);
    ViewMode = static_cast<EViewModeIndex>(FEditorConfigManager::GetValueFromConfig(Config, "ViewMode" + ViewportNum, 0));
    ViewportType = static_cast<ELevelViewportType>(FEditorConfigManager::GetValueFromConfig(Config, "ViewportType" + ViewportNum, 3));
}

void FEditorViewportClient::SaveConfig(TMap<FString, FString>& Config) const
{
    const FString ViewportNum = std::to_string(ViewportIndex);
    Config["CameraSpeedSetting"+ ViewportNum] = std::to_string(CameraSpeedSetting);
    Config["CameraSpeedScalar"+ ViewportNum] = std::to_string(CameraSpeed);
    Config["GridSize"+ ViewportNum] = std::to_string(GridSize);
    Config["PerspectiveCameraLocX" + ViewportNum] = std::to_string(PerspectiveCamera.GetLocation().X);
    Config["PerspectiveCameraLocY" + ViewportNum] = std::to_string(PerspectiveCamera.GetLocation().Y);
    Config["PerspectiveCameraLocZ" + ViewportNum] = std::to_string(PerspectiveCamera.GetLocation().Z);
    Config["PerspectiveCameraRotX" + ViewportNum] = std::to_string(PerspectiveCamera.GetRotation().X);
    Config["PerspectiveCameraRotY" + ViewportNum] = std::to_string(PerspectiveCamera.GetRotation().Y);
    Config["PerspectiveCameraRotZ" + ViewportNum] = std::to_string(PerspectiveCamera.GetRotation().Z);
    Config["ShowFlag"+ ViewportNum] = std::to_string(ShowFlag);
    Config["ViewMode" + ViewportNum] = std::to_string(static_cast<int32>(ViewMode));
    Config["ViewportType" + ViewportNum] = std::to_string(ViewportType);
}

TMap<FString, FString> FEditorViewportClient::ReadIniFile(const FString& FilePath)
{
    TMap<FString, FString> Config;
    std::ifstream File(*FilePath);
    std::string Line;

    while (std::getline(File, Line))
    {
        if (Line.empty() || Line[0] == '[' || Line[0] == ';')
        {
            continue;
        }
        std::istringstream SS(Line);
        std::string Key, Value;
        if (std::getline(SS, Key, '=') && std::getline(SS, Value))
        {
            Config[Key] = Value;
        }
    }
    return Config;
}

auto FEditorViewportClient::WriteIniFile(const FString& FilePath, const TMap<FString, FString>& Config) -> void
{
    std::ofstream File(*FilePath);
    for (const auto& Pair : Config)
    {
        File << *Pair.Key << "=" << *Pair.Value << "\n";
    }
}

void FEditorViewportClient::SetCameraSpeed(const float InValue)
{
    CameraSpeed = FMath::Clamp(InValue, 0.1f, 200.0f);
}

FVector FViewportCamera::GetForwardVector() const
{
    FVector Forward = FVector(1.f, 0.f, 0.0f);
    Forward = JungleMath::FVectorRotate(Forward, ViewRotation);
    return Forward;
}

FVector FViewportCamera::GetRightVector() const
{
    FVector Right = FVector(0.f, 1.f, 0.0f);
	Right = JungleMath::FVectorRotate(Right, ViewRotation);
	return Right;
}

FVector FViewportCamera::GetUpVector() const
{
    FVector Up = FVector(0.f, 0.f, 1.0f);
    Up = JungleMath::FVectorRotate(Up, ViewRotation);
    return Up;
}
