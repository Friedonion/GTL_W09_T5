
#include "Renderer.h"
#include "World/World.h"
#include "UnrealEd/EditorViewportClient.h"
#include "D3D11RHI/DXDShaderManager.h"
#include "Engine/EditorEngine.h"
#include "RendererHelpers.h"

#include "FadeRenderpass.h"
#include "LineRenderPass.h"
#include "FogRenderPass.h"
#include "SlateRenderPass.h"
#include "EditorRenderPass.h"
#include "GizmoRenderPass.h"
#include "ShadowRenderPass.h"
#include "StaticMeshRenderPass.h"
#include "SkeletalMeshRenderPass.h"
#include "LightHeatMapRenderPass.h"
#include "WorldBillboardRenderPass.h"
#include "EditorBillboardRenderPass.h"

#include "UpdateLightBufferPass.h"
#include "DepthPrePass.h"
#include "TileLightCullingPass.h"
#include <UObject/UObjectIterator.h>
#include <UObject/Casts.h>

#include "ShowFlag.h"
#include "ShadowManager.h"
#include "UnrealClient.h"
#include "CompositingPass.h"
#include "PostProcessCompositingPass.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"

#include "GameFramework/PlayerController.h"
#include "GameFrameWork/Actor.h"

#include "Math/JungleMath.h"

#include "Stats/Stats.h"
#include "Stats/GPUTimingManager.h"

//------------------------------------------------------------------------------
// 초기화 및 해제 관련 함수
//------------------------------------------------------------------------------
void FRenderer::Initialize(FGraphicsDevice* InGraphics, FDXDBufferManager* InBufferManager, FGPUTimingManager* InGPUTimingManager)
{
    Graphics = InGraphics;
    BufferManager = InBufferManager;
    GPUTimingManager = InGPUTimingManager;

    ShaderManager = new FDXDShaderManager(Graphics->Device);
    ShadowManager = new FShadowManager();
    ShadowRenderPass = new FShadowRenderPass();

    CreateConstantBuffers();
    CreateCommonShader();
    
    StaticMeshRenderPass = new FStaticMeshRenderPass();
    SkeletalMeshRenderPass = new FSkeletalMeshRenderPass();

    WorldBillboardRenderPass = new FWorldBillboardRenderPass();
    EditorBillboardRenderPass = new FEditorBillboardRenderPass();
    GizmoRenderPass = new FGizmoRenderPass();
    UpdateLightBufferPass = new FUpdateLightBufferPass();
    LineRenderPass = new FLineRenderPass();
    FogRenderPass = new FFogRenderPass();
    EditorRenderPass = new FEditorRenderPass();
    
    DepthPrePass = new FDepthPrePass();
    TileLightCullingPass = new FTileLightCullingPass();
    LightHeatMapRenderPass = new FLightHeatMapRenderPass();
    
    CompositingPass = new FCompositingPass();
    PostProcessCompositingPass = new FPostProcessCompositingPass();
    SlateRenderPass = new FSlateRenderPass();
    FadeRenderPass = new FFadeRenderPass();

    if (false == ShadowManager->Initialize(Graphics, BufferManager))
    {
        static_assert(true, "ShadowManager Initialize Failed");
    }
    ShadowRenderPass->Initialize(BufferManager, Graphics, ShaderManager);
    ShadowRenderPass->InitializeShadowManager(ShadowManager);
    
    StaticMeshRenderPass->Initialize(BufferManager, Graphics, ShaderManager);
    StaticMeshRenderPass->InitializeShadowManager(ShadowManager);
    
    SkeletalMeshRenderPass->Initialize(BufferManager, Graphics, ShaderManager);
    SkeletalMeshRenderPass->InitializeShadowManager(ShadowManager);

    WorldBillboardRenderPass->Initialize(BufferManager, Graphics, ShaderManager);
    EditorBillboardRenderPass->Initialize(BufferManager, Graphics, ShaderManager);
    GizmoRenderPass->Initialize(BufferManager, Graphics, ShaderManager);
    UpdateLightBufferPass->Initialize(BufferManager, Graphics, ShaderManager);
    LineRenderPass->Initialize(BufferManager, Graphics, ShaderManager);
    FogRenderPass->Initialize(BufferManager, Graphics, ShaderManager);
    EditorRenderPass->Initialize(BufferManager, Graphics, ShaderManager);
    
    DepthPrePass->Initialize(BufferManager, Graphics, ShaderManager);
    TileLightCullingPass->Initialize(BufferManager, Graphics, ShaderManager);
    LightHeatMapRenderPass->Initialize(BufferManager, Graphics, ShaderManager);

    CompositingPass->Initialize(BufferManager, Graphics, ShaderManager);
    PostProcessCompositingPass->Initialize(BufferManager, Graphics, ShaderManager);
    FadeRenderPass->Initialize(BufferManager, Graphics, ShaderManager);
    
    SlateRenderPass->Initialize(BufferManager, Graphics, ShaderManager);
}

void FRenderer::Release()
{
    delete ShaderManager;
    delete ShadowManager;
    delete ShadowRenderPass;

    delete StaticMeshRenderPass;
    delete WorldBillboardRenderPass;
    delete EditorBillboardRenderPass;
    delete GizmoRenderPass;
    delete UpdateLightBufferPass;
    delete LineRenderPass;
    delete FogRenderPass;
    delete CompositingPass;
    delete PostProcessCompositingPass;
    delete SlateRenderPass;
}

//------------------------------------------------------------------------------
// 사용하는 모든 상수 버퍼 생성
//------------------------------------------------------------------------------
void FRenderer::CreateConstantBuffers()
{
    UINT CascadeBufferSize = sizeof(FCascadeConstantBuffer);
    BufferManager->CreateBufferGeneric<FCascadeConstantBuffer>("FCascadeConstantBuffer", nullptr, CascadeBufferSize, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

    UINT PointLightGSBufferSize = sizeof(FPointLightGSBuffer);
    BufferManager->CreateBufferGeneric<FPointLightGSBuffer>("FPointLightGSBuffer", nullptr, PointLightGSBufferSize, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

    UINT IsShadowBufferSize = sizeof(FIsShadowConstants);
    BufferManager->CreateBufferGeneric<FIsShadowConstants>("FIsShadowConstants", nullptr, IsShadowBufferSize, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

    UINT ShadowBufferSize = sizeof(FShadowConstantBuffer);
    BufferManager->CreateBufferGeneric<FShadowConstantBuffer>("FShadowConstantBuffer", nullptr, ShadowBufferSize, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

    UINT ObjectBufferSize = sizeof(FObjectConstantBuffer);
    BufferManager->CreateBufferGeneric<FObjectConstantBuffer>("FObjectConstantBuffer", nullptr, ObjectBufferSize, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

    UINT CameraConstantBufferSize = sizeof(FCameraConstantBuffer);
    BufferManager->CreateBufferGeneric<FCameraConstantBuffer>("FCameraConstantBuffer", nullptr, CameraConstantBufferSize, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

    UINT SubUVBufferSize = sizeof(FSubUVConstant);
    BufferManager->CreateBufferGeneric<FSubUVConstant>("FSubUVConstant", nullptr, SubUVBufferSize, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

    UINT MaterialBufferSize = sizeof(FMaterialConstants);
    BufferManager->CreateBufferGeneric<FMaterialConstants>("FMaterialConstants", nullptr, MaterialBufferSize, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

    UINT SubMeshBufferSize = sizeof(FSubMeshConstants);
    BufferManager->CreateBufferGeneric<FSubMeshConstants>("FSubMeshConstants", nullptr, SubMeshBufferSize, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

    UINT TextureBufferSize = sizeof(FTextureUVConstants);
    BufferManager->CreateBufferGeneric<FTextureUVConstants>("FTextureConstants", nullptr, TextureBufferSize, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
    
    UINT LitUnlitBufferSize = sizeof(FLitUnlitConstants);
    BufferManager->CreateBufferGeneric<FLitUnlitConstants>("FLitUnlitConstants", nullptr, LitUnlitBufferSize, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

    UINT ViewModeBufferSize = sizeof(FViewModeConstants);
    BufferManager->CreateBufferGeneric<FViewModeConstants>("FViewModeConstants", nullptr, ViewModeBufferSize, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

    UINT ScreenConstantsBufferSize = sizeof(FScreenConstants);
    BufferManager->CreateBufferGeneric<FScreenConstants>("FScreenConstants", nullptr, ScreenConstantsBufferSize, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

    UINT FogConstantBufferSize = sizeof(FFogConstants);
    BufferManager->CreateBufferGeneric<FFogConstants>("FFogConstants", nullptr, FogConstantBufferSize, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

    UINT LightInfoBufferSize = sizeof(FLightInfoBuffer);
    BufferManager->CreateBufferGeneric<FLightInfoBuffer>("FLightInfoBuffer", nullptr, LightInfoBufferSize, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);

    UINT FadeConstantBufferSize = sizeof(FFadeConstants);
    BufferManager->CreateBufferGeneric<FFadeConstants>("FFadeConstants", nullptr, FadeConstantBufferSize, D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);



    // TODO: 함수로 분리
    ID3D11Buffer* ObjectBuffer = BufferManager->GetConstantBuffer(TEXT("FObjectConstantBuffer"));
    ID3D11Buffer* CameraConstantBuffer = BufferManager->GetConstantBuffer(TEXT("FCameraConstantBuffer"));
    Graphics->DeviceContext->VSSetConstantBuffers(12, 1, &ObjectBuffer);
    Graphics->DeviceContext->VSSetConstantBuffers(13, 1, &CameraConstantBuffer);
    Graphics->DeviceContext->PSSetConstantBuffers(12, 1, &ObjectBuffer);
    Graphics->DeviceContext->PSSetConstantBuffers(13, 1, &CameraConstantBuffer);
}

void FRenderer::ReleaseConstantBuffer() const
{
    BufferManager->ReleaseConstantBuffer();
}

void FRenderer::CreateCommonShader() const
{
    D3D11_INPUT_ELEMENT_DESC StaticMeshLayoutDesc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"MATERIAL_INDEX", 0, DXGI_FORMAT_R32_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    HRESULT hr = ShaderManager->AddVertexShaderAndInputLayout(L"StaticMeshVertexShader", L"Shaders/StaticMeshVertexShader.hlsl", "mainVS", StaticMeshLayoutDesc, ARRAYSIZE(StaticMeshLayoutDesc));
    if (FAILED(hr))
    {
        return;
    }
    
#pragma region UberShader
    D3D_SHADER_MACRO DefinesGouraud[] =
    {
        { GOURAUD, "1" },
        { nullptr, nullptr }
    };
    hr = ShaderManager->AddVertexShaderAndInputLayout(L"GOURAUD_StaticMeshVertexShader", L"Shaders/StaticMeshVertexShader.hlsl", "mainVS", StaticMeshLayoutDesc, ARRAYSIZE(StaticMeshLayoutDesc), DefinesGouraud);
    if (FAILED(hr))
    {
        return;
    }
#pragma endregion UberShader
}

void FRenderer::PrepareRender(FViewportResource* ViewportResource) const
{
    // Setup Viewport
    Graphics->DeviceContext->RSSetViewports(1, &ViewportResource->GetD3DViewport());

    ViewportResource->ClearDepthStencils(Graphics->DeviceContext);
    ViewportResource->ClearRenderTargets(Graphics->DeviceContext);

    PrepareRenderPass();
}

void FRenderer::PrepareRenderPass() const
{
    StaticMeshRenderPass->PrepareRenderArr();
    SkeletalMeshRenderPass->PrepareRenderArr();
    ShadowRenderPass->PrepareRenderArr();
    GizmoRenderPass->PrepareRenderArr();
    WorldBillboardRenderPass->PrepareRenderArr();
    EditorBillboardRenderPass->PrepareRenderArr();
    UpdateLightBufferPass->PrepareRenderArr();
    FogRenderPass->PrepareRenderArr();
    EditorRenderPass->PrepareRenderArr();
    TileLightCullingPass->PrepareRenderArr();
    DepthPrePass->PrepareRenderArr();
    FadeRenderPass->PrepareRenderArr();
}

void FRenderer::ClearRenderArr() const
{
    StaticMeshRenderPass->ClearRenderArr();
    SkeletalMeshRenderPass->ClearRenderArr();
    ShadowRenderPass->ClearRenderArr();
    WorldBillboardRenderPass->ClearRenderArr();
    EditorBillboardRenderPass->ClearRenderArr();
    GizmoRenderPass->ClearRenderArr();
    UpdateLightBufferPass->ClearRenderArr();
    FogRenderPass->ClearRenderArr();
    FadeRenderPass->ClearRenderArr();
    EditorRenderPass->ClearRenderArr();
    DepthPrePass->ClearRenderArr();
    TileLightCullingPass->ClearRenderArr();
}

void FRenderer::UpdateCommonBuffer(const std::shared_ptr<FViewportClient>& Viewport) const
{
    FCameraConstantBuffer CameraConstantBuffer;
    if (GEngine->ActiveWorld->WorldType == EWorldType::Editor)
    {
        CameraConstantBuffer.ViewMatrix = Viewport->GetViewMatrix();
        CameraConstantBuffer.InvViewMatrix = FMatrix::Inverse(CameraConstantBuffer.ViewMatrix);
        CameraConstantBuffer.ProjectionMatrix = Viewport->GetProjectionMatrix();
        CameraConstantBuffer.InvProjectionMatrix = FMatrix::Inverse(CameraConstantBuffer.ProjectionMatrix);
        CameraConstantBuffer.ViewLocation = Viewport->GetCameraLocation();
        CameraConstantBuffer.NearClip = Viewport->GetNearClip();
        CameraConstantBuffer.FarClip = Viewport->GetFarClip();
    }
    else if (GEngine->ActiveWorld->WorldType == EWorldType::PIE)
    {
        auto CameraPOV = GEngine->ActiveWorld->GetFirstPlayerController()->PlayerCameraManager->GetCameraCachePOV();
        CameraConstantBuffer.ViewMatrix = JungleMath::CreateViewMatrix(
                                        CameraPOV.Location,
                                        CameraPOV.Location  + CameraPOV.Rotation.GetForwardVector(),
                                            CameraPOV.Rotation.GetUpVector()
                                    );
        CameraConstantBuffer.InvViewMatrix = FMatrix::Inverse(CameraConstantBuffer.ViewMatrix);

        if (CameraPOV.ProjectionMode == ECameraProjectionMode::Perspective)
        {
            CameraConstantBuffer.ProjectionMatrix = JungleMath::CreateProjectionMatrix(
                FMath::DegreesToRadians(CameraPOV.FOV),
                CameraPOV.AspectRatio,
                CameraPOV.PerspectiveNearClipPlane,
                CameraPOV.PerspectiveFarClipPlane
            );
            CameraConstantBuffer.NearClip = CameraPOV.PerspectiveNearClipPlane;
            CameraConstantBuffer.FarClip = CameraPOV.PerspectiveFarClipPlane;
        }
        else if (CameraPOV.ProjectionMode == ECameraProjectionMode::Orthographic)
        {
            // 오쏘그래픽 너비는 줌 값과 가로세로 비율에 따라 결정됩니다.
            const float OrthoWidth = CameraPOV.OrthoWidth;
            const float OrthoHeight = CameraPOV.OrthoWidth / CameraPOV.AspectRatio;

            // 오쏘그래픽 투영 행렬 생성 (nearPlane, farPlane 은 기존 값 사용)
            CameraConstantBuffer.ProjectionMatrix = JungleMath::CreateOrthoProjectionMatrix(
                OrthoWidth,
                OrthoHeight,
                CameraPOV.OrthoNearClipPlane,
                CameraPOV.OrthoFarClipPlane
            );

            CameraConstantBuffer.NearClip = CameraPOV.OrthoNearClipPlane;
            CameraConstantBuffer.FarClip = CameraPOV.OrthoFarClipPlane;
        }
        CameraConstantBuffer.InvProjectionMatrix = FMatrix::Inverse(CameraConstantBuffer.ProjectionMatrix);
        CameraConstantBuffer.ViewLocation = CameraPOV.Location;
    }
    BufferManager->UpdateConstantBuffer("FCameraConstantBuffer", CameraConstantBuffer);
}

void FRenderer::BeginRender(const std::shared_ptr<FViewportClient>& Viewport)
{
    FViewportResource* ViewportResource = Viewport->GetViewportResource();
    if (!ViewportResource)
    {
        return;
    }

    UpdateCommonBuffer(Viewport);
    
    PrepareRender(ViewportResource);
}


void FRenderer::Render(const std::shared_ptr<FViewportClient>& Viewport)
{
    if (!GPUTimingManager || !GPUTimingManager->IsInitialized())
    {
        return;
    }

    QUICK_SCOPE_CYCLE_COUNTER(Renderer_Render_CPU)
    QUICK_GPU_SCOPE_CYCLE_COUNTER(Renderer_Render_GPU, *GPUTimingManager)

    BeginRender(Viewport);

    /**
     * 각 렌더 패스의 시작과 끝은 필요한 리소스를 바인딩하고 해제하는 것까지입니다.
     * 다음에 작동할 렌더 패스에서는 이전에 사용했던 리소스들을 충돌 없이 바인딩 할 수 있어야 한다는 의미입니다.
     * e.g.
     *   1번 렌더 패스: 여기에서 사용했던 RTV를 마지막에 해제함으로써, 해당 RTV와 연결된 텍스처를 쉐이더 리소스로 사용할 수 있습니다.
     *   2번 렌더 패스: 1번 렌더 패스에서 렌더한 결과 텍스처를 쉐이더 리소스로 사용할 수 있습니다.
     *
     * 경우에 따라(연속적인 렌더 패스에서 동일한 리소스를 사용하는 경우) 바인딩 및 해제 작업을 생략하는 것도 가능하지만,
     * 다음 전제 조건을 지켜주어야 합니다.
     *   1. 렌더 패스는 엄격하게 순차적으로 실행됨
     *   2. 렌더 타겟의 생명주기와 용도가 명확함
     *   3. RTV -> SRV 전환 타이밍이 정확히 지켜짐
     */

	if (DepthPrePass) // Depth Pre Pass : 렌더타겟 nullptr 및 렌더 후 복구
    {
        QUICK_SCOPE_CYCLE_COUNTER(DepthPrePass_CPU)
        QUICK_GPU_SCOPE_CYCLE_COUNTER(DepthPrePass_GPU, *GPUTimingManager)
        DepthPrePass->Render(Viewport);
    }

    // Added Compute Shader Pass
    if (TileLightCullingPass)
    {
        QUICK_SCOPE_CYCLE_COUNTER(TileLightCulling_CPU)
        QUICK_GPU_SCOPE_CYCLE_COUNTER(TileLightCulling_GPU, *GPUTimingManager)
        TileLightCullingPass->Render(Viewport);

        // 이후 패스에서 사용할 수 있도록 리소스 생성
        LightHeatMapRenderPass->SetDebugHeatmapSRV(TileLightCullingPass->GetDebugHeatmapSRV());
        // @todo UpdateLightBuffer에서 병목 발생 -> 필요한 라이트에 대하여만 업데이트 필요, Tiled Culling으로 GPU->CPU 전송은 주객전도
        UpdateLightBufferPass->SetLightData(TileLightCullingPass->GetPointLights(), TileLightCullingPass->GetSpotLights(),
                                TileLightCullingPass->GetPerTilePointLightIndexMaskBufferSRV(), TileLightCullingPass->GetPerTileSpotLightIndexMaskBufferSRV());
        UpdateLightBufferPass->SetTileConstantBuffer(TileLightCullingPass->GetTileConstantBuffer());
    }

    if (Viewport->GetViewMode() != EViewModeIndex::VMI_Unlit)
    {
        QUICK_SCOPE_CYCLE_COUNTER(ShadowPass_CPU)
        QUICK_GPU_SCOPE_CYCLE_COUNTER(ShadowPass_GPU, *GPUTimingManager)
        ShadowRenderPass->SetLightData(TileLightCullingPass->GetPointLights(), TileLightCullingPass->GetSpotLights());
        ShadowRenderPass->Render(Viewport);
    }

    RenderWorldScene(Viewport);
    RenderPostProcess(Viewport);
    RenderEditorOverlay(Viewport);

    Graphics->DeviceContext->PSSetShaderResources(
        static_cast<UINT>(EShaderSRVSlot::SRV_Debug),
        1,
        &TileLightCullingPass->GetDebugHeatmapSRV()
    ); // TODO: 최악의 코드

    // Compositing: 위에서 렌더한 결과들을 하나로 합쳐서 뷰포트의 최종 이미지를 만드는 작업
    {
        QUICK_SCOPE_CYCLE_COUNTER(CompositingPass_CPU)
        QUICK_GPU_SCOPE_CYCLE_COUNTER(CompositingPass_GPU, *GPUTimingManager);
        CompositingPass->Render(Viewport);
    }

    EndRender();
}

void FRenderer::RenderViewer(const std::shared_ptr<FViewportClient>& Viewport)
{
    if (!GPUTimingManager || !GPUTimingManager->IsInitialized())
    {
        return;
    }

    QUICK_SCOPE_CYCLE_COUNTER(Renderer_Render_CPU)
        QUICK_GPU_SCOPE_CYCLE_COUNTER(Renderer_Render_GPU, *GPUTimingManager)

        BeginRender(Viewport);

    /**
     * 각 렌더 패스의 시작과 끝은 필요한 리소스를 바인딩하고 해제하는 것까지입니다.
     * 다음에 작동할 렌더 패스에서는 이전에 사용했던 리소스들을 충돌 없이 바인딩 할 수 있어야 한다는 의미입니다.
     * e.g.
     *   1번 렌더 패스: 여기에서 사용했던 RTV를 마지막에 해제함으로써, 해당 RTV와 연결된 텍스처를 쉐이더 리소스로 사용할 수 있습니다.
     *   2번 렌더 패스: 1번 렌더 패스에서 렌더한 결과 텍스처를 쉐이더 리소스로 사용할 수 있습니다.
     *
     * 경우에 따라(연속적인 렌더 패스에서 동일한 리소스를 사용하는 경우) 바인딩 및 해제 작업을 생략하는 것도 가능하지만,
     * 다음 전제 조건을 지켜주어야 합니다.
     *   1. 렌더 패스는 엄격하게 순차적으로 실행됨
     *   2. 렌더 타겟의 생명주기와 용도가 명확함
     *   3. RTV -> SRV 전환 타이밍이 정확히 지켜짐
     */

    if (DepthPrePass) // Depth Pre Pass : 렌더타겟 nullptr 및 렌더 후 복구
    {
        QUICK_SCOPE_CYCLE_COUNTER(DepthPrePass_CPU)
            QUICK_GPU_SCOPE_CYCLE_COUNTER(DepthPrePass_GPU, *GPUTimingManager)
            DepthPrePass->Render(Viewport);
    }


   /* if (Viewport->GetViewMode() != EViewModeIndex::VMI_Unlit)
    {
        QUICK_SCOPE_CYCLE_COUNTER(ShadowPass_CPU)
            QUICK_GPU_SCOPE_CYCLE_COUNTER(ShadowPass_GPU, *GPUTimingManager)
            ShadowRenderPass->SetLightData(TileLightCullingPass->GetPointLights(), TileLightCullingPass->GetSpotLights());
        ShadowRenderPass->Render(Viewport);
    }*/

    RenderWorldScene(Viewport);
    //RenderPostProcess(Viewport);
    RenderEditorOverlay(Viewport);

 /*   Graphics->DeviceContext->PSSetShaderResources(
        static_cast<UINT>(EShaderSRVSlot::SRV_Debug),
        1,
        &TileLightCullingPass->GetDebugHeatmapSRV()
    );*/ // TODO: 최악의 코드

    // Compositing: 위에서 렌더한 결과들을 하나로 합쳐서 뷰포트의 최종 이미지를 만드는 작업
    {
        QUICK_SCOPE_CYCLE_COUNTER(CompositingPass_CPU)
            QUICK_GPU_SCOPE_CYCLE_COUNTER(CompositingPass_GPU, *GPUTimingManager);
        CompositingPass->Render(Viewport);
    }

    EndRender();
}


void FRenderer::EndRender()
{
    ClearRenderArr();
    ShaderManager->ReloadAllShaders(); // 
}

void FRenderer::RenderWorldScene(const std::shared_ptr<FViewportClient>& Viewport) const
{
    const uint64 ShowFlag = Viewport->GetShowFlag();
    
    if (ShowFlag & EEngineShowFlags::SF_Primitives)
    {
        {
            QUICK_SCOPE_CYCLE_COUNTER(UpdateLightBufferPass_CPU)
            QUICK_GPU_SCOPE_CYCLE_COUNTER(UpdateLightBufferPass_GPU, *GPUTimingManager)
            UpdateLightBufferPass->Render(Viewport);
        }
        {
            QUICK_SCOPE_CYCLE_COUNTER(StaticMeshPass_CPU)
            QUICK_GPU_SCOPE_CYCLE_COUNTER(StaticMeshPass_GPU, *GPUTimingManager)
            StaticMeshRenderPass->Render(Viewport);
            SkeletalMeshRenderPass->Render(Viewport);
        }
    }
    
    // Render World Billboard
    if (ShowFlag & EEngineShowFlags::SF_BillboardText)
    {
        {
            QUICK_SCOPE_CYCLE_COUNTER(WorldBillboardPass_CPU)
            QUICK_GPU_SCOPE_CYCLE_COUNTER(WorldBillboardPass_GPU, *GPUTimingManager)
            WorldBillboardRenderPass->Render(Viewport);
        }
    }
}

void FRenderer::RenderPostProcess(const std::shared_ptr<FViewportClient>& Viewport) const
{
    const uint64 ShowFlag = Viewport->GetShowFlag();
    const EViewModeIndex ViewMode = Viewport->GetViewMode();
    
    if (ViewMode >= EViewModeIndex::VMI_Unlit)
    {
        return;
    }
    
    if (ShowFlag & EEngineShowFlags::SF_Fog)
    {
        QUICK_SCOPE_CYCLE_COUNTER(FogPass_CPU)
        QUICK_GPU_SCOPE_CYCLE_COUNTER(FogPass_GPU, *GPUTimingManager)
        FogRenderPass->Render(Viewport);
        /**
         * TODO: Fog 렌더 작업 해야 함.
         * 여기에서는 씬 렌더가 적용된 뎁스 스텐실 뷰를 SRV로 전달하고, 뎁스 스텐실 뷰를 아래에서 다시 써야함.
         */
    }

    FadeRenderPass->Render(Viewport);

    // TODO: 포스트 프로세스 별로 각자의 렌더 타겟 뷰에 렌더하기

    /**
     * TODO: 반드시 씬에 먼저 반영되어야 하는 포스트 프로세싱 효과는 먼저 씬에 반영하고,
     *       그 외에는 렌더한 포스트 프로세싱 효과들을 이 시점에서 하나로 합친 후에, 다음에 올 컴포짓 과정에서 합성.
     */

    {
        QUICK_SCOPE_CYCLE_COUNTER(PostProcessCompositing_CPU)
        QUICK_GPU_SCOPE_CYCLE_COUNTER(PostProcessCompositing_GPU, *GPUTimingManager)
        PostProcessCompositingPass->Render(Viewport);
    }
    
    Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
}

void FRenderer::RenderEditorOverlay(const std::shared_ptr<FViewportClient>& Viewport) const
{
    const uint64 ShowFlag = Viewport->GetShowFlag();
    const EViewModeIndex ViewMode = Viewport->GetViewMode();
    
    if (GEngine->ActiveWorld->WorldType != EWorldType::Editor)
    {
        return;
    }
    
    // Render Editor Billboard
    /**
     * TODO: 에디터 전용 빌보드는 이런 방식 처럼 빌보드의 bool값을 바꿔서 렌더하기 보다는
     *       빌보드가 나와야 하는 컴포넌트는 텍스처만 가지고있고, 쉐이더를 통해 쿼드를 생성하고
     *       텍스처를 전달해서 렌더하는 방식이 더 좋음.
     *       이렇게 하는 경우 필요없는 빌보드 컴포넌트가 아웃라이너에 나오지 않음.
     */
    if (ShowFlag & EEngineShowFlags::SF_BillboardText)
    {
        QUICK_SCOPE_CYCLE_COUNTER(EditorBillboardPass_CPU)
        QUICK_GPU_SCOPE_CYCLE_COUNTER(EditorBillboardPass_GPU, *GPUTimingManager)
        EditorBillboardRenderPass->Render(Viewport);
    }

    {
        QUICK_SCOPE_CYCLE_COUNTER(EditorRenderPass_CPU)
        QUICK_GPU_SCOPE_CYCLE_COUNTER(EditorRenderPass_GPU, *GPUTimingManager)
        EditorRenderPass->Render(Viewport); // TODO: 임시로 이전에 작성되었던 와이어 프레임 렌더 패스이므로, 이후 개선 필요.
    }
    {
        QUICK_SCOPE_CYCLE_COUNTER(LinePass_CPU)
        QUICK_GPU_SCOPE_CYCLE_COUNTER(LinePass_GPU, *GPUTimingManager)
        LineRenderPass->Render(Viewport); // 기존 뎁스를 그대로 사용하지만 뎁스를 클리어하지는 않음
    }
    {
        QUICK_SCOPE_CYCLE_COUNTER(GizmoPass_CPU)
        QUICK_GPU_SCOPE_CYCLE_COUNTER(GizmoPass_GPU, *GPUTimingManager)
        GizmoRenderPass->Render(Viewport); // 기존 뎁스를 SRV로 전달해서 샘플 후 비교하기 위해 기즈모 전용 DSV 사용
    }

    Graphics->DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
}

void FRenderer::RenderViewport(const std::shared_ptr<FViewportClient>& Viewport) const
{
    QUICK_SCOPE_CYCLE_COUNTER(SlatePass_CPU)
    QUICK_GPU_SCOPE_CYCLE_COUNTER(SlatePass_GPU, *GPUTimingManager)
    SlateRenderPass->Render(Viewport);
}
