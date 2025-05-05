#pragma once
#include "ViewportClient.h"
#include "Define.h"
#include "Container/Map.h"
#include "Container/Set.h"
#include "InputCore/InputCoreTypes.h"
#include "SlateCore/Input/Events.h"


class ATransformGizmo;
class USceneComponent;
enum class EViewScreenLocation : uint8;

class FEditorViewportClient : public FViewportClient
{
public:
    FEditorViewportClient();
    virtual void Tick(float DeltaTime) override;
    void Initialize(EViewScreenLocation InViewportIndex, const FRect& InRect);

    void UpdateEditorCameraMovement(float DeltaTime);
    void InputKey(const FKeyEvent& InKeyEvent);
    void MouseMove(const FPointerEvent& InMouseEvent);
    void ResizeViewport(FRect Top, FRect Bottom, FRect Left, FRect Right);

    bool IsSelected(const FVector2D& InPoint) const;


    /**
     * ScreenPos를 World Space로 Deprojection 합니다.
     * @param ScreenPos Deproject할 스크린 좌표
     * @param OutWorldOrigin Origin Vector (World Space)
     * @param OutWorldDir Direction Vector (World Space)
     */
    void DeprojectFVector2D(const FVector2D& ScreenPos, FVector& OutWorldOrigin, FVector& OutWorldDir) const;

protected:
    /** Camera speed setting */
    int32 CameraSpeedSetting = 1;
    /** Camera speed scalar */
    float CameraSpeed = 1.0f;
    float GridSize;

public:
    //Camera Movement
    void CameraMoveForward(float InValue);
    void CameraMoveRight(float InValue);
    void CameraMoveUp(float InValue);
    void CameraRotateYaw(float InValue);
    void CameraRotatePitch(float InValue);
    void PivotMoveRight(float InValue) const;
    void PivotMoveUp(float InValue) const;
    
    bool GetIsOnRBMouseClick() const { return bRightMouseDown; }

private: // Input
    POINT PrevMousePos;
    bool bRightMouseDown = false;

    // 카메라 움직임에 사용될 키를 임시로 저장해서 사용할 예정
    TSet<EKeys::Type> PressedKeys;

public:
    void LoadConfig(const TMap<FString, FString>& Config);
    void SaveConfig(TMap<FString, FString>& Config) const;

private:
    static TMap<FString, FString> ReadIniFile(const FString& FilePath);
    static void WriteIniFile(const FString& FilePath, const TMap<FString, FString>& Config);

public:
    void SetCameraSpeedSetting(const int32& value) { CameraSpeedSetting = value; }
    int32 GetCameraSpeedSetting() const { return CameraSpeedSetting; }
    void SetGridSize(const float& value) { GridSize = value; }
    float GetGridSize() const { return GridSize; }
    float GetCameraSpeedScalar() const { return CameraSpeed; }
    void SetCameraSpeed(float InValue);

public:
    // Gizmo
    // void SetGizmoActor(ATransformGizmo* gizmo) { GizmoActor = gizmo; }
    ATransformGizmo* GetGizmoActor() const { return GizmoActor; }

    void SetPickedGizmoComponent(USceneComponent* component) { PickedGizmoComponent = component; }
    USceneComponent* GetPickedGizmoComponent() const { return PickedGizmoComponent; }

    void SetShowGizmo(bool bShow) { bShowGizmo = bShow; }
    bool IsShowGizmo() const { return bShowGizmo; }

    HWND NativeWindow = nullptr;
private:
    ATransformGizmo* GizmoActor = nullptr;
    USceneComponent* PickedGizmoComponent = nullptr;
    bool bShowGizmo = true;
};
