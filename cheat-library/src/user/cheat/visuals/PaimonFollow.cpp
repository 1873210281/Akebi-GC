#include "pch-il2cpp.h"
#include "PaimonFollow.h"
#include <helpers.h>
#include <cheat/events.h>


namespace cheat::feature
{
    namespace GameObject {
        app::GameObject* Paimon = nullptr;
        app::GameObject* ProfileLayer = nullptr;
    }

    PaimonFollow::PaimonFollow() : Feature(),
        NFEX(f_Enabled, u8"派蒙跟随", "PaimonFollow", "Visuals", false, false),
        toBeUpdate(), nextUpdate(0)
    {
        events::GameUpdateEvent += MY_METHOD_HANDLER(PaimonFollow::OnGameUpdate);
    }

    const FeatureGUIInfo& PaimonFollow::GetGUIInfo() const
    {
        static const FeatureGUIInfo info{ u8"派蒙跟随", "Visuals", true };
        return info;
    }

    void PaimonFollow::DrawMain()
    {
        ConfigWidget(u8"派蒙跟随", f_Enabled, u8"要显示派蒙，请先打开该功能，然后点击esc让后将其关闭 \n" \
            u8"如果传送后派蒙消失，请不要禁用该功能，打开然后关闭esc即可。");
    }

    bool PaimonFollow::NeedStatusDraw() const
    {
        return f_Enabled;
    }

    void PaimonFollow::DrawStatus()
    {
        ImGui::Text(u8"派蒙跟随");
    }

    PaimonFollow& PaimonFollow::GetInstance()
    {
        static PaimonFollow instance;
        return instance;
    }

    void PaimonFollow::OnGameUpdate()
    {
        if (!f_Enabled)
            return;

        auto currentTime = util::GetCurrentTimeMillisec();
        if (currentTime < nextUpdate)
            return;

        if (GameObject::Paimon == nullptr) {
            GameObject::Paimon = app::GameObject_Find(string_to_il2cppi("/EntityRoot/OtherGadgetRoot/NPC_Guide_Paimon(Clone)"), nullptr);  
        }

        if (GameObject::ProfileLayer == nullptr) {
            GameObject::ProfileLayer = app::GameObject_Find(string_to_il2cppi("/Canvas/Pages/PlayerProfilePage"), nullptr);
        }
          
        if (GameObject::Paimon != nullptr && GameObject::ProfileLayer != nullptr) {
            auto ProfileOpen = app::GameObject_get_active(GameObject::ProfileLayer, nullptr);

            if (ProfileOpen) {
                app::GameObject_set_active(GameObject::Paimon, false, nullptr);
            }
            else {
                app::GameObject_set_active(GameObject::Paimon, true, nullptr);
            }
        }
        nextUpdate = currentTime + (int)f_DelayUpdate;
    }
}