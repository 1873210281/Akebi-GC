#include "pch-il2cpp.h"
#include "OpenTeamImmediately.h"

#include <helpers.h>

namespace cheat::feature
{
	static bool MoleMole_InLevelMainPageContext_DoTeamCountDown_c_Iterator0__MoveNext_Hook(app::InLevelMainPageContext_DoTeamCountDown_Iterator* __this, MethodInfo* method);
	static void MoleMole_InLevelPlayerProfilePageContext_ClearView_Hook(void* __this, MethodInfo* method);
	static void MoleMole_InLevelPlayerProfilePageContext_SetupView_Hook(void* __this, MethodInfo* method);


	OpenTeamImmediately::OpenTeamImmediately() : Feature(),
		NF(f_Enabled, u8"秒打开队伍切换·", "Open Team", false),
		InLevelPlayerProfilePageContext(nullptr)
	{
		HookManager::install(app::MoleMole_InLevelMainPageContext_DoTeamCountDown_c_Iterator0__MoveNext, MoleMole_InLevelMainPageContext_DoTeamCountDown_c_Iterator0__MoveNext_Hook);
		HookManager::install(app::MoleMole_InLevelPlayerProfilePageContext_ClearView, MoleMole_InLevelPlayerProfilePageContext_ClearView_Hook);
		HookManager::install(app::MoleMole_InLevelPlayerProfilePageContext_SetupView, MoleMole_InLevelPlayerProfilePageContext_SetupView_Hook);
	}

	const FeatureGUIInfo& OpenTeamImmediately::GetGUIInfo() const
	{
		static const FeatureGUIInfo info{ "", "World", false };
		return info;
	}

	void OpenTeamImmediately::DrawMain()
	{
		ConfigWidget(u8"秒打开队伍切换", f_Enabled, u8"立即打开团队，无需等待倒计时");
	}

	bool OpenTeamImmediately::NeedStatusDraw() const
	{
		return f_Enabled;
	}

	void OpenTeamImmediately::DrawStatus()
	{
		ImGui::Text("OpenTeamImmediately");
	}

	OpenTeamImmediately& OpenTeamImmediately::GetInstance()
	{
		static OpenTeamImmediately instance;
		return instance;
	}

	static bool MoleMole_InLevelMainPageContext_DoTeamCountDown_c_Iterator0__MoveNext_Hook(app::InLevelMainPageContext_DoTeamCountDown_Iterator* __this, MethodInfo* method)
	{
		auto& openTeamImmediately = OpenTeamImmediately::GetInstance();

		// if paimon menu open, team ui will not open
		if (openTeamImmediately.f_Enabled && !openTeamImmediately.InLevelPlayerProfilePageContext)
		{
			__this->fields._levelMainPageContext->fields._countDownTime = __this->fields._levelMainPageContext->fields.EnterCountDown + 1.f;
		}
		return CALL_ORIGIN(MoleMole_InLevelMainPageContext_DoTeamCountDown_c_Iterator0__MoveNext_Hook, __this, method);
	}

	static void MoleMole_InLevelPlayerProfilePageContext_SetupView_Hook(void* __this, MethodInfo* method)
	{
		auto& openTeamImmediately = OpenTeamImmediately::GetInstance();
		openTeamImmediately.InLevelPlayerProfilePageContext = __this;
		CALL_ORIGIN(MoleMole_InLevelPlayerProfilePageContext_SetupView_Hook, __this, method);
	}

	static void MoleMole_InLevelPlayerProfilePageContext_ClearView_Hook(void* __this, MethodInfo* method)
	{
		auto& openTeamImmediately = OpenTeamImmediately::GetInstance();
		CALL_ORIGIN(MoleMole_InLevelPlayerProfilePageContext_ClearView_Hook, __this, method);
		openTeamImmediately.InLevelPlayerProfilePageContext = nullptr;
	}
}