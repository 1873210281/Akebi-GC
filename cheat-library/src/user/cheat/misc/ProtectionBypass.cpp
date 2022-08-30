#include "pch-il2cpp.h"
#include "ProtectionBypass.h"

#include <cheat/native.h>
#include <helpers.h>

namespace cheat::feature 
{
	static app::Byte__Array* RecordUserData_Hook(int32_t nType)
	{
		auto& inst = ProtectionBypass::GetInstance();

		return inst.OnRecordUserData(nType);
	}

	static int CrashReporter_Hook(__int64 a1, __int64 a2, const char* a3)
	{
		return 0;
	}

    ProtectionBypass::ProtectionBypass() : Feature(),
        NFEX(f_Enabled, u8"���ñ���", "m_DisableMhyProt", "General", true, false),
		m_CorrectSignatures({})
    {
		HookManager::install(app::Unity_RecordUserData, RecordUserData_Hook);
		HookManager::install(app::CrashReporter, CrashReporter_Hook);
    }

	void ProtectionBypass::Init()
	{
		for (int i = 0; i < 4; i++) {
			LOG_TRACE(u8"����ģ������Ϊ�ļ�¼�û����ݵĵ��� %d", i);
			app::Application_RecordUserData(i, nullptr);
		}

		// if (m_Enabled) {
			LOG_TRACE(u8"��ͼ�ر�mhy�����ֱ���");
			if (util::CloseHandleByName(L"\\Device\\mhyprot2"))
				LOG_INFO(u8"Mhy����2����ѳɹ��رա����ֺڿ�");
			else
				LOG_ERROR(u8"�޷��ر�Mhy����2��������沢���������⡣");
		//}

		LOG_DEBUG(u8"�ѳ�ʼ����");
	}

    const FeatureGUIInfo& ProtectionBypass::GetGUIInfo() const
    {
        static const FeatureGUIInfo info { "", "Settings", false };
        return info;
    }

    void ProtectionBypass::DrawMain()
    {
		ConfigWidget(f_Enabled, 
			u8"�ر�mhy����2��������Ľ���������������Ч��");
    }

    ProtectionBypass& ProtectionBypass::GetInstance()
    {
        static ProtectionBypass instance;
        return instance;
    }

	app::Byte__Array* ProtectionBypass::OnRecordUserData(int32_t nType)
	{
		if (m_CorrectSignatures.count(nType))
		{
			auto byteClass = app::GetIl2Classes()[0x25];

			auto& content = m_CorrectSignatures[nType];
			auto newArray = (app::Byte__Array*)il2cpp_array_new(byteClass, content.size());
			memmove_s(newArray->vector, content.size(), content.data(), content.size());

			return newArray;
		}

		app::Byte__Array* result = CALL_ORIGIN(RecordUserData_Hook, nType);
		auto resultArray = TO_UNI_ARRAY(result, byte);

		auto length = resultArray->length();
		if (length == 0)
			return result;

		auto stringValue = std::string((char*)result->vector, length);
		m_CorrectSignatures[nType] = stringValue;

		LOG_DEBUG(u8"��⵽���� %d ֵ����ȷǩ�� '%s'", nType, stringValue.c_str());

		return result;
	}
}

