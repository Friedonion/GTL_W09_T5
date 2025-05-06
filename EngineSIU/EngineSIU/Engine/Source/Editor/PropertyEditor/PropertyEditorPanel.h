#pragma once

#include <filesystem>

#include "Components/ActorComponent.h"
#include "Components/SpringArmComponent.h"
#include "Components/Shapes/ShapeComponent.h"
#include "UnrealEd/EditorPanel.h"

class UCameraComponent;
struct ImVec2;
class ULightComponentBase;
class UEditorPlayer;
class USceneComponent;
class USpotLightComponent;
class UPointLightComponent;
class UDirectionalLightComponent;
class UAmbientLightComponent;
class UProjectileMovementComponent;
class UTextComponent;
class UHeightFogComponent;
class UStaticMeshComponent;
class USkinnedMeshComponent;

// 헬퍼 함수 예시
template<typename Getter, typename Setter>
void DrawColorProperty(const char* Label, Getter Get, Setter Set);


class PropertyEditorPanel : public UEditorPanel
{
public:
    virtual void Render() override;
    virtual void OnResize(HWND hWnd) override;


private:
    static void RGBToHSV(float R, float G, float B, float& H, float& S, float& V);
    static void HSVToRGB(float H, float S, float V, float& R, float& G, float& B);

    void RenderForSceneComponent(USceneComponent* SceneComponent, UEditorPlayer* Player) const;
    void RenderForActor(AActor* InActor, USceneComponent* TargetComponent) const;

    /* Static Mesh Settings */
    void RenderForStaticMesh(UStaticMeshComponent* StaticMeshComp) const;
    /* Materials Settings */
    void RenderForMaterial(UStaticMeshComponent* StaticMeshComp);
    void RenderMaterialView(UMaterial* Material);
    void RenderCreateMaterialView();

    void RenderForAmbientLightComponent(UAmbientLightComponent* LightComp) const;
    void RenderForDirectionalLightComponent(UDirectionalLightComponent* LightComponent) const;
    void RenderForPointLightComponent(UPointLightComponent* LightComponent) const;
    void RenderForSpotLightComponent(USpotLightComponent* LightComponent) const;
    void RenderForLightShadowCommon(ULightComponentBase* LightComponent) const;
    
    void RenderForProjectileMovementComponent(UProjectileMovementComponent* ProjectileComp) const;
    void RenderForTextComponent(UTextComponent* TextComponent) const;
    
    void RenderForExponentialHeightFogComponent(UHeightFogComponent* ExponentialHeightFogComp) const;

    void RenderForShapeComponent(UShapeComponent* ShapeComponent) const;

    void RenderForSpringArmComponent(USpringArmComponent* SpringArmComp) const;

    void RenderForCurve(FString& CurvePath) const;

    void RenderForCameraComponent(UCameraComponent* CameraComponent) const;

    void RenderForSkeletalMeshComponent(USkinnedMeshComponent* SkinnedMeshComponent) const;
    
    template<typename T>
        requires std::derived_from<T, UActorComponent>
    T* GetTargetComponent(AActor* SelectedActor, USceneComponent* SelectedComponent);

private:
    float Width = 0, Height = 0;
    /* Material Property */
    int SelectedMaterialIndex = -1;
    int CurMaterialIndex = -1;
    UStaticMeshComponent* SelectedStaticMeshComp = nullptr;
    FObjMaterialInfo tempMaterialInfo;
    bool IsCreateMaterial;

    const FString TemplateFilePath = FString("Scripts/LuaTemplate.lua");
};
