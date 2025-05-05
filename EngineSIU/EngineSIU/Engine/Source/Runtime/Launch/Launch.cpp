#include "EngineLoop.h" // 에디터(엔진) 루프 헤더
#include "ViewerLoop.h" // 뷰어 루프 헤더

FViewerLoop GViewerLoop;
FEngineLoop GEngineLoop;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    // 사용 안하는 파라미터들
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nShowCmd);

    // 위에서 선택된 GAppLoop 객체를 사용
#ifdef BUILD_AS_VIEWER
// 뷰어 모드로 빌드할 경우
    GViewerLoop.Init(hInstance /*, 다른 필요한 인자들... , APP_MODE_NAME */); // Init 함수가 이름 등을 받을 수 있다면 전달
    GViewerLoop.Tick();
    GViewerLoop.Exit();
#define APP_MODE_NAME L"Viewer Mode" // 창 제목 등에 사용할 이름 (선택적)
#else
    GEngineLoop.Init(hInstance /*, 다른 필요한 인자들... , APP_MODE_NAME */); // Init 함수가 이름 등을 받을 수 있다면 전달
    GEngineLoop.Tick();
    GEngineLoop.Exit();
#define APP_MODE_NAME L"Editor Mode" // 창 제목 등에 사용할 이름 (선택적)
#endif
   

    return 0;
}
