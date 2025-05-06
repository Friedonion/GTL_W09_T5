#include "ControlEditorPanel.h"

#include "World/World.h"

#include "Actors/Player.h"
#include "Actors/FireballActor.h"

#include "Components/Light/LightComponent.h"
#include "Components/SphereComp.h"
#include "Components/ParticleSubUVComponent.h"
#include "Components/TextComponent.h"

#include "Engine/FLoaderOBJ.h"
#include "LevelEditor/SLevelEditor.h"
#include "UnrealEd/EditorViewportClient.h"
#include "tinyfiledialogs/tinyfiledialogs.h"

#include "Actors/Cube.h"

#include "Engine/EditorEngine.h"
#include <Actors/HeightFogActor.h>

#include "ShowFlag.h"
#include "Actors/PointLightActor.h"
#include "Actors/DirectionalLightActor.h"
#include "Actors/SpotLightActor.h"
#include "Actors/AmbientLightActor.h"
#include "Actors/ASkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Games/LastWar/Core/Spawner.h"
#include "ImGUI/imgui.h"
#include "UObject/Casts.h"
#include "FLoaderFBX.h"

void ControlEditorPanel::Render()
{
    /* Pre Setup */
    const ImGuiIO& IO = ImGui::GetIO();
    ImFont* IconFont = IO.Fonts->Fonts[FEATHER_FONT];
    constexpr ImVec2 IconSize = ImVec2(32, 32);

    const float PanelWidth = (Width) * 0.8f;
    constexpr float PanelHeight = 45.0f;

    constexpr float PanelPosX = 1.0f;
    constexpr float PanelPosY = 1.0f;

    constexpr ImVec2 MinSize(300, 50);
    constexpr ImVec2 MaxSize(FLT_MAX, 50);

    /* Min, Max Size */
    ImGui::SetNextWindowSizeConstraints(MinSize, MaxSize);

    /* Panel Position */
    ImGui::SetNextWindowPos(ImVec2(PanelPosX, PanelPosY), ImGuiCond_Always);

    /* Panel Size */
    ImGui::SetNextWindowSize(ImVec2(PanelWidth, PanelHeight), ImGuiCond_Always);

    /* Panel Flags */
    constexpr ImGuiWindowFlags PanelFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoBackground;

    /* Render Start */
    ImGui::Begin("Control Panel", nullptr, PanelFlags);

    CreateMenuButton(IconSize, IconFont);
    ImGui::SameLine();
    CreateFlagButton();
    ImGui::SameLine();
    CreateModifyButton(IconSize, IconFont);
    ImGui::SameLine();
    ImGui::SameLine();
    CreateLightSpawnButton(IconSize, IconFont);
    ImGui::SameLine();
    {
        ImGui::PushFont(IconFont);
        CreatePIEButton(IconSize, IconFont);
        ImGui::SameLine();
        /* Get Window Content Region */
        const float ContentWidth = ImGui::GetWindowContentRegionMax().x;
        /* Move Cursor X Position */
        ImGui::SetCursorPosX(ContentWidth - (IconSize.x * 3.0f + 16.0f));
        CreateSRTButton(IconSize);
        ImGui::PopFont();
    }

    ImGui::End();
}

void ControlEditorPanel::CreateMenuButton(const ImVec2 ButtonSize, ImFont* IconFont)
{
    ImGui::PushFont(IconFont);
    if (ImGui::Button("\ue9ad", ButtonSize)) // Menu
    {
        bOpenMenu = !bOpenMenu;
    }
    ImGui::PopFont();

    if (bOpenMenu)
    {
        ImGui::SetNextWindowPos(ImVec2(10, 55), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(135, 170), ImGuiCond_Always);

        ImGui::Begin("Menu", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

        if (ImGui::MenuItem("New Level"))
        {
            if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
            {
                EditorEngine->NewLevel();
            }
        }

        if (ImGui::MenuItem("Load Level"))
        {
            char const* lFilterPatterns[1] = { "*.scene" };
            const char* FileName = tinyfd_openFileDialog("Open Scene File", "", 1, lFilterPatterns, "Scene(.scene) file", 0);

            if (FileName == nullptr)
            {
                tinyfd_messageBox("Error", "파일을 불러올 수 없습니다.", "ok", "error", 1);
                ImGui::End();
                return;
            }
            if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
            {
                EditorEngine->NewLevel();
                EditorEngine->LoadLevel(FileName);
            }
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Save Level"))
        {
            char const* lFilterPatterns[1] = { "*.scene" };
            const char* FileName = tinyfd_saveFileDialog("Save Scene File", "", 1, lFilterPatterns, "Scene(.scene) file");

            if (FileName == nullptr)
            {
                ImGui::End();
                return;
            }
            if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
            {
                EditorEngine->SaveLevel(FileName);
                EditorEngine->NewLevel();
                EditorEngine->LoadLevel(FileName);
            }

            tinyfd_messageBox("알림", "저장되었습니다.", "ok", "info", 1);
        }

        ImGui::Separator();

        if (ImGui::Button("ImGui데모"))
        {
            bShowImGuiDemoWindow = !bShowImGuiDemoWindow;
        }

        if (bShowImGuiDemoWindow)
        {
            ImGui::ShowDemoWindow(&bShowImGuiDemoWindow); // 창이 닫힐 때 상태를 업데이트
        }

        ImGui::Separator();

        if (ImGui::BeginMenu("Import"))
        {
            if (ImGui::MenuItem("Wavefront (.obj)"))
            {
                char const* lFilterPatterns[1] = { "*.obj" };
                const char* FileName = tinyfd_openFileDialog("Open OBJ File", "", 1, lFilterPatterns, "Wavefront(.obj) file", 0);

                if (FileName != nullptr)
                {
                    std::cout << FileName << '\n';

                    if (FManagerOBJ::CreateStaticMesh(FileName) == nullptr)
                    {
                        tinyfd_messageBox("Error", "파일을 불러올 수 없습니다.", "ok", "error", 1);
                    }
                }
            }

            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Quit"))
        {
            ImGui::OpenPopup("프로그램 종료");
        }

        const ImVec2 Center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(Center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        if (ImGui::BeginPopupModal("프로그램 종료", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("정말 프로그램을 종료하시겠습니까?");

            ImGui::Separator();

            const float ContentWidth = ImGui::GetWindowContentRegionMax().x;
            /* Move Cursor X Position */
            ImGui::SetCursorPosX(ContentWidth - (160.f + 10.0f));
            if (ImGui::Button("OK", ImVec2(80, 0)))
            {
                PostQuitMessage(0);
            }
            ImGui::SameLine();
            ImGui::SetItemDefaultFocus();
            ImGui::PushID("CancelButtonWithQuitWindow");
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(ImColor::HSV(0.0f, 1.0f, 1.0f)));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(ImColor::HSV(0.0f, 0.9f, 1.0f)));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(ImColor::HSV(0.0f, 1.0f, 1.0f)));
            if (ImGui::Button("Cancel", ImVec2(80, 0)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(3);
            ImGui::PopID();

            ImGui::EndPopup();
        }

        ImGui::End();
    }
}

void ControlEditorPanel::CreateModifyButton(const ImVec2 ButtonSize, ImFont* IconFont)
{
    ImGui::PushFont(IconFont);
    if (ImGui::Button("\ue9c4", ButtonSize)) // Slider
    {
        ImGui::OpenPopup("SliderControl");
    }
    ImGui::PopFont();

    std::shared_ptr<FEditorViewportClient> ViewportClient = GEngineLoop.GetLevelEditor()->GetActiveViewportClient();

    if (ImGui::BeginPopup("SliderControl"))
    {
        ImGui::Text("Grid Scale");
        float GridScale = ViewportClient->GetGridSize();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::DragFloat("##Grid Scale", &GridScale, 0.1f, 1.0f, 20.0f, "%.1f"))
        {
            ViewportClient->SetGridSize(GridScale);
        }

        ImGui::Separator();

        ImGui::Text("Camera FOV");
        float FOV = ViewportClient->GetFieldOfView();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::DragFloat("##Fov", &FOV, 0.1f, 30.0f, 120.0f, "%.1f"))
        {
            ViewportClient->SetFieldOfView(FOV);
        }

        ImGui::Spacing();

        ImGui::Text("Camera Speed");
        float CameraSpeed = ViewportClient->GetCameraSpeedScalar();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::DragFloat("##CamSpeed", &CameraSpeed, 0.1f, 0.198f, 192.0f, "%.1f"))
        {
            ViewportClient->SetCameraSpeed(CameraSpeed);
        }

        ImGui::EndPopup();
    }

    ImGui::SameLine();

    // @todo 적절한 이름으로 변경 바람
    ImGui::PushFont(IconFont);
    if (ImGui::Button("\ue9c8", ButtonSize))
    {
        ImGui::OpenPopup("PrimitiveControl");
    }
    ImGui::PopFont();

    if (ImGui::BeginPopup("PrimitiveControl"))
    {
        struct Primitive
        {
            const char* Label;
            int OBJ;
        };

        static const Primitive primitives[] = 
        {
            { .Label= "Cube",      .OBJ= OBJ_CUBE },
            { .Label= "Sphere",    .OBJ= OBJ_SPHERE },
            { .Label= "PointLight", .OBJ= OBJ_POINTLIGHT },
            { .Label= "SpotLight", .OBJ= OBJ_SPOTLIGHT },
            { .Label= "DirectionalLight", .OBJ= OBJ_DIRECTIONALLGIHT },
            { .Label= "AmbientLight", .OBJ= OBJ_AMBIENTLIGHT },
            { .Label= "Particle",  .OBJ= OBJ_PARTICLE },
            { .Label= "Text",      .OBJ= OBJ_TEXT },
            { .Label= "Fireball",  .OBJ = OBJ_FIREBALL},
            { .Label= "Fog",       .OBJ= OBJ_FOG },
            { .Label = "Spawner",  .OBJ = OBJ_Spawner},
            { .Label= "Mutant",    .OBJ= OBJ_Mutant }
        };

        for (const auto& primitive : primitives)
        {
            if (ImGui::Selectable(primitive.Label))
            {
                UWorld* World = GEngine->ActiveWorld;
                AActor* SpawnedActor = nullptr;
                switch (static_cast<OBJECTS>(primitive.OBJ))
                {
                case OBJ_SPHERE:
                {
                    SpawnedActor = World->SpawnActor<AActor>();
                    SpawnedActor->SetActorLabel(TEXT("OBJ_SPHERE"));
                    USphereComp* SphereComp = SpawnedActor->AddComponent<USphereComp>();
                    SphereComp->SetStaticMesh(FManagerOBJ::GetStaticMesh(L"Contents/Sphere.obj"));
                    break;
                }
                case OBJ_CUBE:
                {
                    // TODO: 다른 부분들 전부 Actor만 소환하도록 하고, Component 생성은 Actor가 자체적으로 하도록 변경.
                    ACube* CubeActor = World->SpawnActor<ACube>();
                    CubeActor->SetActorLabel(TEXT("OBJ_CUBE"));
                    break;
                }

                case OBJ_SPOTLIGHT:
                {
                    ASpotLight* SpotActor = World->SpawnActor<ASpotLight>();
                    SpotActor->SetActorLabel(TEXT("OBJ_SPOTLIGHT"));
                    break;
                }
                case OBJ_POINTLIGHT:
                {
                    APointLight* LightActor = World->SpawnActor<APointLight>();
                    LightActor->SetActorLabel(TEXT("OBJ_POINTLIGHT"));
                    break;
                }
                case OBJ_DIRECTIONALLGIHT:
                {
                    ADirectionalLight* LightActor = World->SpawnActor<ADirectionalLight>();
                    LightActor->SetActorLabel(TEXT("OBJ_DIRECTIONALLGIHT"));
                    break;
                }
                case OBJ_AMBIENTLIGHT:
                {
                    AAmbientLight* LightActor = World->SpawnActor<AAmbientLight>();
                    LightActor->SetActorLabel(TEXT("OBJ_AMBIENTLIGHT"));
                    break;
                }
                case OBJ_PARTICLE:
                {
                    SpawnedActor = World->SpawnActor<AActor>();
                    SpawnedActor->SetActorLabel(TEXT("OBJ_PARTICLE"));
                    UParticleSubUVComponent* ParticleComponent = SpawnedActor->AddComponent<UParticleSubUVComponent>();
                    ParticleComponent->SetTexture(L"Assets/Texture/T_Explosion_SubUV.png");



                    ParticleComponent->SetRowColumnCount(6, 6);
                    ParticleComponent->SetRelativeScale3D(FVector(10.0f, 10.0f, 1.0f));
                    ParticleComponent->Activate();
                    SpawnedActor->SetActorTickInEditor(true);
                    break;
                }
                case OBJ_TEXT:
                {
                    SpawnedActor = World->SpawnActor<AActor>();
                    SpawnedActor->SetActorLabel(TEXT("OBJ_TEXT"));
                    UTextComponent* TextComponent = SpawnedActor->AddComponent<UTextComponent>();
                    TextComponent->SetTexture(L"Assets/Texture/font.png");
                    TextComponent->SetRowColumnCount(106, 106);
                    TextComponent->SetText(L"안녕하세요 Jungle 1");

                    break;
                }
                case OBJ_FIREBALL:
                {
                    SpawnedActor = World->SpawnActor<AFireballActor>();
                    SpawnedActor->SetActorLabel(TEXT("OBJ_FIREBALL"));

                    break;
                }
                case OBJ_FOG:
                {
                    SpawnedActor = World->SpawnActor<AHeightFogActor>();
                    SpawnedActor->SetActorLabel(TEXT("OBJ_FOG"));
                    break;
                }
                case OBJ_Spawner:
                {
                    SpawnedActor = World->SpawnActor<ASpawnerActor>();
                    SpawnedActor->SetActorLabel(TEXT("OBJ_Spawner"));
                    break;
                }
                case OBJ_Mutant:
                {
                    SpawnedActor = World->SpawnActor<ASkeletalMeshActor>();
                    Cast<ASkeletalMeshActor>(SpawnedActor)->GetSkeletalMeshComponent()->SetSkeletalMesh(FManagerFBX::GetSkeletalMesh(L"Contents/Mutant.fbx"));
                    SpawnedActor->SetActorLabel(TEXT("OBJ_Mutant"));
                }
                case OBJ_TRIANGLE:
                case OBJ_CAMERA:
                case OBJ_PLAYER:
                case OBJ_END:
                    break;
                }

                if (SpawnedActor)
                {
                    UEditorEngine* Engine = Cast<UEditorEngine>(GEngine);
                    Engine->DeselectComponent(Engine->GetSelectedComponent());
                    Engine->SelectActor(SpawnedActor);
                }
            }
        }
        ImGui::EndPopup();
    }
}

void ControlEditorPanel::CreateFlagButton()
{
    const std::shared_ptr<FEditorViewportClient> ActiveViewport = GEngineLoop.GetLevelEditor()->GetActiveViewportClient();

    const char* ViewTypeNames[] = { "Perspective", "Top", "Bottom", "Left", "Right", "Front", "Back" };
    const ELevelViewportType ActiveViewType = ActiveViewport->GetViewportType();
    FString TextViewType = ViewTypeNames[ActiveViewType];

    if (ImGui::Button(GetData(TextViewType), ImVec2(120, 32)))
    {
        // toggleViewState = !toggleViewState;
        ImGui::OpenPopup("ViewControl");
    }

    if (ImGui::BeginPopup("ViewControl"))
    {
        for (int i = 0; i < IM_ARRAYSIZE(ViewTypeNames); i++)
        {
            bool bIsSelected = (static_cast<int>(ActiveViewport->GetViewportType()) == i);
            if (ImGui::Selectable(ViewTypeNames[i], bIsSelected))
            {
                ActiveViewport->SetViewportType(static_cast<ELevelViewportType>(i));
            }

            if (bIsSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    const char* ViewModeNames[] = { 
        "Lit_Gouraud", "Lit_Lambert", "Lit_Phong", 
        "Unlit", "Wireframe",
        "Scene Depth", "World Normal", "Light Heat Map"
    };
    constexpr uint32 ViewModeCount = std::size(ViewModeNames);
    const int RawViewMode = static_cast<int>(ActiveViewport->GetViewMode());
    const int SafeIndex = (RawViewMode >= 0) ? (RawViewMode % ViewModeCount) : 0;
    FString ViewModeControl = ViewModeNames[SafeIndex];
    const ImVec2 ViewModeTextSize = ImGui::CalcTextSize(GetData(ViewModeControl));
    if (ImGui::Button(GetData(ViewModeControl), ImVec2(30 + ViewModeTextSize.x, 32)))
    {
        ImGui::OpenPopup("ViewModeControl");
    }

    if (ImGui::BeginPopup("ViewModeControl"))
    {
        for (int i = 1; i < IM_ARRAYSIZE(ViewModeNames); i++)
        {
            bool bIsSelected = (static_cast<int>(ActiveViewport->GetViewMode()) == i);
            if (ImGui::Selectable(ViewModeNames[i], bIsSelected))
            {
                ActiveViewport->SetViewMode(static_cast<EViewModeIndex>(i));
            }

            if (bIsSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();


    if (ImGui::Button("Show", ImVec2(60, 32)))
    {
        ImGui::OpenPopup("ShowFlags");
    }

    const char* Items[] = { "AABB", "Primitives", "BillBoardText", "UUID", "Fog", "LightWireframe", "LightWireframeSelectedOnly", "Shadow", "Collision", "CollisionSelectedOnly" };
    const uint64 CurFlag = ActiveViewport->GetShowFlag();

    if (ImGui::BeginPopup("ShowFlags"))
    {
        bool Selected[IM_ARRAYSIZE(Items)] = 
        {
            static_cast<bool>(CurFlag & EEngineShowFlags::SF_AABB),
            static_cast<bool>(CurFlag & EEngineShowFlags::SF_Primitives),
            static_cast<bool>(CurFlag & EEngineShowFlags::SF_BillboardText),
            static_cast<bool>(CurFlag & EEngineShowFlags::SF_UUIDText),
            static_cast<bool>(CurFlag & EEngineShowFlags::SF_Fog),
            static_cast<bool>(CurFlag & EEngineShowFlags::SF_LightWireframe),
            static_cast<bool>(CurFlag & EEngineShowFlags::SF_LightWireframeSelectedOnly),
            static_cast<bool>(CurFlag & EEngineShowFlags::SF_Shadow),
            static_cast<bool>(CurFlag & EEngineShowFlags::SF_Collision),
            static_cast<bool>(CurFlag & EEngineShowFlags::SF_CollisionSelectedOnly),
        }; // 각 항목의 체크 상태 저장

        for (int i = 0; i < IM_ARRAYSIZE(Items); i++)
        {
            ImGui::Checkbox(Items[i], &Selected[i]);
        }

        ActiveViewport->SetShowFlag(ConvertSelectionToFlags(Selected));
        ImGui::EndPopup();
    }
}

void ControlEditorPanel::CreatePIEButton(const ImVec2 ButtonSize, ImFont* IconFont)
{
    UEditorEngine* Engine = Cast<UEditorEngine>(GEngine);
    if (!Engine)
    {
        return;
    }

    const ImVec2 WindowSize = ImGui::GetIO().DisplaySize;

    const float CenterX = (WindowSize.x - ButtonSize.x) / 2.5f;

    ImGui::SetCursorScreenPos(ImVec2(CenterX - 40.0f, 10.0f));
    
    if (ImGui::Button("\ue9a8", ButtonSize)) // Play
    {
        UE_LOG(LogLevel::Display, TEXT("PIE Button Clicked"));
        Engine->StartPIE();
    }

    ImGui::SetCursorScreenPos(ImVec2(CenterX - 10.0f, 10.0f));
    if (ImGui::Button("\ue9e4", ButtonSize)) // Stop
    {
        UE_LOG(LogLevel::Display, TEXT("Stop Button Clicked"));
        Engine->EndPIE();
    }
}

// code is so dirty / Please refactor
void ControlEditorPanel::CreateSRTButton(ImVec2 ButtonSize)
{
    const UEditorEngine* Engine = Cast<UEditorEngine>(GEngine);
    UEditorPlayer* Player = Engine->GetEditorPlayer();

    constexpr ImVec4 ActiveColor = ImVec4(0.00f, 0.00f, 0.85f, 1.0f);

    const EControlMode ControlMode = Player->GetControlMode();

    if (ControlMode == CM_TRANSLATION)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ActiveColor);
    }
    if (ImGui::Button("\ue9bc", ButtonSize)) // Move
    {
        Player->SetMode(CM_TRANSLATION);
    }
    if (ControlMode == CM_TRANSLATION)
    {
        ImGui::PopStyleColor();
    }
    ImGui::SameLine();
    if (ControlMode == CM_ROTATION)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ActiveColor);
    }
    if (ImGui::Button("\ue9d3", ButtonSize)) // Rotate
    {
        Player->SetMode(CM_ROTATION);
    }
    if (ControlMode == CM_ROTATION)
    {
        ImGui::PopStyleColor();
    }
    ImGui::SameLine();
    if (ControlMode == CM_SCALE)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ActiveColor);
    }
    if (ImGui::Button("\ue9ab", ButtonSize)) // Scale
    {
        Player->SetMode(CM_SCALE);
    }
    if (ControlMode == CM_SCALE)
    {
        ImGui::PopStyleColor();
    }
}

void ControlEditorPanel::OnResize(const HWND hWnd)
{
    RECT ClientRect;
    GetClientRect(hWnd, &ClientRect);
    Width = ClientRect.right - ClientRect.left;
    Height = ClientRect.bottom - ClientRect.top;
}

void ControlEditorPanel::CreateLightSpawnButton(const ImVec2 InButtonSize, ImFont* IconFont)
{
    UWorld* World = GEngine->ActiveWorld;
    const ImVec2 WindowSize = ImGui::GetIO().DisplaySize;

    const float CenterX = (WindowSize.x - InButtonSize.x) / 2.5f;

    ImGui::SetCursorScreenPos(ImVec2(CenterX + 40.0f, 10.0f));
    const char* Text = "Light";
    const ImVec2 TextSize = ImGui::CalcTextSize(Text);
    const ImVec2 Padding = ImGui::GetStyle().FramePadding;
    ImVec2 ButtonSize = ImVec2(
        TextSize.x + Padding.x * 2.0f,
        TextSize.y + Padding.y * 2.0f
    );
    ButtonSize.y = InButtonSize.y;
    if (ImGui::Button("Light", ButtonSize))
    {
        ImGui::OpenPopup("LightGeneratorControl");
    }

    if (ImGui::BeginPopup("LightGeneratorControl"))
    {
        struct LightGeneratorMode
        {
            const char* Label;
            int Mode;
        };

        static constexpr LightGeneratorMode modes[] = 
        {
            {.Label = "Generate", .Mode = ELightGridGenerator::Generate },
            {.Label = "Delete", .Mode = ELightGridGenerator::Delete },
            {.Label = "Reset", .Mode = ELightGridGenerator::Reset },
        };

        for (const auto& [Label, Mode] : modes)
        {
            if (ImGui::Selectable(Label))
            {
                switch (Mode)
                {
                case ELightGridGenerator::Generate:
                    LightGridGenerator.GenerateLight(World);
                    break;
                case ELightGridGenerator::Delete:
                    LightGridGenerator.DeleteLight(World);
                    break;
                case ELightGridGenerator::Reset:
                    LightGridGenerator.Reset(World);
                    break;
                }
            }
        }

        ImGui::EndPopup();
    }
}

uint64 ControlEditorPanel::ConvertSelectionToFlags(const bool Selected[])
{
    uint64 Flags = EEngineShowFlags::None;

    if (Selected[0])
    {
        Flags |= static_cast<uint64>(EEngineShowFlags::SF_AABB);
    }
    if (Selected[1])
    {
        Flags |= static_cast<uint64>(EEngineShowFlags::SF_Primitives);
    }
    if (Selected[2])
    {
        Flags |= static_cast<uint64>(EEngineShowFlags::SF_BillboardText);
    }
    if (Selected[3])
    {
        Flags |= static_cast<uint64>(EEngineShowFlags::SF_UUIDText);
    }
    if (Selected[4])
    {
        Flags |= static_cast<uint64>(EEngineShowFlags::SF_Fog);
    }
    if (Selected[5])
    {
        Flags |= static_cast<uint64>(EEngineShowFlags::SF_LightWireframe);
    }
    if (Selected[6])
    {
        // Need SF_LightWireframe to Draw
        Flags |= static_cast<uint64>(EEngineShowFlags::SF_LightWireframe);
        Flags |= static_cast<uint64>(EEngineShowFlags::SF_LightWireframeSelectedOnly);
    }
    if (Selected[7])
    {
        Flags |= static_cast<uint64>(EEngineShowFlags::SF_Shadow);
    }

    if (Selected[8])
    {
        Flags |= static_cast<uint64>(EEngineShowFlags::SF_Collision);
    }
    
    if (Selected[9])
    {
        Flags |= static_cast<uint64>(EEngineShowFlags::SF_Collision);
        Flags |= static_cast<uint64>(EEngineShowFlags::SF_CollisionSelectedOnly);
    }

    return Flags;
}
