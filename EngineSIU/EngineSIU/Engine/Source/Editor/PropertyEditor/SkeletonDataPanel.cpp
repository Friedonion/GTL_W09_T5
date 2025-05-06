#include "SkeletonDataPanel.h"
#include "World/World.h"
#include "Classes/Actors/ASkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/Mesh/SkeletalMesh.h"
#include "Engine/EditorEngine.h"
#include <functional>

#include "ImGUI/imgui.h"

void SkeletonDataPanel::Render()
{
    /* 패널 설정 */
    const float rightMargin = 0.0f;
    const float topMargin = 0.0f;

    ImGui::SetNextWindowPos(ImVec2(
        Width,
        topMargin
    ), ImGuiCond_Always);

    ImGui::SetNextWindowSize(ImVec2(
        500,
        800
    ), ImGuiCond_Always);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse;

    /* 본 계층 구조 표시 시작 */
    ImGui::Begin("Skeleton Hierarchy", nullptr, flags);

    UEditorEngine* Engine = Cast<UEditorEngine>(GEngine);

    /* 선택된 스켈레탈 메시 컴포넌트 가져오기 */
    AActor* SelectedActor = nullptr;

    for (AActor* Actor : Engine->ActiveWorld->GetActiveLevel()->Actors)
    {
        if (ASkeletalMeshActor* SkeletalActor = Cast<ASkeletalMeshActor>(Actor))
        {
            SelectedActor = SkeletalActor;
        }
    }

    if (!Engine || !SelectedActor)
    {
        ImGui::Text("No Skeletal Mesh Selected");
        ImGui::End();
        return;
    }

    USkeletalMeshComponent* SkeletalComp = SelectedActor->GetComponentByClass<USkeletalMeshComponent>();

    USkeletalMesh* SkeletalMesh = SkeletalComp->GetSkeletalMesh();

    if (!SkeletalMesh) 
    {
        ImGui::Text("No Skeletal Mesh Asset");
        ImGui::End();
        return;
    };

    USkeleton* Skeleton = SkeletalMesh->Skeleton;

    if (!Skeleton)
    {
        ImGui::Text("Invalid Skeleton");
        ImGui::End();
        return;
    }

    const FReferenceSkeleton& RefSkeleton = Skeleton->ReferenceSkeleton;

    /* 부모-자식 관계 맵 생성 */
    TMap<int32, TArray<int32>> ParentToChildren;
    for (int32 BoneIdx = 0; BoneIdx < RefSkeleton.BoneInfo.Num(); ++BoneIdx)
    {
        ParentToChildren.Add(BoneIdx, TArray<int32>()); // 모든 본에 엔트리 생성
    }

    for (int32 BoneIdx = 0; BoneIdx < RefSkeleton.BoneInfo.Num(); ++BoneIdx)
    {
        const int32 ParentIdx = RefSkeleton.BoneInfo[BoneIdx].ParentIndex;
        ParentToChildren.FindOrAdd(ParentIdx).Add(BoneIdx);
    }

    /* 계층 구조 표시 */
    ImGui::BeginChild("BoneTree", ImVec2(0, 0), true);

    // 재귀적 본 트리 생성 함수
    std::function<void(int32)> DrawBoneTree = [&](int32 BoneIndex)
        {
            const FBoneNode& Bone = RefSkeleton.BoneInfo[BoneIndex];
            const TArray<int32> Children = *ParentToChildren.Find(BoneIndex);

            ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow;
            
            if (Children.Num() == 0) 
            {
                nodeFlags |= ImGuiTreeNodeFlags_Leaf;
                return;
            };

            bool isOpen = ImGui::TreeNodeEx(
                (*Bone.Name.ToString()),
                nodeFlags
            );

            if (ImGui::IsItemClicked())
            {
                //Engine->SelectBone(Bone.Name);
            }

            if (isOpen)
            {
                for (int32 ChildIndex : Children)
                {
                    DrawBoneTree(ChildIndex);
                }
                ImGui::TreePop();
            }
        };

    // 루트 본 표시 (ParentIndex == INDEX_NONE)
    const TArray<int32>& RootBones = *ParentToChildren.Find(INDEX_NONE);
    for (int32 RootIndex : RootBones)
    {
        DrawBoneTree(RootIndex);
    }

    ImGui::EndChild();
    ImGui::End();
}

void SkeletonDataPanel::OnResize(HWND hWnd)
{
    RECT clientRect;
    GetClientRect(hWnd, &clientRect);
    Width = clientRect.right - clientRect.left;
    Height = clientRect.bottom - clientRect.top;
}
