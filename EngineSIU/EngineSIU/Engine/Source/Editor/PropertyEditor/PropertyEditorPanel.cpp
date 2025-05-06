#include "PropertyEditorPanel.h"

#include "ImGUI/Bezier.h"
#include "ImGUI/Curve.h"

#include <filesystem>
#include <shellapi.h>

#include "Math/JungleMath.h"

#include "World/World.h"
#include "Actors/Player.h"
#include "Camera/CameraComponent.h"

#include "Engine/EditorEngine.h"
#include "Engine/FLoaderOBJ.h"
#include "Engine/AssetManager.h"

#include "UnrealEd/ImGuiWidget.h"
#include "UnrealEd/EditorViewportClient.h"
#include "LevelEditor/SLevelEditor.h"
#include "UObject/UObjectIterator.h"
#include "Renderer/ShadowManager.h"

#include "Components/Light/PointLightComponent.h"
#include "Components/Light/SpotLightComponent.h"
#include "Components/Light/DirectionalLightComponent.h"
#include "Components/Light/AmbientLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextComponent.h"
#include "Components/HeightFogComponent.h"
#include "Components/ProjectileMovementComponent.h"
#include "Components/SpringArmComponent.h"
#include "Components/Shapes/BoxComponent.h"
#include "Components/Shapes/CapsuleComponent.h"
#include "Components/Shapes/SphereComponent.h"
#include "Engine/CurveManager.h"

#include "Components/SkeletalMeshComponent.h"
#include "FLoaderFBX.h"

void PropertyEditorPanel::Render()
{
    UEditorEngine* Engine = Cast<UEditorEngine>(GEngine);
    if (!Engine)
    {
        return;
    }
    
    /* Pre Setup */
    float PanelWidth = (Width) * 0.2f - 6.0f;
    float PanelHeight = (Height) * 0.65f;

    float PanelPosX = (Width) * 0.8f + 5.0f;
    float PanelPosY = (Height) * 0.3f + 15.0f;

    ImVec2 MinSize(140, 370);
    ImVec2 MaxSize(FLT_MAX, 900);

    /* Min, Max Size */
    ImGui::SetNextWindowSizeConstraints(MinSize, MaxSize);

    /* Panel Position */
    ImGui::SetNextWindowPos(ImVec2(PanelPosX, PanelPosY), ImGuiCond_Always);

    /* Panel Size */
    ImGui::SetNextWindowSize(ImVec2(PanelWidth, PanelHeight), ImGuiCond_Always);

    /* Panel Flags */
    ImGuiWindowFlags PanelFlags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;

    /* Render Start */
    ImGui::Begin("Detail", nullptr, PanelFlags);
    
    UEditorPlayer* Player = Engine->GetEditorPlayer();
    AActor* SelectedActor = Engine->GetSelectedActor();
    USceneComponent* SelectedComponent = Engine->GetSelectedComponent();
    USceneComponent* TargetComponent = nullptr;

    if (SelectedComponent != nullptr)
    {
        TargetComponent = SelectedComponent;
    }
    else if (SelectedActor != nullptr)
    {        
        TargetComponent = SelectedActor->GetRootComponent();
    }
         
    if (TargetComponent != nullptr)
    {
        RenderForSceneComponent(TargetComponent, Player);
    }
    if (SelectedActor)
    {
        RenderForActor(SelectedActor, TargetComponent);
    }

    if (ULightComponentBase* LightComponent = GetTargetComponent<ULightComponentBase>(SelectedActor, SelectedComponent))
    {
        if (!LightComponent->IsA<UAmbientLightComponent>()) // AmbientLight 는 그림자 관련 X.
        {
            RenderForLightShadowCommon(LightComponent);
        }
    }

    if (UAmbientLightComponent* LightComponent = GetTargetComponent<UAmbientLightComponent>(SelectedActor, SelectedComponent))
    {
        RenderForAmbientLightComponent(LightComponent);
    }

    if (UDirectionalLightComponent* LightComponent = GetTargetComponent<UDirectionalLightComponent>(SelectedActor, SelectedComponent))
    {
        RenderForDirectionalLightComponent(LightComponent);
    }

    if (UPointLightComponent* LightComponent = GetTargetComponent<UPointLightComponent>(SelectedActor, SelectedComponent))
    {
        RenderForPointLightComponent(LightComponent);
    }

    if (USpotLightComponent* LightComponent = GetTargetComponent<USpotLightComponent>(SelectedActor, SelectedComponent))
    {
        RenderForSpotLightComponent(LightComponent);
    }
    
    if (UProjectileMovementComponent* ProjectileComp = GetTargetComponent<UProjectileMovementComponent>(SelectedActor, SelectedComponent))
    {
        RenderForProjectileMovementComponent(ProjectileComp);
    }
    
    if (UTextComponent* TextComp = GetTargetComponent<UTextComponent>(SelectedActor, SelectedComponent))
    {
        RenderForTextComponent(TextComp);
    }

    if (UStaticMeshComponent* StaticMeshComponent = GetTargetComponent<UStaticMeshComponent>(SelectedActor, SelectedComponent))
    {
        RenderForStaticMesh(StaticMeshComponent);
        RenderForMaterial(StaticMeshComponent);
    }

    if (UHeightFogComponent* FogComponent = GetTargetComponent<UHeightFogComponent>(SelectedActor, SelectedComponent))
    {
        RenderForExponentialHeightFogComponent(FogComponent);
    }

    if (UShapeComponent* ShapeComponent = GetTargetComponent<UShapeComponent>(SelectedActor, SelectedComponent))
    {
        RenderForShapeComponent(ShapeComponent);
    }

    if (USpringArmComponent* SpringArmComponent = GetTargetComponent<USpringArmComponent>(SelectedActor, SelectedComponent))
    {
        RenderForSpringArmComponent(SpringArmComponent);
    }

    if (UCameraComponent* CameraComponent = GetTargetComponent<UCameraComponent>(SelectedActor, SelectedComponent))
    {
        RenderForCameraComponent(CameraComponent);
    }

    if (USkinnedMeshComponent* SkinnedMeshComponent = GetTargetComponent<USkinnedMeshComponent>(SelectedActor, SelectedComponent))
    {
        RenderForSkeletalMeshComponent(SkinnedMeshComponent);
    }


    ImGui::End();
}

void PropertyEditorPanel::RGBToHSV(const float R, const float G, const float B, float& H, float& S, float& V)
{
    const float MX = FMath::Max(R, FMath::Max(G, B));
    const float MN = FMath::Min(R, FMath::Min(G, B));
    const float Delta = MX - MN;

    V = MX;

    if (MX == 0.0f) {
        S = 0.0f;
        H = 0.0f;
        return;
    }
    else {
        S = Delta / MX;
    }

    if (Delta < 1e-6) {
        H = 0.0f;
    }
    else {
        if (R >= MX) {
            H = (G - B) / Delta;
        }
        else if (G >= MX) {
            H = 2.0f + (B - R) / Delta;
        }
        else {
            H = 4.0f + (R - G) / Delta;
        }
        H *= 60.0f;
        if (H < 0.0f) {
            H += 360.0f;
        }
    }
}

void PropertyEditorPanel::HSVToRGB(const float H, const float S, const float V, float& R, float& G, float& B)
{
    // h: 0~360, s:0~1, v:0~1
    const float C = V * S;
    const float Hp = H / 60.0f;             // 0~6 구간
    const float X = C * (1.0f - fabsf(fmodf(Hp, 2.0f) - 1.0f));
    const float M = V - C;

    if (Hp < 1.0f) { R = C;  G = X;  B = 0.0f; }
    else if (Hp < 2.0f) { R = X;  G = C;  B = 0.0f; }
    else if (Hp < 3.0f) { R = 0.0f; G = C;  B = X; }
    else if (Hp < 4.0f) { R = 0.0f; G = X;  B = C; }
    else if (Hp < 5.0f) { R = X;  G = 0.0f; B = C; }
    else { R = C;  G = 0.0f; B = X; }

    R += M;  G += M;  B += M;
}

void PropertyEditorPanel::RenderForSceneComponent(USceneComponent* SceneComponent, UEditorPlayer* Player) const
{
    ImGui::SetItemDefaultFocus();
    // TreeNode 배경색을 변경 (기본 상태)
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    if (ImGui::TreeNodeEx("Transform", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) // 트리 노드 생성
    {
        FVector Location = SceneComponent->GetRelativeLocation();
        FRotator Rotation = SceneComponent->GetRelativeRotation();
        FVector Scale = SceneComponent->GetRelativeScale3D();

        FImGuiWidget::DrawVec3Control("Location", Location, 0, 85);
        ImGui::Spacing();

        FImGuiWidget::DrawRot3Control("Rotation", Rotation, 0, 85);
        ImGui::Spacing();

        FImGuiWidget::DrawVec3Control("Scale", Scale, 0, 85);
        ImGui::Spacing();

        SceneComponent->SetRelativeLocation(Location);
        SceneComponent->SetRelativeRotation(Rotation);
        SceneComponent->SetRelativeScale3D(Scale);

        FString CoordiButtonLabel;
        if (Player->GetCoordMode() == ECoordMode::CDM_WORLD)
            CoordiButtonLabel = "World";
        else if (Player->GetCoordMode() == ECoordMode::CDM_LOCAL)
            CoordiButtonLabel = "Local";

        if (ImGui::Button(GetData(FString(CoordiButtonLabel + "##")), ImVec2(ImGui::GetWindowContentRegionMax().x * 0.9f, 32)))
        {
            Player->SetCoordiMode();
        }
        ImGui::TreePop(); // 트리 닫기
    }
    ImGui::PopStyleColor();
}

void PropertyEditorPanel::RenderForActor(AActor* InActor, USceneComponent* TargetComponent) const
{
    UEditorEngine* Engine = Cast<UEditorEngine>(GEngine);
    if (!Engine)
    {
        return;
    }
    
    if (ImGui::Button("Duplicate"))
    {
        AActor* NewActor = Engine->ActiveWorld->DuplicateActor(Engine->GetSelectedActor());
        Engine->SelectActor(NewActor);
        Engine->DeselectComponent(Engine->GetSelectedComponent());
    }

    //FString SceneName = Engine->ActiveWorld->GetActiveLevel()->GetLevelName();
    //FString Directory = FString("Scripts");
    //FString LuaScriptFilePath = Directory + "/" + SceneName + "/" + InActor->GetClass()->GetName() + ".lua";
    //std::filesystem::path filePath = std::filesystem::path(GetData(LuaScriptFilePath));
    //FString LuaScriptFileName = FString(filePath.filename()); 

    FString LuaFilePath = InActor->GetLuaScriptPathName();
    std::filesystem::path FilePath = std::filesystem::path(GetData(LuaFilePath));

    if (std::filesystem::exists(GetData(LuaFilePath)))
    {
        if (ImGui::Button("Edit Script"))
        {
            std::filesystem::path AbsPath = std::filesystem::absolute(FilePath);
            LPCTSTR LuaFilePath = AbsPath.c_str();

            // ShellExecute() -> Windows 확장자 연결(Association)에 따라 파일 열기
            HINSTANCE HInstance = ShellExecute(
                nullptr,            // 부모 윈도우 핸들 (NULL 사용 가능)
                L"open",      // 동작(Verb). "open"이면 등록된 기본 프로그램으로 열기
                LuaFilePath,     // 열고자 하는 파일 경로
                nullptr,            // 명령줄 인자 (필요 없다면 NULL)
                nullptr,            // 작업 디렉터리 (필요 없다면 NULL)
                SW_SHOWNORMAL    // 열리는 창의 상태
            );

            // ShellExecute는 성공 시 32보다 큰 값을 반환합니다.
            // 실패 시 32 이하의 값이 반환되므로 간단히 체크 가능
            if ((INT_PTR)HInstance <= 32) {
                MessageBox(nullptr, L"파일 열기에 실패했습니다.", L"Error", MB_OK | MB_ICONERROR);
            }
        }

        ImGui::SameLine();
        
        if (ImGui::Button("Delete Script"))
        {
            try
            {
                if (std::filesystem::exists(FilePath))
                {
                    std::filesystem::remove(FilePath);
                }
                else
                {
                    MessageBoxA(nullptr, "The script file does not exist.", "Error", MB_OK | MB_ICONERROR);
                }
            }
            catch (const std::filesystem::filesystem_error& e)
            {
                MessageBoxA(nullptr, "Failed to Delete Script File: ", "Error", MB_OK | MB_ICONERROR);
            }
        }
    }
    else
    {
        if (ImGui::Button("Create Script"))
        {
            try
            {
                std::filesystem::path Dir = FilePath.parent_path();
                if (!std::filesystem::exists(Dir))
                {
                    std::filesystem::create_directories(Dir);
                }

                std::ifstream luaTemplateFile(TemplateFilePath.ToWideString());

                std::ofstream file(FilePath);
                if (file.is_open())
                {
                    if (luaTemplateFile.is_open())
                    {
                        file << luaTemplateFile.rdbuf();
                    }
                    // 생성 완료
                    file.close();
                }
                else
                {
                    // TODO: Error Check
                    MessageBoxA(nullptr, "Failed to Create Script File for writing: ", "Error", MB_OK | MB_ICONERROR);
                }
            }
            catch (const std::filesystem::filesystem_error& e) 
            {
                // TODO: Error Check
                MessageBoxA(nullptr, "Failed to Create Script File for writing: ", "Error", MB_OK | MB_ICONERROR);
            }
        }
    }
    ImGui::InputText("Script Name", GetData(LuaFilePath), LuaFilePath.Len() + 1, ImGuiInputTextFlags_ReadOnly);

    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    if (ImGui::TreeNodeEx("Component", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) // 트리 노드 생성
    {
        ImGui::Text("Add");
        ImGui::SameLine();

        TArray<UClass*> CompClasses;
        GetChildOfClass(USceneComponent::StaticClass(), CompClasses);

        if (ImGui::BeginCombo("##AddComponent", "Components", ImGuiComboFlags_None))
        {
            for (UClass* Class : CompClasses)
            {
                if (ImGui::Selectable(GetData(Class->GetName()), false))
                {
                    USceneComponent* NewComp = Cast<USceneComponent>(InActor->AddComponent(Class));
                    if (NewComp != nullptr && TargetComponent != nullptr)
                    {
                        NewComp->SetupAttachment(TargetComponent);
                    }
                }
            }
            ImGui::EndCombo();
        }

        ImGui::TreePop();
    }
    ImGui::PopStyleColor();
}

void PropertyEditorPanel::RenderForStaticMesh(UStaticMeshComponent* StaticMeshComp) const
{
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    if (ImGui::TreeNodeEx("Static Mesh", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) // 트리 노드 생성
    {
        ImGui::Text("StaticMesh");
        ImGui::SameLine();

        FString PreviewName;
        if (StaticMeshComp->GetStaticMesh())
        {
            PreviewName = StaticMeshComp->GetStaticMesh()->GetRenderData()->DisplayName;
        }
        else
        {
            PreviewName = TEXT("None");
        }

        const TMap<FName, FAssetInfo> Assets = UAssetManager::Get().GetAssetRegistry();

        if (ImGui::BeginCombo("##StaticMesh", GetData(PreviewName), ImGuiComboFlags_None))
        {
            if (ImGui::Selectable(TEXT("None"), false))
            {
                StaticMeshComp->SetStaticMesh(nullptr);
            }

            for (const auto& Asset : Assets)
            {
                if (Asset.Value.AssetType == EAssetType::StaticMesh)
                {
                    if (ImGui::Selectable(GetData(Asset.Value.AssetName.ToString()), false))
                    {
                        FString MeshName = Asset.Value.PackagePath.ToString() + "/" + Asset.Value.AssetName.ToString();
                        UStaticMesh* StaticMesh = FManagerOBJ::GetStaticMesh(MeshName.ToWideString());
                        if (StaticMesh)
                        {
                            StaticMeshComp->SetStaticMesh(StaticMesh);
                        }
                    }
                }
            }
            ImGui::EndCombo();
        }

        ImGui::TreePop();
    }
    ImGui::PopStyleColor();
}

void PropertyEditorPanel::RenderForAmbientLightComponent(UAmbientLightComponent* LightComponent) const
{
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));

    if (ImGui::TreeNodeEx("AmbientLight Component", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
    {
        DrawColorProperty("Light Color",
            [&]() { return LightComponent->GetLightColor(); },
            [&](FLinearColor c) { LightComponent->SetLightColor(c.ToColorRawRGB8()); });
        ImGui::TreePop();
    }

    ImGui::PopStyleColor();
}

void PropertyEditorPanel::RenderForDirectionalLightComponent(UDirectionalLightComponent* LightComponent) const
{
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));

    if (ImGui::TreeNodeEx("DirectionalLight Component", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
    {
        DrawColorProperty("Light Color",
            [&]() { return LightComponent->GetLightColor(); },
            [&](FLinearColor c) { LightComponent->SetLightColor(c); });

        float Intensity = LightComponent->GetIntensity();
        if (ImGui::SliderFloat("Intensity", &Intensity, 0.0f, 150.0f, "%.1f"))
        {
            LightComponent->SetIntensity(Intensity);
        }

        // --- Cast Shadows 체크박스 추가 ---
        bool bCastShadows = LightComponent->GetCastShadows(); // 현재 상태 가져오기
        if (ImGui::Checkbox("Cast Shadows", &bCastShadows)) // 체크박스 UI 생성 및 상호작용 처리
        {
            LightComponent->SetCastShadows(bCastShadows); // 변경된 상태 설정
            // 필요하다면, 상태 변경에 따른 즉각적인 렌더링 업데이트 요청 로직 추가
            // 예: PointlightComponent->MarkRenderStateDirty();
        }
        ImGui::Text("ShadowMap");

        // 분할된 개수만큼 CSM 해당 SRV 출력
        const uint32& NumCascades = FEngineLoop::Renderer.ShadowManager->GetNumCasCades();
        for (uint32 i = 0; i < NumCascades; ++i)
        {
            ImGui::Image(reinterpret_cast<ImTextureID>(FEngineLoop::Renderer.ShadowManager->GetDirectionalShadowCascadeDepthRHI()->ShadowSRVs[i]), ImVec2(200, 200));
            //ImGui::SameLine();
        }
        ImGui::TreePop();
    }

    ImGui::PopStyleColor();
}

void PropertyEditorPanel::RenderForPointLightComponent(UPointLightComponent* LightComponent) const
{
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));

    if (ImGui::TreeNodeEx("PointLight Component", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
    {
        DrawColorProperty("Light Color",
            [&]() { return LightComponent->GetLightColor(); },
            [&](FLinearColor c) { LightComponent->SetLightColor(c); });

        float Intensity = LightComponent->GetIntensity();
        if (ImGui::SliderFloat("Intensity", &Intensity, 0.0f, 160.0f, "%.1f"))
        {
            LightComponent->SetIntensity(Intensity);
        }

        float Radius = LightComponent->GetRadius();
        if (ImGui::SliderFloat("Radius", &Radius, 0.01f, 200.f, "%.1f"))
        {
            LightComponent->SetRadius(Radius);
        }
        // --- Cast Shadows 체크박스 추가 ---
        bool bCastShadows = LightComponent->GetCastShadows(); // 현재 상태 가져오기
        if (ImGui::Checkbox("Cast Shadows", &bCastShadows)) // 체크박스 UI 생성 및 상호작용 처리
        {
            LightComponent->SetCastShadows(bCastShadows); // 변경된 상태 설정
            // 필요하다면, 상태 변경에 따른 즉각적인 렌더링 업데이트 요청 로직 추가
            // 예: PointlightComponent->MarkRenderStateDirty();
        }

        ImGui::Text("ShadowMap");

        FShadowCubeMapArrayRHI* pointRHI = FEngineLoop::Renderer.ShadowManager->GetPointShadowCubeMapRHI();
        const char* faceNames[] = { "+X", "-X", "+Y", "-Y", "+Z", "-Z" };
        float imageSize = 128.0f;
        int index = LightComponent->GetPointLightInfo().ShadowMapArrayIndex;
        // CubeMap이므로 6개의 ShadowMap을 그립니다.
        for (int i = 0; i < 6; ++i)
        {
            ID3D11ShaderResourceView* faceSRV = pointRHI->ShadowFaceSRVs[index][i];
            if (faceSRV)
            {
                ImGui::Image(reinterpret_cast<ImTextureID>(faceSRV), ImVec2(imageSize, imageSize));
                ImGui::SameLine(); 
                ImGui::Text("%s", faceNames[i]);
            }
        }

        ImGui::TreePop();
    }

    ImGui::PopStyleColor();
}

void PropertyEditorPanel::RenderForSpotLightComponent(USpotLightComponent* LightComponent) const
{
ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));

            if (ImGui::TreeNodeEx("SpotLight Component", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
            {
                DrawColorProperty("Light Color",
                    [&]() { return LightComponent->GetLightColor(); },
                    [&](FLinearColor c) { LightComponent->SetLightColor(c); });

                float Intensity = LightComponent->GetIntensity();
                if (ImGui::SliderFloat("Intensity", &Intensity, 0.0f, 160.0f, "%.1f"))
                {
                    LightComponent->SetIntensity(Intensity);
                }

                float Radius = LightComponent->GetRadius();
                if (ImGui::SliderFloat("Radius", &Radius, 0.01f, 200.f, "%.1f"))
                {
                    LightComponent->SetRadius(Radius);
                }

                float InnerConeAngle = LightComponent->GetInnerDegree();
                float OuterConeAngle = LightComponent->GetOuterDegree();
                if (ImGui::DragFloat("InnerConeAngle", &InnerConeAngle, 0.5f, 0.0f, 80.0f))
                {
                    OuterConeAngle = std::max(InnerConeAngle, OuterConeAngle);

                    LightComponent->SetInnerDegree(InnerConeAngle);
                    LightComponent->SetOuterDegree(OuterConeAngle);
                }

                if (ImGui::DragFloat("OuterConeAngle", &OuterConeAngle, 0.5f, 0.0f, 80.0f))
                {
                    InnerConeAngle = std::min(OuterConeAngle, InnerConeAngle);

                    LightComponent->SetOuterDegree(OuterConeAngle);
                    LightComponent->SetInnerDegree(InnerConeAngle);
                }

                // --- Cast Shadows 체크박스 추가 ---
                bool bCastShadows = LightComponent->GetCastShadows(); // 현재 상태 가져오기
                if (ImGui::Checkbox("Cast Shadows", &bCastShadows)) // 체크박스 UI 생성 및 상호작용 처리
                {
                    LightComponent->SetCastShadows(bCastShadows); // 변경된 상태 설정
                    // 필요하다면, 상태 변경에 따른 즉각적인 렌더링 업데이트 요청 로직 추가
                    // 예: PointlightComponent->MarkRenderStateDirty();
                }

                ImGui::Text("ShadowMap");
                ImGui::Image(reinterpret_cast<ImTextureID>(FEngineLoop::Renderer.ShadowManager->GetSpotShadowDepthRHI()->ShadowSRVs[0]), ImVec2(200, 200));

                ImGui::TreePop();
            }

            ImGui::PopStyleColor();
}

void PropertyEditorPanel::RenderForLightShadowCommon(ULightComponentBase* LightComponent) const
{
    if (GEngine->ActiveWorld->WorldType != EWorldType::Editor)
    {
        return;
    }
    // bCastShadow 토글.
    // Shadow Property 수치 조절.

    ImGui::Separator();

    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));

    // --- "Override Camera" 버튼 추가 ---
    if (ImGui::Button("Override Camera with Light's Perspective"))
    {
        // 1. 라이트의 월드 위치 및 회전 가져오기
        FVector LightLocation = LightComponent->GetWorldLocation();

        FVector Forward = FVector(1.f, 0.f, 0.0f);
        Forward = JungleMath::FVectorRotate(Forward, LightLocation);
        FVector LightForward = Forward;
        FRotator LightRotation = LightComponent->GetWorldRotation();
        FVector LightRotationVecter;
        LightRotationVecter.X = LightRotation.Roll;
        LightRotationVecter.Y = -LightRotation.Pitch;
        LightRotationVecter.Z = LightRotation.Yaw;

        // 2. 활성 에디터 뷰포트 클라이언트 가져오기 (!!! 엔진별 구현 필요 !!!)
        std::shared_ptr<FEditorViewportClient> ViewportClient = GEngineLoop.GetLevelEditor()->GetActiveViewportClient(); // 위에 정의된 헬퍼 함수 사용 (또는 직접 구현)
        
        // 3. 뷰포트 클라이언트가 유효하면 카메라 설정
        if (ViewportClient)
        {
            ViewportClient->GetPerspectiveCamera().SetLocation(LightLocation + LightForward); // 카메라 위치 설정 함수 호출
            ViewportClient->GetPerspectiveCamera().SetRotation(LightRotationVecter); // 카메라 회전 설정 함수 호출

            // 필요시 뷰포트 강제 업데이트/다시 그리기 호출
            // ViewportClient->Invalidate();
        }
        else
        {
            // 뷰포트 클라이언트를 찾을 수 없음 (오류 로그 등)
            // UE_LOG(LogTemp, Warning, TEXT("Active Viewport Client not found."));
        }
    }
    ImGui::PopStyleColor();

    ImGui::Separator();
}

void PropertyEditorPanel::RenderForProjectileMovementComponent(UProjectileMovementComponent* ProjectileComp) const
{
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));

    if (ImGui::TreeNodeEx("Projectile Movement Component", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
    {
        float InitialSpeed = ProjectileComp->GetInitialSpeed();
        if (ImGui::InputFloat("InitialSpeed", &InitialSpeed, 0.f, 10000.0f, "%.1f"))
            ProjectileComp->SetInitialSpeed(InitialSpeed);

        float MaxSpeed = ProjectileComp->GetMaxSpeed();
        if (ImGui::InputFloat("MaxSpeed", &MaxSpeed, 0.f, 10000.0f, "%.1f"))
            ProjectileComp->SetMaxSpeed(MaxSpeed);

        float Gravity = ProjectileComp->GetGravity();
        if (ImGui::InputFloat("Gravity", &Gravity, 0.f, 10000.f, "%.1f"))
            ProjectileComp->SetGravity(Gravity); 
                
        float ProjectileLifetime = ProjectileComp->GetLifetime();
        if (ImGui::InputFloat("Lifetime", &ProjectileLifetime, 0.f, 10000.f, "%.1f"))
            ProjectileComp->SetLifetime(ProjectileLifetime);

        FVector currentVelocity = ProjectileComp->GetVelocity();

        float velocity[3] = { currentVelocity.X, currentVelocity.Y, currentVelocity.Z };

        if (ImGui::InputFloat3("Velocity", velocity, "%.1f")) {
            ProjectileComp->SetVelocity(FVector(velocity[0], velocity[1], velocity[2]));
        }

        ImGui::TreePop();
    }

    ImGui::PopStyleColor();
}

void PropertyEditorPanel::RenderForTextComponent(UTextComponent* TextComponent) const
{
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    if (ImGui::TreeNodeEx("Text Component", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) // 트리 노드 생성
    {
        if (TextComponent) {
            TextComponent->SetTexture(L"Assets/Texture/font.png");
            TextComponent->SetRowColumnCount(106, 106);
            FWString wText = TextComponent->GetText();
            int len = WideCharToMultiByte(CP_UTF8, 0, wText.c_str(), -1, nullptr, 0, nullptr, nullptr);
            std::string u8Text(len, '\0');
            WideCharToMultiByte(CP_UTF8, 0, wText.c_str(), -1, u8Text.data(), len, nullptr, nullptr);

            static char buf[256];
            strcpy_s(buf, u8Text.c_str());

            ImGui::Text("Text: ", buf);
            ImGui::SameLine();
            ImGui::PushItemFlag(ImGuiItemFlags_NoNavDefaultFocus, true);
            if (ImGui::InputText("##Text", buf, 256, ImGuiInputTextFlags_EnterReturnsTrue))
            {
                TextComponent->ClearText();
                int wlen = MultiByteToWideChar(CP_UTF8, 0, buf, -1, nullptr, 0);
                FWString newWText(wlen, L'\0');
                MultiByteToWideChar(CP_UTF8, 0, buf, -1, newWText.data(), wlen);
                TextComponent->SetText(newWText.c_str());
            }
            ImGui::PopItemFlag();
        }
        ImGui::TreePop();
    }
    ImGui::PopStyleColor();
}

void PropertyEditorPanel::RenderForMaterial(UStaticMeshComponent* StaticMeshComp)
{
    if (StaticMeshComp->GetStaticMesh() == nullptr)
    {
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    if (ImGui::TreeNodeEx("Materials", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) // 트리 노드 생성
    {
        for (uint32 i = 0; i < StaticMeshComp->GetNumMaterials(); ++i)
        {
            if (ImGui::Selectable(GetData(StaticMeshComp->GetMaterialSlotNames()[i].ToString()), false, ImGuiSelectableFlags_AllowDoubleClick))
            {
                if (ImGui::IsMouseDoubleClicked(0))
                {
                    std::cout << GetData(StaticMeshComp->GetMaterialSlotNames()[i].ToString()) << '\n';
                    SelectedMaterialIndex = i;
                    SelectedStaticMeshComp = StaticMeshComp;
                }
            }
        }

        if (ImGui::Button("    +    ")) {
            IsCreateMaterial = true;
        }

        ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("SubMeshes", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) // 트리 노드 생성
    {
        auto Subsets = StaticMeshComp->GetStaticMesh()->GetRenderData()->MaterialSubsets;
        for (uint32 i = 0; i < Subsets.Num(); ++i)
        {
            std::string temp = "subset " + std::to_string(i);
            if (ImGui::Selectable(temp.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick))
            {
                if (ImGui::IsMouseDoubleClicked(0))
                {
                    StaticMeshComp->SetselectedSubMeshIndex(i);
                    SelectedStaticMeshComp = StaticMeshComp;
                }
            }
        }
        std::string Temp = "clear subset";
        if (ImGui::Selectable(Temp.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick))
        {
            if (ImGui::IsMouseDoubleClicked(0))
                StaticMeshComp->SetselectedSubMeshIndex(-1);
        }

        ImGui::TreePop();
    }

    ImGui::PopStyleColor();

    if (SelectedMaterialIndex != -1)
    {
        RenderMaterialView(SelectedStaticMeshComp->GetMaterial(SelectedMaterialIndex));
    }
    if (IsCreateMaterial) {
        RenderCreateMaterialView();
    }
}

void PropertyEditorPanel::RenderMaterialView(UMaterial* Material)
{
    ImGui::SetNextWindowSize(ImVec2(380, 400), ImGuiCond_Once);
    ImGui::Begin("Material Viewer", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav);

    static ImGuiSelectableFlags BaseFlag = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_None | ImGuiColorEditFlags_NoAlpha;

    FVector MatDiffuseColor = Material->GetMaterialInfo().Diffuse;
    FVector MatSpecularColor = Material->GetMaterialInfo().Specular;
    FVector MatAmbientColor = Material->GetMaterialInfo().Ambient;
    FVector MatEmissiveColor = Material->GetMaterialInfo().Emissive;

    float DiffuseR = MatDiffuseColor.X;
    float DiffuseG = MatDiffuseColor.Y;
    float DiffuseB = MatDiffuseColor.Z;
    float DiffuseA = 1.0f;
    float DiffuseColorPick[4] = { DiffuseR, DiffuseG, DiffuseB, DiffuseA };

    ImGui::Text("Material Name |");
    ImGui::SameLine();
    ImGui::Text(*Material->GetMaterialInfo().MaterialName);
    ImGui::Separator();

    ImGui::Text("  Diffuse Color");
    ImGui::SameLine();
    if (ImGui::ColorEdit4("Diffuse##Color", reinterpret_cast<float*>(&DiffuseColorPick), BaseFlag))
    {
        FVector NewColor = { DiffuseColorPick[0], DiffuseColorPick[1], DiffuseColorPick[2] };
        Material->SetDiffuse(NewColor);
    }

    float SpecularR = MatSpecularColor.X;
    float SpecularG = MatSpecularColor.Y;
    float SpecularB = MatSpecularColor.Z;
    float SpecularA = 1.0f;
    float SpecularColorPick[4] = { SpecularR, SpecularG, SpecularB, SpecularA };

    ImGui::Text("Specular Color");
    ImGui::SameLine();
    if (ImGui::ColorEdit4("Specular##Color", reinterpret_cast<float*>(&SpecularColorPick), BaseFlag))
    {
        FVector NewColor = { SpecularColorPick[0], SpecularColorPick[1], SpecularColorPick[2] };
        Material->SetSpecular(NewColor);
    }


    float AmbientR = MatAmbientColor.X;
    float AmbientG = MatAmbientColor.Y;
    float AmbientB = MatAmbientColor.Z;
    float AmbientA = 1.0f;
    float AmbientColorPick[4] = { AmbientR, AmbientG, AmbientB, AmbientA };

    ImGui::Text("Ambient Color");
    ImGui::SameLine();
    if (ImGui::ColorEdit4("Ambient##Color", reinterpret_cast<float*>(&AmbientColorPick), BaseFlag))
    {
        FVector NewColor = { AmbientColorPick[0], AmbientColorPick[1], AmbientColorPick[2] };
        Material->SetAmbient(NewColor);
    }


    const float EmissiveR = MatEmissiveColor.X;
    const float EmissiveG = MatEmissiveColor.Y;
    const float EmissiveB = MatEmissiveColor.Z;
    constexpr float EmissiveA = 1.0f;
    float EmissiveColorPick[4] = { EmissiveR, EmissiveG, EmissiveB, EmissiveA };
    
    ImGui::Text("Emissive Color");
    ImGui::SameLine();
    if (ImGui::ColorEdit4("Emissive##Color", (float*)&EmissiveColorPick, BaseFlag))
    {
        FVector NewColor = { EmissiveColorPick[0], EmissiveColorPick[1], EmissiveColorPick[2] };
        Material->SetEmissive(NewColor);
    }

    ImGui::Spacing();
    ImGui::Separator();

    ImGui::Text("Choose Material");
    ImGui::Spacing();

    ImGui::Text("Material Slot Name |");
    ImGui::SameLine();
    ImGui::Text(GetData(SelectedStaticMeshComp->GetMaterialSlotNames()[SelectedMaterialIndex].ToString()));

    ImGui::Text("Override Material |");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160);
    // 메테리얼 이름 목록을 const char* 배열로 변환
    std::vector<const char*> materialChars;
    for (const auto& material : FManagerOBJ::GetMaterials()) {
        materialChars.push_back(*material.Value->GetMaterialInfo().MaterialName);
    }

    //// 드롭다운 표시 (currentMaterialIndex가 범위를 벗어나지 않도록 확인)
    //if (currentMaterialIndex >= FManagerOBJ::GetMaterialNum())
    //    currentMaterialIndex = 0;

    if (ImGui::Combo("##MaterialDropdown", &CurMaterialIndex, materialChars.data(), FManagerOBJ::GetMaterialNum())) {
        UMaterial* material = FManagerOBJ::GetMaterial(materialChars[CurMaterialIndex]);
        SelectedStaticMeshComp->SetMaterial(SelectedMaterialIndex, material);
    }

    if (ImGui::Button("Close"))
    {
        SelectedMaterialIndex = -1;
        SelectedStaticMeshComp = nullptr;
    }

    ImGui::End();
}

void PropertyEditorPanel::RenderCreateMaterialView()
{
    ImGui::SetNextWindowSize(ImVec2(300, 500), ImGuiCond_Once);
    ImGui::Begin("Create Material Viewer", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoNav);

    static ImGuiSelectableFlags BaseFlag = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_None | ImGuiColorEditFlags_NoAlpha;

    ImGui::Text("New Name");
    ImGui::SameLine();
    static char materialName[256] = "New Material";
    // 기본 텍스트 입력 필드
    ImGui::SetNextItemWidth(128);
    if (ImGui::InputText("##NewName", materialName, IM_ARRAYSIZE(materialName))) {
        tempMaterialInfo.MaterialName = materialName;
    }

    FVector MatDiffuseColor = tempMaterialInfo.Diffuse;
    FVector MatSpecularColor = tempMaterialInfo.Specular;
    FVector MatAmbientColor = tempMaterialInfo.Ambient;
    FVector MatEmissiveColor = tempMaterialInfo.Emissive;

    const float DiffuseR = MatDiffuseColor.X;
    const float DiffuseG = MatDiffuseColor.Y;
    const float DiffuseB = MatDiffuseColor.Z;
    constexpr float DiffuseA = 1.0f;
    float DiffuseColorPick[4] = { DiffuseR, DiffuseG, DiffuseB, DiffuseA };

    ImGui::Text("Set Property");
    ImGui::Indent();

    ImGui::Text("  Diffuse Color");
    ImGui::SameLine();
    if (ImGui::ColorEdit4("Diffuse##Color", reinterpret_cast<float*>(&DiffuseColorPick), BaseFlag))
    {
        FVector NewColor = { DiffuseColorPick[0], DiffuseColorPick[1], DiffuseColorPick[2] };
        tempMaterialInfo.Diffuse = NewColor;
    }

    const float SpecularR = MatSpecularColor.X;
    const float SpecularG = MatSpecularColor.Y;
    const float SpecularB = MatSpecularColor.Z;
    constexpr float SpecularA = 1.0f;
    float SpecularColorPick[4] = { SpecularR, SpecularG, SpecularB, SpecularA };
    
    ImGui::Text("Specular Color");
    ImGui::SameLine();
    if (ImGui::ColorEdit4("Specular##Color", reinterpret_cast<float*>(&SpecularColorPick), BaseFlag))
    {
        FVector NewColor = { SpecularColorPick[0], SpecularColorPick[1], SpecularColorPick[2] };
        tempMaterialInfo.Specular = NewColor;
    }

    const float AmbientR = MatAmbientColor.X;
    const float AmbientG = MatAmbientColor.Y;
    const float AmbientB = MatAmbientColor.Z;
    constexpr float AmbientA = 1.0f;
    float AmbientColorPick[4] = { AmbientR, AmbientG, AmbientB, AmbientA };

    ImGui::Text("Ambient Color");
    ImGui::SameLine();
    if (ImGui::ColorEdit4("Ambient##Color", (float*)&AmbientColorPick, BaseFlag))
    {
        FVector NewColor = { AmbientColorPick[0], AmbientColorPick[1], AmbientColorPick[2] };
        tempMaterialInfo.Ambient = NewColor;
    }


    const float EmissiveR = MatEmissiveColor.X;
    const float EmissiveG = MatEmissiveColor.Y;
    const float EmissiveB = MatEmissiveColor.Z;
    constexpr float EmissiveA = 1.0f;
    float EmissiveColorPick[4] = { EmissiveR, EmissiveG, EmissiveB, EmissiveA };

    ImGui::Text("Emissive Color");
    ImGui::SameLine();
    if (ImGui::ColorEdit4("Emissive##Color", reinterpret_cast<float*>(&EmissiveColorPick), BaseFlag))
    {
        FVector NewColor = { EmissiveColorPick[0], EmissiveColorPick[1], EmissiveColorPick[2] };
        tempMaterialInfo.Emissive = NewColor;
    }
    ImGui::Unindent();

    ImGui::NewLine();
    if (ImGui::Button("Create Material")) {
        FManagerOBJ::CreateMaterial(tempMaterialInfo);
    }

    ImGui::NewLine();
    if (ImGui::Button("Close"))
    {
        IsCreateMaterial = false;
    }

    ImGui::End();
}

void PropertyEditorPanel::RenderForExponentialHeightFogComponent(UHeightFogComponent* FogComponent) const
{
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    if (ImGui::TreeNodeEx("Exponential Height Fog", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) // 트리 노드 생성
    {
        FLinearColor currColor = FogComponent->GetFogColor();

        float R = currColor.R;
        float G = currColor.G;
        float B = currColor.B;
        float A = currColor.A;
        float H, S, V;
        float lightColor[4] = { R, G, B, A };

        // Fog Color
        if (ImGui::ColorPicker4("##Fog Color", lightColor,
            ImGuiColorEditFlags_DisplayRGB |
            ImGuiColorEditFlags_NoSidePreview |
            ImGuiColorEditFlags_NoInputs |
            ImGuiColorEditFlags_Float))

        {

            R = lightColor[0];
            G = lightColor[1];
            B = lightColor[2];
            A = lightColor[3];
            FogComponent->SetFogColor(FLinearColor(R, G, B, A));
        }
        RGBToHSV(R, G, B, H, S, V);
        // RGB/HSV
        bool changedRGB = false;
        bool changedHSV = false;

        // RGB
        ImGui::PushItemWidth(50.0f);
        if (ImGui::DragFloat("R##R", &R, 0.001f, 0.f, 1.f)) changedRGB = true;
        ImGui::SameLine();
        if (ImGui::DragFloat("G##G", &G, 0.001f, 0.f, 1.f)) changedRGB = true;
        ImGui::SameLine();
        if (ImGui::DragFloat("B##B", &B, 0.001f, 0.f, 1.f)) changedRGB = true;
        ImGui::Spacing();

        // HSV
        if (ImGui::DragFloat("H##H", &H, 0.1f, 0.f, 360)) changedHSV = true;
        ImGui::SameLine();
        if (ImGui::DragFloat("S##S", &S, 0.001f, 0.f, 1)) changedHSV = true;
        ImGui::SameLine();
        if (ImGui::DragFloat("V##V", &V, 0.001f, 0.f, 1)) changedHSV = true;
        ImGui::PopItemWidth();
        ImGui::Spacing();

        if (changedRGB && !changedHSV)
        {
            // RGB -> HSV
            RGBToHSV(R, G, B, H, S, V);
            FogComponent->SetFogColor(FLinearColor(R, G, B, A));
        }
        else if (changedHSV && !changedRGB)
        {
            // HSV -> RGB
            HSVToRGB(H, S, V, R, G, B);
            FogComponent->SetFogColor(FLinearColor(R, G, B, A));
        }

        float FogDensity = FogComponent->GetFogDensity();
        if (ImGui::SliderFloat("Density", &FogDensity, 0.00f, 3.0f))
        {
            FogComponent->SetFogDensity(FogDensity);
        }

        float FogDistanceWeight = FogComponent->GetFogDistanceWeight();
        if (ImGui::SliderFloat("Distance Weight", &FogDistanceWeight, 0.00f, 3.0f))
        {
            FogComponent->SetFogDistanceWeight(FogDistanceWeight);
        }

        float FogHeightFallOff = FogComponent->GetFogHeightFalloff();
        if (ImGui::SliderFloat("Height Fall Off", &FogHeightFallOff, 0.001f, 0.15f))
        {
            FogComponent->SetFogHeightFalloff(FogHeightFallOff);
        }

        float FogStartDistance = FogComponent->GetStartDistance();
        if (ImGui::SliderFloat("Start Distance", &FogStartDistance, 0.00f, 50.0f))
        {
            FogComponent->SetStartDistance(FogStartDistance);
        }

        float FogEndtDistance = FogComponent->GetEndDistance();
        if (ImGui::SliderFloat("End Distance", &FogEndtDistance, 0.00f, 50.0f))
        {
            FogComponent->SetEndDistance(FogEndtDistance);
        }

        ImGui::TreePop();
    }
    ImGui::PopStyleColor();
}

void PropertyEditorPanel::RenderForShapeComponent(UShapeComponent* ShapeComponent) const
{
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    if (USphereComponent* Component = Cast<USphereComponent>(ShapeComponent))
    {
        if (ImGui::TreeNodeEx("Sphere Collision", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) // 트리 노드 생성
        {
            float Radius = Component->GetRadius();
            ImGui::Text("Radius");
            ImGui::SameLine();
            if (ImGui::DragFloat("##Radius", &Radius))
            {
                Component->SetRadius(Radius);
            }
            ImGui::TreePop();
        }
    }

    if (UBoxComponent* Component = Cast<UBoxComponent>(ShapeComponent))
    {
        if (ImGui::TreeNodeEx("Box Collision", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) // 트리 노드 생성
        {
            FVector Extent = Component->GetBoxExtent();

            float Extents[3] = { Extent.X, Extent.Y, Extent.Z };

            ImGui::Text("Extent");
            ImGui::SameLine();
            if (ImGui::DragFloat3("##Extent", Extents)) {
                Component->SetBoxExtent(FVector(Extents[0], Extents[1], Extents[2]));
            }
            ImGui::TreePop();
        }
    }

    if (UCapsuleComponent* Component = Cast<UCapsuleComponent>(ShapeComponent))
    {
        if (ImGui::TreeNodeEx("Box Collision", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen)) // 트리 노드 생성
        {
            float HalfHeight = Component->GetHalfHeight();
            float Radius = Component->GetRadius();

            ImGui::Text("HalfHeight");
            ImGui::SameLine();
            if (ImGui::DragFloat("##HalfHeight", &HalfHeight, 0.1f)) {
                Component->SetHalfHeight(HalfHeight);
            }

            ImGui::Text("Radius");
            ImGui::SameLine();
            if (ImGui::DragFloat("##Radius", &Radius, 0.1f)) {
                Component->SetRadius(Radius);
            }
            ImGui::TreePop();
        }
    }
    
    ImGui::PopStyleColor();
}

void PropertyEditorPanel::RenderForSpringArmComponent(USpringArmComponent* SpringArmComp) const
{
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    
    if (ImGui::TreeNodeEx("Spring Arm - Camera", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
    {
        float TargetArmLength = SpringArmComp->GetTargetArmLength();
        
        ImGui::Text("Target Arm Length");
        ImGui::SameLine();
        if (ImGui::DragFloat("##Target ArmLength", &TargetArmLength, 0.1f, 0, 0, "%.1f")) {
            SpringArmComp->SetTargetArmLength(TargetArmLength);
        }
        
        FVector SocketOffset = SpringArmComp->GetSocketOffset();
        float SocketOffsets[3] = { SocketOffset.X, SocketOffset.Y, SocketOffset.Z };

        ImGui::Text("Socket Offset");
        ImGui::SameLine();
        if (ImGui::DragFloat3("##Socket Offset", SocketOffsets, 0.1f, 0, 0, "%.1f")) {
            SpringArmComp->SetSocketOffset(FVector(SocketOffsets[0], SocketOffsets[1], SocketOffsets[2]));
        }
        
        FVector TargetOffset = SpringArmComp->GetTargetOffset();
        float TargetOffsets[3] = { TargetOffset.X, TargetOffset.Y, TargetOffset.Z };

        ImGui::Text("Target Offset");
        ImGui::SameLine();
        if (ImGui::DragFloat3("##Target Offset", TargetOffsets, 0.1f, 0, 0, "%.1f")) {
            SpringArmComp->SetTargetOffset(FVector(TargetOffsets[0], TargetOffsets[1], TargetOffsets[2]));
        }
        
        ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("Camera Setting", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool bUsePawnControlRotation = SpringArmComp->GetUsePawnControlRotation();
        bool bInheritPitch = SpringArmComp->GetInheritPitch();
        bool bInheritYaw = SpringArmComp->GetInheritYaw();
        bool bInheritRoll = SpringArmComp->GetInheritRoll();

        if (ImGui::Checkbox("Use Pawn Control Rotation", &bUsePawnControlRotation))
        {
            SpringArmComp->SetUsePawnControlRotation(bUsePawnControlRotation);
        }
        
        if (ImGui::Checkbox("Inherit Pitch", &bInheritPitch))
        {
            SpringArmComp->SetInheritPitch(bInheritPitch);
        }

        if (ImGui::Checkbox("Inherit Yaw", &bInheritYaw))
        {
            SpringArmComp->SetInheritYaw(bInheritYaw);
        }

        if (ImGui::Checkbox("Inherit Roll", &bInheritRoll))
        {
            SpringArmComp->SetInheritRoll(bInheritRoll);
        }
        
        ImGui::TreePop();
    }
    
    if (ImGui::TreeNodeEx("Lag", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
    {
        bool bEnableCameraLag = SpringArmComp->GetEnableCameraLag();
        bool bEnableCameraRotationLag = SpringArmComp->GetEnableCameraRotationLag();

        if (ImGui::Checkbox("Enable Lag", &bEnableCameraLag))
        {
            SpringArmComp->SetEnableCameraLag(bEnableCameraLag);
        }
        
        if (ImGui::Checkbox("Enable Rotation Lag", &bEnableCameraRotationLag))
        {
            SpringArmComp->SetEnableCameraRotationLag(bEnableCameraRotationLag);
        }

        float CameraLagSpeed = SpringArmComp->GetCameraLagSpeed();
        float CameraRotationLagSpeed = SpringArmComp->GetCameraRotationLagSpeed();
        float CameraLagMaxDistance = SpringArmComp->GetCameraLagMaxDistance();
        
        ImGui::Text("Lag Speed");
        ImGui::SameLine();
        if (ImGui::DragFloat("##Lag Speed", &CameraLagSpeed, 0.1f, 0, 0, "%.1f")) {
            SpringArmComp->SetCameraLagSpeed(CameraLagSpeed);
        }

        ImGui::Text("Rotation Lag Speed");
        ImGui::SameLine();
        if (ImGui::DragFloat("##Rotation Lag Speed", &CameraRotationLagSpeed, 0.1f, 0, 0, "%.1f")) {
            SpringArmComp->SetCameraRotationLagSpeed(CameraRotationLagSpeed);
        }

        ImGui::Text("Lag Max Distance");
        ImGui::SameLine();
        if (ImGui::DragFloat("##Lag Max Distance", &CameraLagMaxDistance, 0.1f, 0, 0, "%.1f")) {
            SpringArmComp->SetCameraLagMaxDistance(CameraLagMaxDistance);
        }
        
        ImGui::TreePop();
    }
    ImGui::PopStyleColor();
}

void PropertyEditorPanel::RenderForCurve(FString& CurvePath) const
{
    // 초기값
    static float v[5] = { 0.950f, 0.050f, 0.795f, 0.035f };
    if (ImGui::Bezier("Label", v))
    {
        float y = ImGui::BezierValue( 0.5f, v ); // x delta in [0..1] range
        UE_LOG(LogLevel::Display, "Value: %.3f", y);
    }
    
    // 조절 가능 Value
    // ------
    const static uint32 PointCount = 100;
    const ImVec2 Min = ImVec2(0, 0);
    const ImVec2 Max = ImVec2(1, 1);
    // ------

    const ImVec2 End = ImVec2(-10000, 1);
    
    static ImVec2 Curves[PointCount] = {
        Min,
        Max,
        End,
    };
    
    static int selectedIndex = -1;
    if (ImGui::Curve("Das editor", ImVec2(200, 200), PointCount, Curves, &selectedIndex, Min, Max))
    {
        // curve changed
        float Value = CurveManager::CurveValue(0.5, PointCount, Curves); // x delta in [0..1] range
        float SmoothValue = ImGui::CurveValueSmooth(0.5, PointCount, Curves); // x delta in [0..1] range
        UE_LOG(LogLevel::Display, "Value: %.3f, Smooth Value: %.3f", Value, SmoothValue);
    }
    
    char CurveFileName[256];
    strcpy_s(CurveFileName, std::filesystem::path(GetData(CurvePath)).stem().string().c_str());
    
    ImGui::InputText("Curve Name", CurveFileName, IM_ARRAYSIZE(CurveFileName));


    std::filesystem::path CurveFilePath = GetData("Contents/Curves/" + FString(CurveFileName) + ".csv");
    bool isOpen;
    if (std::filesystem::exists(CurveFilePath))
    {
        isOpen = ImGui::Button("Overwrite Curve");
    }
    else
    {
        isOpen = ImGui::Button("Create Curve");
    }

    if (isOpen)
    {
        try
        {
            std::filesystem::path Dir = CurveFilePath.parent_path();
            if (!std::filesystem::exists(Dir))
            {
                std::filesystem::create_directories(Dir);
            }

            bool bIsExist = std::filesystem::exists(CurveFilePath);

            std::ofstream file(CurveFilePath);
            if (file.is_open())
            {
                if (!bIsExist)
                {
                    UAssetManager::Get().LoadObjFiles();
                }
                // Linear
                // Cubic Hermite?
                // Cubic Bezier
                // Catmull Rom
                // B Spline
                // Stop Interporation
                // file << "Time,Value,InTangent,OutTangent.Interpolation,\n";
                file << "Time,Value,\n";

                // 데이터 작성
                for (const auto& key : Curves) {
                    file << key.x << "," << key.y << ",\n";
                    //file << key.time << "," << key.value << "," << key.inTangent << "," << key.outTangent << "," << key.Interpolation << ",\n";
                }
                
                file.close();
            }
            else
            {
                // TODO: Error Check
                MessageBoxA(nullptr, "Failed to Create Curve File for writing: ", "Error", MB_OK | MB_ICONERROR);
            }
        }
        catch (const std::filesystem::filesystem_error& e) 
        {
            // TODO: Error Check
            MessageBoxA(nullptr, "Failed to Curve Script File for writing: ", "Error", MB_OK | MB_ICONERROR);
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Reset"))
    {
        try
        {
            bool bIsExist = std::filesystem::exists(CurveFilePath);

            if (bIsExist)
            {
                CurveManager::LoadCurve(CurveFilePath, PointCount, Curves);
            }
            else
            {
                CurveManager::ResetCurve(Curves, Min, Max, End, PointCount);
            }
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            // TODO: Error Check
            MessageBoxA(nullptr, "Failed to Load Curve File for writing: ", "Error", MB_OK | MB_ICONERROR);
        }
    }

    ImGui::Text("Curve");
    ImGui::SameLine();

    FString FileName;
    if (CurveFilePath.stem().string().starts_with("."))
    {
        FileName = "";
    }
    else
    {
        FileName = FString(CurveFilePath.stem());
    }
    const TMap<FName, FAssetInfo> Assets = UAssetManager::Get().GetAssetRegistry();

    if (ImGui::BeginCombo("##Curve", GetData(FileName), ImGuiComboFlags_None))
    {
        if (ImGui::Selectable(TEXT("Empty"), false))
        {
            strcpy_s(CurveFileName, "");
            CurveManager::ResetCurve(Curves, Min, Max, End, PointCount);
        }        
            
        for (const auto& Asset : Assets)
        {
            if (Asset.Value.AssetType == EAssetType::Curve)
            {
                if (ImGui::Selectable(GetData(Asset.Value.AssetName.ToString()), false))
                {
                    FString filepath = Asset.Value.PackagePath.ToString() + "/" + Asset.Value.AssetName.ToString();
                    if (std::filesystem::exists(std::filesystem::path(GetData(filepath))))
                    {
                        strcpy_s(CurveFileName, GetData(FString(std::filesystem::path(GetData(Asset.Value.AssetName.ToString())).stem())));
                        CurveFilePath = GetData("Contents/Curves/" + FString(CurveFileName) + ".csv");
                        CurveManager::LoadCurve(CurveFilePath, PointCount, Curves);
                    }
                }
            }
        }
        ImGui::EndCombo();
    }

    CurvePath = FString(CurveFilePath);
}

void PropertyEditorPanel::RenderForCameraComponent(UCameraComponent* CameraComponent) const
{
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    
    if (ImGui::TreeNodeEx("Camera", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
    {
        float AspectRatio = CameraComponent->GetAspectRatio();
        ImGui::Text("AspectRatio");
        ImGui::SameLine();
        if (ImGui::DragFloat("##AspectRatio", &AspectRatio, 0.1f, 0, 0, "%.1f")) {
            CameraComponent->SetAspectRatio(AspectRatio);
        }
        
        if (CameraComponent->GetProjectionMode() == ECameraProjectionMode::Perspective)
        {
            float FOV = CameraComponent->GetFieldOfView();
            ImGui::Text("Field Of View");
            ImGui::SameLine();
            if (ImGui::DragFloat("##Field Of View", &FOV, 0.1f, 0, 0, "%.1f")) {
                CameraComponent->SetFieldOfView(FOV);
            }
            float NearClip = CameraComponent->GetNearClip();
            ImGui::Text("NearClip");
            ImGui::SameLine();
            if (ImGui::DragFloat("##NearClip", &NearClip, 0.1f, 0, 0, "%.1f")) {
                CameraComponent->SetNearClip(NearClip);
            }

            float FarClip = CameraComponent->GetFarClip();
            ImGui::Text("FarClip");
            ImGui::SameLine();
            if (ImGui::DragFloat("##FarClip", &FarClip, 0.1f, 0, 0, "%.1f")) {
                CameraComponent->SetFarClip(FarClip);
            }
        }
        else
        {
            float OrthoZoom = CameraComponent->GetOrthoZoom();
            ImGui::Text("OrthoZoom");
            ImGui::SameLine();
            if (ImGui::DragFloat("##OrthoZoom", &OrthoZoom, 0.1f, 0, 0, "%.1f")) {
                CameraComponent->SetOrthoZoom(OrthoZoom);
            }

            float OrthoWidth = CameraComponent->GetOrthoWidth();
            ImGui::Text("OrthoWidth");
            ImGui::SameLine();
            if (ImGui::DragFloat("##OrthoWidth", &OrthoWidth, 0.1f, 0, 0, "%.1f")) {
                CameraComponent->SetOrthoWidth(OrthoWidth);
            }
        
            float OrthoNearClipPlane = CameraComponent->GetOrthoNearClipPlane();
            ImGui::Text("OrthoNearClipPlane");
            ImGui::SameLine();
            if (ImGui::DragFloat("##OrthoNearClipPlane", &OrthoNearClipPlane, 0.1f, 0, 0, "%.1f")) {
                CameraComponent->SetOrthoNearClipPlane(OrthoNearClipPlane);
            }

            float OrthoFarClipPlane = CameraComponent->GetOrthoFarClipPlane();
            ImGui::Text("OrthoFarClipPlane");
            ImGui::SameLine();
            if (ImGui::DragFloat("##OrthoNearClipPlane", &OrthoFarClipPlane, 0.1f, 0, 0, "%.1f")) {
                CameraComponent->SetOrthoFarClipPlane(OrthoFarClipPlane);
            }
        }

        FString Path = CameraComponent->GetCurvePath();
        RenderForCurve(Path);
        if (CameraComponent->GetCurvePath() != Path)
        {
            CameraComponent->SetCurvePath(Path);
        }
        
        ImGui::TreePop();
    }
    ImGui::PopStyleColor();
}

void PropertyEditorPanel::RenderForSkeletalMeshComponent(USkinnedMeshComponent* SkeletalMeshComp) const
{
    ImGui::Text("Skinned Mesh Component");
    ImGui::Separator();

    USkeletalMesh* SkeletalMesh = SkeletalMeshComp->GetSkeletalMesh();
    if (SkeletalMesh)
    {
        FString MeshName = SkeletalMesh->GetName();
        FString FilePath = SkeletalMesh->GetRenderData()->FilePath;

        ImGui::Text("Skeletal Mesh: %s", *MeshName);
        ImGui::Separator();

        if (ImGui::Button("Open in EngineSIU_Viewer"))
        {
            std::filesystem::path AbsPath = std::filesystem::absolute(std::filesystem::path(GetData(FilePath)));

            FString ViewerExe = TEXT("EngineSIU_Viewer.exe");

            ShellExecuteW(
                nullptr,
                L"open",
                ViewerExe.ToWideString().c_str(),
                AbsPath.wstring().c_str(),
                nullptr,
                SW_SHOWNORMAL
            );

        }
    }
}
void PropertyEditorPanel::OnResize(HWND hWnd)
{
    RECT ClientRect;
    GetClientRect(hWnd, &ClientRect);
    Width = ClientRect.right - ClientRect.left;
    Height = ClientRect.bottom - ClientRect.top;
}

template<typename Getter, typename Setter>
void DrawColorProperty(const char* Label, Getter Get, Setter Set)
{
    ImGui::PushItemWidth(200.0f);
    const FLinearColor CurrentColor = Get();
    float Col[4] = { CurrentColor.R, CurrentColor.G, CurrentColor.B, CurrentColor.A };

    if (ImGui::ColorEdit4(Label, Col,
        ImGuiColorEditFlags_DisplayRGB |
        ImGuiColorEditFlags_NoSidePreview |
        ImGuiColorEditFlags_NoInputs |
        ImGuiColorEditFlags_Float))
    {
        Set(FLinearColor(Col[0], Col[1], Col[2], Col[3]));
    }
    ImGui::PopItemWidth();
}

template<typename T>
    requires std::derived_from<T, UActorComponent>
T* PropertyEditorPanel::GetTargetComponent(AActor* SelectedActor, USceneComponent* SelectedComponent)
{
    T* ResultComp = nullptr;
    if (SelectedComponent != nullptr)
    {
        ResultComp = Cast<T>(SelectedComponent);
    }
    else if (SelectedActor != nullptr)
    {
        ResultComp = SelectedActor->GetComponentByClass<T>();
    }
        
    return ResultComp;
}
