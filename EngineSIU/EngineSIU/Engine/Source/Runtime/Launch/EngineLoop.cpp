#include "EngineLoop.h"
#include "ImGuiManager.h"
#include "UnrealClient.h"
#include "WindowsPlatformTime.h"
#include "Audio/AudioManager.h"
#include "D3D11RHI/GraphicDevice.h"
#include "Engine/EditorEngine.h"
#include "LevelEditor/SLevelEditor.h"
#include "Slate/Widgets/Layout/SSplitter.h"
#include "UnrealEd/EditorViewportClient.h"
#include "UnrealEd/UnrealEd.h"
#include "World/World.h"
#include "Renderer/TileLightCullingPass.h"
#include "Engine/Lua/LuaScriptManager.h" 
#include "UnrealEd/EditorConfigManager.h"
#include "Games/LastWar/UI/LastWarUI.h"
#include "GameFramework/Actor.h"
#include "Classes/Actors/ASkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "FLoaderFBX.h"
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

extern FWString GViewerFilePath;
FGraphicsDevice FEngineLoop::GraphicDevice;
FRenderer FEngineLoop::Renderer;
UPrimitiveDrawBatch FEngineLoop::PrimitiveDrawBatch;
FResourceMgr FEngineLoop::ResourceManager;
uint32 FEngineLoop::TotalAllocationBytes = 0;
uint32 FEngineLoop::TotalAllocationCount = 0;

FEngineLoop::FEngineLoop()
    : AppWnd(nullptr)
    , UIMgr(nullptr)
    , LevelEditor(nullptr)
    , UnrealEditor(nullptr)
    , BufferManager(nullptr)
{
}

int32 FEngineLoop::PreInit()
{
    return 0;
}

int32 FEngineLoop::Init(HINSTANCE hInstance)
{
    FPlatformTime::InitTiming();

    /* must be initialized before window. */
    WindowInit(hInstance);

    UnrealEditor = new UnrealEd();
    BufferManager = new FDXDBufferManager();
    UIMgr = new UImGuiManager;
    AppMessageHandler = std::make_unique<FSlateAppMessageHandler>();
    LevelEditor = new SLevelEditor();
    LuaScriptManager = new FLuaScriptManager();
    

    UnrealEditor->Initialize();
    GraphicDevice.Initialize(AppWnd);
    AudioManager::Get().Initialize();

    if (!GPUTimingManager.Initialize(GraphicDevice.Device, GraphicDevice.DeviceContext))
    {
        UE_LOG(LogLevel::Error, TEXT("Failed to initialize GPU Timing Manager!"));
    }
    EngineProfiler.SetGPUTimingManager(&GPUTimingManager);

    // @todo Table에 Tree 구조로 넣을 수 있도록 수정
    EngineProfiler.RegisterStatScope(TEXT("Renderer_Render"), FName(TEXT("Renderer_Render_CPU")), FName(TEXT("Renderer_Render_GPU")));
    EngineProfiler.RegisterStatScope(TEXT("|- DepthPrePass"), FName(TEXT("DepthPrePass_CPU")), FName(TEXT("DepthPrePass_GPU")));
    EngineProfiler.RegisterStatScope(TEXT("|- TileLightCulling"), FName(TEXT("TileLightCulling_CPU")), FName(TEXT("TileLightCulling_GPU")));
    EngineProfiler.RegisterStatScope(TEXT("|- ShadowPass"), FName(TEXT("ShadowPass_CPU")), FName(TEXT("ShadowPass_GPU")));
    EngineProfiler.RegisterStatScope(TEXT("|- StaticMeshPass"), FName(TEXT("StaticMeshPass_CPU")), FName(TEXT("StaticMeshPass_GPU")));
    EngineProfiler.RegisterStatScope(TEXT("|- WorldBillboardPass"), FName(TEXT("WorldBillboardPass_CPU")), FName(TEXT("WorldBillboardPass_GPU")));
    EngineProfiler.RegisterStatScope(TEXT("|- UpdateLightBufferPass"), FName(TEXT("UpdateLightBufferPass_CPU")), FName(TEXT("UpdateLightBufferPass_GPU")));
    EngineProfiler.RegisterStatScope(TEXT("|- FogPass"), FName(TEXT("FogPass_CPU")), FName(TEXT("FogPass_GPU")));
    EngineProfiler.RegisterStatScope(TEXT("|- PostProcessCompositing"), FName(TEXT("PostProcessCompositing_CPU")), FName(TEXT("PostProcessCompositing_GPU")));
    EngineProfiler.RegisterStatScope(TEXT("|- EditorBillboardPass"), FName(TEXT("EditorBillboardPass_CPU")), FName(TEXT("EditorBillboardPass_GPU")));
    EngineProfiler.RegisterStatScope(TEXT("|- EditorRenderPass"), FName(TEXT("EditorRenderPass_CPU")), FName(TEXT("EditorRenderPass_GPU")));
    EngineProfiler.RegisterStatScope(TEXT("|- LinePass"), FName(TEXT("LinePass_CPU")), FName(TEXT("LinePass_GPU")));
    EngineProfiler.RegisterStatScope(TEXT("|- GizmoPass"), FName(TEXT("GizmoPass_CPU")), FName(TEXT("GizmoPass_GPU")));
    EngineProfiler.RegisterStatScope(TEXT("|- CompositingPass"), FName(TEXT("CompositingPass_CPU")), FName(TEXT("CompositingPass_GPU")));
    EngineProfiler.RegisterStatScope(TEXT("SlatePass"), FName(TEXT("SlatePass_CPU")), FName(TEXT("SlatePass_GPU")));

    BufferManager->Initialize(GraphicDevice.Device, GraphicDevice.DeviceContext);
    Renderer.Initialize(&GraphicDevice, BufferManager, &GPUTimingManager);
    PrimitiveDrawBatch.Initialize(&GraphicDevice);
    UIMgr->Initialize(AppWnd, GraphicDevice.Device, GraphicDevice.DeviceContext);
    ResourceManager.Initialize(&Renderer, &GraphicDevice);

    uint32 ClientWidth = 0;
    uint32 ClientHeight = 0;
    GetClientSize(ClientWidth, ClientHeight);
    LevelEditor->Initialize(ClientWidth, ClientHeight);

    GEngine = FObjectFactory::ConstructObject<UEditorEngine>(nullptr);
    GEngine->Init();
    
#ifdef _DEBUG_VIEWER
    ASkeletalMeshActor* SkeletalMeshActor = GEngine->ActiveWorld->SpawnActor<ASkeletalMeshActor>();
    SkeletalMeshActor->GetSkeletalMeshComponent()->SetSkeletalMesh(FManagerFBX::GetSkeletalMesh(GViewerFilePath));
#endif

    UpdateUI();

    return 0;
}

void FEngineLoop::Render() const
{
    GraphicDevice.Prepare();

    std::shared_ptr<FEditorViewportClient> ActiveViewportCache = GetLevelEditor()->GetActiveViewportClient();
    if (LevelEditor->IsMultiViewport())
    {
        for (int i = 0; i < 4; ++i)
        {
            LevelEditor->SetActiveViewportClient(i);
#ifdef _DEBUG_VIEWER
            Renderer.RenderViewer(LevelEditor->GetActiveViewportClient());
#else 
            Renderer.Render(LevelEditor->GetActiveViewportClient());
#endif
        }
        
        for (int i = 0; i < 4; ++i)
        {
            LevelEditor->SetActiveViewportClient(i);
            Renderer.RenderViewport(LevelEditor->GetActiveViewportClient());
        }
        GetLevelEditor()->SetActiveViewportClient(ActiveViewportCache);
    }
    else
    {
#ifdef _DEBUG_VIEWER
        Renderer.RenderViewer(LevelEditor->GetActiveViewportClient());
#else 
        Renderer.Render(LevelEditor->GetActiveViewportClient());
#endif
        Renderer.RenderViewport(ActiveViewportCache);
    }
    
}

void FEngineLoop::Tick()
{
    LARGE_INTEGER Frequency;
    const double TargetFrameTime = 1000.0 / TargetFPS; // 한 프레임의 목표 시간 (밀리초 단위)

    QueryPerformanceFrequency(&Frequency);

    LARGE_INTEGER StartTime, EndTime;
    double ElapsedTime = 0.0;

    while (bIsExit == false)
    {
        FProfilerStatsManager::BeginFrame();    // Clear previous frame stats
        if (GPUTimingManager.IsInitialized())
        {
            GPUTimingManager.BeginFrame();      // Start GPU frame timing
        }

        QueryPerformanceCounter(&StartTime);

        MSG Msg;
        while (PeekMessage(&Msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&Msg); // 키보드 입력 메시지를 문자메시지로 변경
            DispatchMessage(&Msg);  // 메시지를 WndProc에 전달

            if (Msg.message == WM_QUIT)
            {
                bIsExit = true;
                break;
            }
        }

        const float DeltaTime = static_cast<float>(ElapsedTime / 1000.f);

        AudioManager::Get().Tick();
        GEngine->Tick(DeltaTime);
        LevelEditor->Tick(DeltaTime);

        Render();
#ifdef _DEBUG_VIEWER
        UIMgr->BeginFrame();
        //UnrealEditor->Render();
        //EngineProfiler.Render(GraphicDevice.DeviceContext, GraphicDevice.ScreenWidth, GraphicDevice.ScreenHeight);
        UIMgr->EndFrame();
#else
        UIMgr->BeginFrame();

        UnrealEditor->Render();


        Console::GetInstance().Draw();
        EngineProfiler.Render(GraphicDevice.DeviceContext, GraphicDevice.ScreenWidth, GraphicDevice.ScreenHeight);

        UIMgr->EndFrame();

        // Pending 처리된 오브젝트 제거
        GUObjectArray.ProcessPendingDestroyObjects();

        if (GPUTimingManager.IsInitialized())
        {
            GPUTimingManager.EndFrame();        // End GPU frame timing
        }

        if (LuaScriptManager)
        {
            LuaScriptManager->HotReloadLuaScript();
        }
#endif

        GraphicDevice.SwapBuffer();
        do
        {
            Sleep(0);
            QueryPerformanceCounter(&EndTime);
            ElapsedTime = (static_cast<double>(EndTime.QuadPart - StartTime.QuadPart) * 1000.f / static_cast<double>(Frequency.QuadPart));
        } while (ElapsedTime < TargetFrameTime);
    }
}

void FEngineLoop::GetClientSize(uint32& OutWidth, uint32& OutHeight) const
{
    RECT ClientRect = {};
    GetClientRect(AppWnd, &ClientRect);
            
    OutWidth = ClientRect.right - ClientRect.left;
    OutHeight = ClientRect.bottom - ClientRect.top;
}

void FEngineLoop::Exit()
{
    AudioManager::Get().Release();
    LevelEditor->Release();
    UIMgr->Shutdown();
    ResourceManager.Release(&Renderer);
    Renderer.Release();
    GraphicDevice.Release();

    
    GEngine->Release();

    delete UnrealEditor;
    delete BufferManager;
    delete UIMgr;
    delete LevelEditor;
}

void FEngineLoop::WindowInit(HINSTANCE hInstance)
{
    WCHAR WindowClass[] = L"JungleWindowClass";

    WCHAR Title[] = L"Game Tech Lab";

    WNDCLASSW wc{};
    wc.lpfnWndProc = AppWndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = WindowClass;
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;

    RegisterClassW(&wc);

    AppWnd = CreateWindowExW(
        0, WindowClass, Title, WS_POPUP | WS_VISIBLE | WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1400, 1000,
        nullptr, nullptr, hInstance, nullptr
    );
}

LRESULT CALLBACK FEngineLoop::AppWndProc(HWND hWnd, uint32 Msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, Msg, wParam, lParam))
    {
        return true;
    }

    switch (Msg)
    {
    case WM_CLOSE:
        // TODO: Editor Engine이 아닌 경우에 bIsSave는 좋은 선택지가 아님. Enum으로 바꾸는 것도 괜찮아 보임
        bool bIsSave;
        if (GEngine->TryQuit(bIsSave))
        {
            if (UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine))
            {
                if (bIsSave)
                {
                    EditorEngine->SaveLevel();
                }
                
                FEditorConfigManager::GetInstance().Clear();
                EditorEngine->SaveConfig();
                if (auto LevelEditor = GEngineLoop.GetLevelEditor())
                {
                    LevelEditor->SaveConfig();
                }
                FEditorConfigManager::GetInstance().Write();
            }
        }
        else
        {
            break;
        }
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            auto LevelEditor = GEngineLoop.GetLevelEditor();
            if (LevelEditor)
            {
                FEngineLoop::GraphicDevice.Resize(hWnd);
                // FEngineLoop::Renderer.DepthPrePass->ResizeDepthStencil();
                
                uint32 ClientWidth = 0;
                uint32 ClientHeight = 0;
                GEngineLoop.GetClientSize(ClientWidth, ClientHeight);

                std::shared_ptr<FEditorViewportClient> ViewportClient = LevelEditor->GetActiveViewportClient();
                LevelEditor->ResizeEditor(ClientWidth, ClientHeight);
                FEngineLoop::Renderer.TileLightCullingPass->ResizeViewBuffers(
                  static_cast<uint32>(ViewportClient->GetD3DViewport().Width),
                    static_cast<uint32>(ViewportClient->GetD3DViewport().Height)
                );
            }
        }
        GEngineLoop.UpdateUI();
        break;
    default:
        GEngineLoop.AppMessageHandler->ProcessMessage(hWnd, Msg, wParam, lParam);
        return DefWindowProc(hWnd, Msg, wParam, lParam);
    }

    return 0;
}

void FEngineLoop::UpdateUI()
{
    Console::GetInstance().OnResize(AppWnd);
    if (GEngineLoop.GetUnrealEditor())
    {
        GEngineLoop.GetUnrealEditor()->OnResize(AppWnd);
    }
}
