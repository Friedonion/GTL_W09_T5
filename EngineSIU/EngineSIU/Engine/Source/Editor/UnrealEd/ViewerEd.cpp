#include "ViewerEd.h"
#include "EditorPanel.h"

#include "PropertyEditor/SkeletonDataPanel.h"

void ViewerEd::Initialize()
{
    auto SkeletonPanel = std::make_shared<SkeletonDataPanel>();
    Panels["SkeletonDataPanel"] = SkeletonPanel;
}

void ViewerEd::Render() const
{
    for (const auto& Panel : Panels)
    {
        Panel.Value->Render();
    }
}

void ViewerEd::AddEditorPanel(const FString& PanelId, const std::shared_ptr<UEditorPanel>& EditorPanel)
{
    Panels[PanelId] = EditorPanel;
}

void ViewerEd::OnResize(HWND hWnd) const
{
    for (auto& Panel : Panels)
    {
        Panel.Value->OnResize(hWnd);
    }
}

std::shared_ptr<UEditorPanel> ViewerEd::GetEditorPanel(const FString& PanelId)
{
    return Panels[PanelId];
}
