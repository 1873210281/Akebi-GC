#include "pch-il2cpp.h"
#include "CustomTeleports.h"

#include <helpers.h>
#include <cheat/events.h>
#include <cheat/game/EntityManager.h>
#include "MapTeleport.h"
#include <cheat/game/util.h>
#include <misc/cpp/imgui_stdlib.h>
#include <filesystem>
#include <fstream>
#include <regex>
#include <imgui_internal.h>
#include "shlwapi.h"
#pragma comment(lib, "shlwapi.lib")

namespace cheat::feature
{
	CustomTeleports::CustomTeleports() : Feature(),
		NF(f_Enabled,		"Custom Teleport",		"CustomTeleports",	false),
		NF(f_Next,			"Teleport Next",		"CustomTeleports",	Hotkey(VK_OEM_6)),
		NF(f_Previous,		"Teleport Previous",	"CustomTeleports",	Hotkey(VK_OEM_4)),
		NF(f_Auto,			"Auto Teleport",		"CustomTeleports",	false),
		NF(f_DelayTime,		"Delay time (in s)",	"CustomTeleports",	20),
		NF(f_Interpolate,	"Interpolate Teleport",	"CustomTeleports",	false),
		NF(f_Speed,			"Interpolation Speed",	"CustomTeleports",	10.0f),
		dir(util::GetCurrentPath() /= "teleports"),
		nextTime(0)
	{
		f_Next.value().PressedEvent += MY_METHOD_HANDLER(CustomTeleports::OnNext);
		f_Previous.value().PressedEvent += MY_METHOD_HANDLER(CustomTeleports::OnPrevious);
		events::GameUpdateEvent += MY_METHOD_HANDLER(CustomTeleports::OnGameUpdate);
	}

	const FeatureGUIInfo& CustomTeleports::GetGUIInfo() const
	{
		static const FeatureGUIInfo info{ u8"自定义传送", "Teleport", true };
		return info;
	}

	void CustomTeleports::CheckFolder()
	{
		if (!std::filesystem::exists(dir))
			std::filesystem::create_directory(dir);
		else return;
	}

	bool CustomTeleports::ValidateTeleport(std::string name)
	{
		for (auto &Teleport : Teleports)
			if (Teleport.name == name)
				return false;
		if (name.find_first_of("\\/:*?\"<>|") != std::string::npos)
				return false;
		return true;
	}

	Teleport CustomTeleports::Teleport_(std::string name, app::Vector3 position, std::string description)
	{
		Teleport t(name, position, description);
		return t;
	}

	void CustomTeleports::SerializeTeleport(Teleport t)
	{
		Teleports.push_back(t);
		LOG_INFO("Teleport '%s' Loaded", t.name.c_str());
		CheckFolder();
		std::ofstream ofs(dir / (t.name + ".json"));
		nlohmann::json j;
		try
		{
			j["name"] = t.name;
			j["position"] = {t.position.x, t.position.y, t.position.z};
			j["description"] = t.description;
			ofs << j;
			ofs.close();
			LOG_INFO("Teleport '%s' Serialized.", t.name.c_str());
		} catch (std::exception e)
		{
			ofs.close();
			LOG_ERROR("Failed to serialize teleport: %s: %s", t.name.c_str(), e.what());
		}
	}

	 std::optional<Teleport> CustomTeleports::SerializeFromJson(std::string json, bool fromfile)
	{
		nlohmann::json j;
		try { j = nlohmann::json::parse(json); }
		catch (nlohmann::json::parse_error& e)
		{
			LOG_ERROR("Invalid JSON Format");
			LOG_ERROR("Failed to parse JSON: %s", e.what());
			return std::nullopt;
		}

		std::string teleportName;

		teleportName = j["name"];
		if (j["name"].is_null() && fromfile)
		{
			LOG_ERROR("No name found! Using File Name");
			teleportName = std::filesystem::path(json).stem().filename().string();
		}
		std::string description;
		if (j["description"].is_null()) description = "";
		else description = j["description"];
		return Teleport_(teleportName, { j["position"][0], j["position"][1], j["position"][2] }, description);

	}
	
	void CustomTeleports::ReloadTeleports()
	{
		auto result = std::filesystem::directory_iterator(dir);
		Teleports.clear();

		for (auto &file : result)
		{
			if (file.path().extension() == ".json")
			{
				std::ifstream ifs(file.path());
				std::string json;
				std::getline(ifs, json);
				auto t = SerializeFromJson(json, true);
				if(t.has_value()) SerializeTeleport(t.value());
			}
		}
	}

	float PositionDistance(app::Vector3 a, app::Vector3 b)
	{
		return sqrt(pow(a.x - b.x, 2) + pow(a.y - b.y, 2) + pow(a.z - b.z, 2));
	}

	void CustomTeleports::TeleportTo(app::Vector3 position, bool interpolate)
	{
		auto &manager = game::EntityManager::instance();
		auto avatar = manager.avatar();
		if (avatar->moveComponent() == nullptr)
		{
			LOG_ERROR("Avatar has no move component, Is scene loaded?");
			return;
		}
		if (interpolate)
		{
			float speed = this->f_Speed;
			auto avatarPos = manager.avatar()->absolutePosition();
			auto endPos = position;
			std::thread interpolate([avatarPos, endPos, &manager, speed]()
									{
                            float t = 0.0f;
							app::Vector3 zero = {0,0,0};
							auto newPos = zero;
                            while (t < 1.0f) {
                                newPos = app::Vector3_Lerp(avatarPos, endPos, t, nullptr);
                                manager.avatar()->setAbsolutePosition(newPos);
								t += speed / 100.0f;
                                Sleep(10); 
                            } });
			interpolate.detach();
		}
		else
		{
			if (PositionDistance(position, app::ActorUtils_GetAvatarPos(nullptr)) > 60.0f)
				MapTeleport::GetInstance().TeleportTo(position);
			else
				manager.avatar()->setAbsolutePosition(position);
		}
	}

	void CustomTeleports::OnTeleportKeyPressed(bool next)
	{
		if (!f_Enabled || selectedIndex < 0)
			return;

		auto &mapTeleport = MapTeleport::GetInstance();
		app::Vector3 position;

		if (selectedByClick)
		{
			position = Teleports.at(selectedIndex).position;
			selectedByClick = false;
		}
		else
		{
			std::vector list(checkedIndices.begin(), checkedIndices.end());
			if (next ?  selectedIndex == list.back() : selectedIndex == list.front())
				return;

			auto index = std::distance(list.begin(), std::find(list.begin(), list.end(), selectedIndex));
			selectedIndex = list.at(index + (next ? 1 : -1));
			position = Teleports.at(selectedIndex).position;
		}
		TeleportTo(position, this->f_Interpolate);
		UpdateIndexName();
	}

	void CustomTeleports::OnPrevious()
	{
		if (f_Auto) return;
		OnTeleportKeyPressed(false);
	}
	void CustomTeleports::OnNext()
	{
		if (f_Auto) return;
		OnTeleportKeyPressed(true);
	}

	void CustomTeleports::OnGameUpdate()
	{
		if (!f_Enabled || !f_Auto)
			return;

		auto currentTime = util::GetCurrentTimeMillisec();
		if (currentTime < nextTime)
			return;

		auto loadingManager = GET_SINGLETON(MoleMole_LoadingManager);
		if (loadingManager == nullptr || !app::MoleMole_LoadingManager_IsLoaded(loadingManager, nullptr))
			return;

		auto camera = app::Camera_get_main(nullptr);
		if (camera == nullptr) return;

		if (!app::Behaviour_get_isActiveAndEnabled(reinterpret_cast<app::Behaviour*>(camera), nullptr))
			return;

		nextTime = currentTime + (int64_t)f_DelayTime * 1000;
		OnTeleportKeyPressed(true); 
	}

	void itr(std::regex exp, std::string name, std::string s)
	{
		std::sregex_iterator itr(name.begin(), name.end(), exp);
		while (itr != std::sregex_iterator())
		{
			for (unsigned i = 0; i < itr->size(); i++)
				s.append((*itr)[i]);
			itr++;
		}
	}

	void CustomTeleports::UpdateIndexName()
	{
		// abbreviate teleport names that are too long
		std::string name(selectedIndex == -1 || checkedIndices.empty() ? "" : Teleports.at(selectedIndex).name);
		if (name.length() > 15)
		{
			std::string shortened;
			std::regex numsExp("[\\d]+");
			std::regex firstCharsExp("\\b[A-Za-z]");
			itr(firstCharsExp, name, shortened);
			itr(numsExp, name, shortened);
			name = shortened;
		}
		selectedIndexName = name;
	}

	void CustomTeleports::DrawMain()
	{
		// Buffers
		static std::string nameBuffer_;
		static std::string searchBuffer_;
		static std::string JSONBuffer_;
		static std::string descriptionBuffer_;

		ImGui::InputText(u8"名字", &nameBuffer_);
		ImGui::InputText(u8"描述", &descriptionBuffer_);
		if (ImGui::Button(u8"添加传送"))
		{
			selectedIndex = -1;
			UpdateIndexName();
			SerializeTeleport(Teleport_(nameBuffer_, app::ActorUtils_GetAvatarPos(nullptr), descriptionBuffer_));
			nameBuffer_ = "";
			descriptionBuffer_ = "";
		}
		ImGui::SameLine();

		if (ImGui::Button(u8"重载"))
		{
			selectedIndex = -1;
			UpdateIndexName();
			checkedIndices.clear();
			ReloadTeleports();
		}

		ImGui::SameLine();
		if (ImGui::Button(u8"打开文件夹"))
		{
			CheckFolder();
			ShellExecuteA(NULL, "open", dir.string().c_str(), NULL, NULL, SW_SHOW);
		}

		ImGui::SameLine();
		if (ImGui::Button(u8"加载从JSON"))
		{
			if (!JSONBuffer_.empty()) {
				auto t = SerializeFromJson(JSONBuffer_, false);
				if (t.has_value()) {
					selectedIndex = -1;
					UpdateIndexName();
					SerializeTeleport(t.value());
				}
				JSONBuffer_.clear();
			}

		}
		ImGui::InputTextMultiline(u8"JSON输入", &JSONBuffer_, ImVec2(0, 50), ImGuiInputTextFlags_AllowTabInput);

		ConfigWidget(u8"传送下一个", f_Next, true, "Press to teleport next of selected.");
		ConfigWidget(u8"传送上一个", f_Previous, true, "Press to teleport previous of selected.");
		ConfigWidget(u8"开/关", f_Enabled,
					 u8"通过列表功能启用远程传输。\n"
					 u8"用法：\n"
					 u8"1.使用热键在你想要传送的传送点上打勾\n"
					 u8"2.单击传送（带复选标记）以选择要开始的位置\n"
					 u8"3.您现在可以按“下一步”或“上一步”热键传送检查表\n"
					 u8"最初，它会将玩家传送到所选位置\n"
					 u8"注意：双击或单击箭头打开传送详细信息");
		ConfigWidget(u8"启用插值", f_Interpolate, u8"使用键绑定时启用远程传送之间的插值。"); ImGui::SameLine(); ImGui::SetNextItemWidth(300.0f);
		ConfigWidget(u8"插补速度", f_Speed, 0.1f, 0.1f, 99.0f,
					 u8"插值速度。\n 建议设置低于或等于0.1。");
		ConfigWidget(u8"自动传送", f_Auto, u8"启用远程传送之间的自动正向远程传送"); ImGui::SameLine(); ImGui::SetNextItemWidth(300.0f);
		ConfigWidget(u8"延迟 (s)", f_DelayTime, 1, 0, 60, u8"传送之间的延迟（秒）。\n"
			u8"注意：这不是完全测试的检测方式。\n不建议使用低值。");

		if (ImGui::Button("Delete Checked"))
		{
			if (!Teleports.empty())
			{
				if (checkedIndices.empty())
				{
					LOG_INFO(u8"未选择远程传送");
					return;
				}
				std::vector<std::string> teleportNames;
				for (auto &Teleport : Teleports)
					teleportNames.push_back(Teleport.name);
				for (auto &index : checkedIndices)
				{
					std::filesystem::remove(dir / (teleportNames[index] + ".json"));
					LOG_INFO(u8"删除传送 %s", teleportNames[index].c_str());
				}
				checkedIndices.clear();
				UpdateIndexName();
				ReloadTeleports();
			} else {LOG_INFO(u8"没有要删除的远程传送");}
		}
		ImGui::SameLine();
		HelpMarker(u8"警告：这将从目录中删除文件，并\n \
		从列表中删除传送。它将永远丢失。");

		if (ImGui::TreeNode(u8"传送"))
		{
			std::sort(Teleports.begin(), Teleports.end(), [](const auto &a, const auto &b)
					  { return StrCmpLogicalW(std::wstring(a.name.begin(), a.name.end()).c_str(), std::wstring(b.name.begin(), b.name.end()).c_str()) < 0; });
			bool allSearchChecked = std::includes(checkedIndices.begin(), checkedIndices.end(), searchIndices.begin(), searchIndices.end()) && !searchIndices.empty();
			bool allChecked = (checkedIndices.size() == Teleports.size() && !Teleports.empty()) || allSearchChecked;
			ImGui::Checkbox("All", &allChecked);
			if (ImGui::IsItemClicked())
			{
				if (!Teleports.empty())
				{
					if (allChecked)
					{
						selectedIndex = -1;
						if (!searchIndices.empty())
							for (const auto &i : searchIndices)
								checkedIndices.erase(i);
						else
							checkedIndices.clear();
					}
					else if (!searchIndices.empty())
						checkedIndices.insert(searchIndices.begin(), searchIndices.end());
					else
						for (int i = 0; i < Teleports.size(); i++)
							checkedIndices.insert(i);
					UpdateIndexName();
				}
			}
			ImGui::SameLine();
			ImGui::InputText("Search", &searchBuffer_);
			unsigned int index = 0;
			searchIndices.clear();

			unsigned int maxNameLength = 0;
			for (auto &Teleport : Teleports)
				if (Teleport.name.length() > maxNameLength)
					maxNameLength = Teleport.name.length();
			ImGui::BeginTable("Teleports", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable | ImGuiTableFlags_NoSavedSettings);
			ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 20);
			ImGui::TableSetupColumn("Commands", ImGuiTableColumnFlags_WidthFixed, 130);
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, maxNameLength * 8 + 10);
			ImGui::TableSetupColumn("Position");
			ImGui::TableHeadersRow();

			for (const auto &[name, position, description] : Teleports)
			{
				if (searchBuffer_.empty() || std::search(name.begin(), name.end(), searchBuffer_.begin(), searchBuffer_.end(), [](char a, char b)
														 { return std::tolower(a) == std::tolower(b); }) != name.end())
				{
					if (!searchBuffer_.empty())
						searchIndices.insert(index);
					bool checked = std::any_of(checkedIndices.begin(), checkedIndices.end(), [&index](const auto &i)
											   { return i == index; });
					bool selected = index == selectedIndex;
					std::string stringIndex = std::to_string(index);

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::Text("%d", index);
					ImGui::TableNextColumn();
					ImGui::Checkbox(("##Index" + stringIndex).c_str(), &checked);
					if (ImGui::IsItemClicked(0))
					{
						if (checked)
						{
							if (selected)
								selectedIndex = -1;
							checkedIndices.erase(index);
						}
						else
							checkedIndices.insert(index);
						UpdateIndexName();
					}

					ImGui::SameLine();
					if (ImGui::Button(("TP##Button" + stringIndex).c_str()))
					{
						TeleportTo(position, false);
					}

					ImGui::SameLine();
					if (ImGui::Button(("Lerp##Button" + stringIndex).c_str()))
					{
						TeleportTo(position, true);
					}
					ImGui::SameLine();

					if (ImGui::Button(("Select##Button" + stringIndex).c_str()))
					{
						auto isChecked = checkedIndices.find(index) != checkedIndices.end();
						if (isChecked) {
							selectedIndex = index;
							selectedByClick = true;
							UpdateIndexName();
						}
					}
					ImGui::TableNextColumn();

					ImGui::PushStyleColor(ImGuiCol_Text, selected ? IM_COL32(40, 90, 175, 255) : ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_Text]));
					ImGui::Text("%s", name.c_str());
					ImGui::PopStyleColor();
					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltip();
						ImGui::Text("%s", description.c_str());
						ImGui::Text("Distance: %.2f", PositionDistance(position, app::ActorUtils_GetAvatarPos(nullptr)));
						ImGui::EndTooltip();
					}
					ImGui::TableNextColumn();
					ImGui::Text("%f, %f, %f", position.x, position.y, position.z);
				}
				index++;
			}
			ImGui::EndTable();
			ImGui::TreePop();
		}

		if (selectedIndex != -1)
			ImGui::Text("Selected: [%d] %s", selectedIndex, Teleports[selectedIndex].name.c_str());
	}

	bool CustomTeleports::NeedStatusDraw() const
	{
		return f_Enabled;
	}

	void CustomTeleports::DrawStatus()
	{
		ImGui::Text("Custom Teleport\n[%s|%s]", f_Auto ? "Auto" : "Manual", selectedIndexName);
	}

	CustomTeleports &CustomTeleports::GetInstance()
	{
		static CustomTeleports instance;
		return instance;
	}
}
