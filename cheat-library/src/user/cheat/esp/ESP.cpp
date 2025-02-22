#include "pch-il2cpp.h"
#include "ESP.h"

#include <helpers.h>
#include <algorithm>

#include <cheat/events.h>
#include <cheat/game/EntityManager.h>
#include "ESPRender.h"
#include <cheat/game/filters.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include "cheat-base/cheat/CheatManagerBase.h"

namespace cheat::feature
{

	ESP::ESP() : Feature(),
		NF(f_Enabled, "ESP", "ESP", false),

		NF(f_DrawBoxMode, u8"绘制模式", "ESP", DrawMode::Box),
		NF(f_DrawTracerMode, u8"跟踪模式", "ESP", DrawTracerMode::Line),
		NF(f_Fill, u8"填充 方框/矩形/箭头", "ESP", false),
		NF(f_FillTransparency, u8"填充透明度", "ESP", 0.5f),

		NF(f_ArrowRadius, u8"箭头半径", "ESP", 100.0f),
		NF(f_OutlineThickness, u8"追踪线宽度", "ESP", 1.0f),
		NF(f_TracerSize, u8"追踪大小", "ESP", 1.0f),
		NF(f_MiddleScreenTracer, u8"从屏幕中间开始追踪", "ESP", false),
		NF(f_DrawDistance, u8"绘制距离", "ESP", false),
		NF(f_DrawName, u8"绘制名称", "ESP", false),

		NF(f_FontSize, u8"字体大小", "ESP", 12.0f),
		NF(f_FontOutline, u8"字体描边", "ESP", true),
		NF(f_FontOutlineSize, u8"字体描边大小", "ESP", 1.0f),

		NF(f_GlobalFontColor, u8"字体颜色", "ESP", ImColor(255, 255, 255)),
		NF(f_GlobalBoxColor, u8"方框颜色", "ESP", ImColor(255, 255, 255)),
		NF(f_GlobalLineColor, u8"追踪颜色", "ESP", ImColor(255, 255, 255)),
		NF(f_GlobalRectColor, u8"矩形颜色", "ESP", ImColor(255, 255, 255)),

		NF(f_MinSize, u8"最小实体大小", "ESP", 0.5f),
		NF(f_Range, u8"范围", "ESP", 100.0f),
		m_Search({})
	{
		InstallFilters();

		m_FontContrastColor = ImGui::CalcContrastColor(f_GlobalFontColor);

		::events::KeyUpEvent += MY_METHOD_HANDLER(ESP::OnKeyUp);
	}


	const FeatureGUIInfo& ESP::GetGUIInfo() const
	{
		static const FeatureGUIInfo info{ "", "ESP", false };
		return info;
	}

	void ESP::DrawMain()
	{
		if (ImGui::BeginGroupPanel(u8"总开关", true))
		{
			ConfigWidget(u8"开关", f_Enabled, u8"无视障碍显示物品位置.");
			ConfigWidget(u8"范围 (m)", f_Range, 1.0f, 1.0f, 200.0f);

			ConfigWidget(u8"绘制模式", f_DrawBoxMode, u8"选择绘制模式.");
			ConfigWidget(u8"追踪模式", f_DrawTracerMode, u8"选择跟踪模式.");

			ConfigWidget(f_Fill);
			ConfigWidget(u8"方框填充透明度", f_FillTransparency, 0.01f, 0.0f, 1.0f, u8"填充绘制方框的透明度.");
			ConfigWidget(f_MiddleScreenTracer, u8"从屏幕的中间部分绘制跟踪线条.");

			if (f_DrawTracerMode.value() == DrawTracerMode::OffscreenArrows)
			{
				if (ImGui::BeginGroupPanel(u8"箭头追踪选项", true))
				{
					ConfigWidget(f_TracerSize, 0.005f, 0.1f, 10.0f, u8"追踪大小.");
					ConfigWidget(f_ArrowRadius, 0.5f, 50.0f, 300.0f, u8"箭头范围.");
					ConfigWidget(f_OutlineThickness, 0.005f, 0.0f, 10.0f, u8"箭头描边程度.");
				}
				ImGui::EndGroupPanel();
			}

			ImGui::Spacing();
			ConfigWidget(f_DrawName, u8"绘制物品名称.");
			ConfigWidget(f_DrawDistance, u8"绘制到物品的距离.");

			ImGui::Spacing();
			ConfigWidget(f_FontSize, 1, 1, 100, u8"物品名称/距离的字体大小.");
			ConfigWidget(u8"## 字体描边已开启", f_FontOutline); ImGui::SameLine();
			ConfigWidget(u8"字体描边", f_FontOutlineSize, 0.001f, 0.0f, 10.0f);

			ImGui::Spacing();
			if (ImGui::BeginGroupPanel(u8"全局颜色", true))
			{
				if (ConfigWidget(f_GlobalFontColor, u8"线条，名称，距离的字体颜色."))
					m_FontContrastColor = ImGui::CalcContrastColor(f_GlobalFontColor);

				ConfigWidget(f_GlobalBoxColor, u8"矩形颜色.");
				ConfigWidget(f_GlobalLineColor, u8"线条颜色.");
				ConfigWidget(f_GlobalRectColor, u8"追踪颜色.");
			}
			ImGui::EndGroupPanel();

			ConfigWidget(f_MinSize, 0.05f, 0.1f, 200.0f, u8"世界中透视的最小实体体积大小.\n" \
				u8"有些实体的很小或根本没有体积.\n" \
				u8"此参数有助于筛选出不符合此条件的实体.");
		}
		ImGui::EndGroupPanel();

		ImGui::Text(u8"如何使用项目筛选器：\n\tLMB-切换可见性\n\tRMB-开放式颜色选择器");
		ImGui::InputText(u8"搜索筛选", &m_Search);

		for (auto& [section, filters] : m_Sections)
		{
			ImGui::PushID(section.c_str());
			DrawSection(section, filters);
			ImGui::PopID();
		}
	}

	bool ESP::NeedStatusDraw() const
	{
		return f_Enabled;
	}

	void ESP::DrawStatus()
	{
		ImGui::Text(u8"绘制透视 [%.01fm|%s|%s%s%s%s]",
			f_Range.value(),
			f_DrawBoxMode.value() == DrawMode::Box ? "Box" : f_DrawBoxMode.value() == DrawMode::Rectangle ? "Rect" : "None",
			f_Fill ? "F" : "",
			f_DrawTracerMode.value() == DrawTracerMode::Line ? "L" : f_DrawTracerMode.value() == DrawTracerMode::OffscreenArrows ? "A" : "",
			f_DrawName ? "N" : "",
			f_DrawDistance ? "D" : ""
		);
	}

	ESP& ESP::GetInstance()
	{
		static ESP instance;
		return instance;
	}

	void ESP::GetNpcName(std::string& name)
	{
		if (name.find(u8"角色") != std::string::npos)
		{
			//cause name is like "Avatar_Catalyst_Boy_Heizo(Clone)" - We'll get name between 3rd underscore and 1st bracket
			int  j = 0;		// j is the number of spaces before the name starts
			int pos1 = 0;
			int pos2 = 0;
			for (int i = 0; i < name.length(); i++)
			{
				if (name[i] == '_')
				{
					j++;
					if (j == 3)
					{
						pos1 = i;
					}

				}
				if (name[i] == '(')
				{
					pos2 = i;
					break;
				}
			}
			name = name.substr(pos1 + 1, pos2 - pos1 - 1);
		}
		else if (name.find(u8"动物") != std::string::npos)
		{
			int count = 0;
			int  j = 0;
			int pos1 = 0;
			int pos2 = 0;
			for (int i = 0; i < name.length(); i++)
			{
				if (name[i] == '_')
				{
					count++;
				}
			}
			//switch statement to determine how we will get name
			switch (count)
			{
			case 3:
			{
				j = 0;		// j is the number of spaces before the name starts
				pos1 = 0;
				pos2 = 0;
				for (int i = 0; i < name.length(); i++)
				{
					if (name[i] == '_')
					{
						j++;
						if (j == 3)
						{
							pos1 = i;
						}

					}
					if (name[i] == '(')
					{
						pos2 = i;
						break;
					}
				}
				name = name.substr(pos1, pos2 - pos1);
			}
			case 4:
			{
				j = 0;		// j is the number of spaces before the name starts
				pos1 = 0;
				pos2 = 0;
				for (int i = 0; i < name.length(); i++)
				{
					if (name[i] == '_')
					{
						j++;
						if (j == 3)
						{
							pos1 = i;
						}
						if (j == 4)
						{
							pos2 = i;
							break;
						}
					}
				}
				name = name.substr(pos1 + 1, pos2 - pos1 - 1);
			}
			default:
				break;
			}
			return;
		}
		else if (name.find(u8"怪物") != std::string::npos)
		{
			int  j = 0;     //number of underscores in the name
			int pos1 = 0;	//position of the first underscore in the name
			int pos2 = 0;   //position of the second underscore in the name
			for (int i = 0; i < name.length(); i++)
			{
				if (name[i] == '_')
				{
					j++;
					if (j == 3)
					{
						pos1 = i;
					}
					if (j == 4)
					{
						pos2 = i;
						break;
					}
				}
			}
			name = name.substr(pos1 + 1, pos2 - pos1 - 1);
		}
		else
		{
			int  j = 0;     //number of underscores in the name
			int pos1 = 0;	//position of the first underscore in the name
			int pos2 = 0;   //position of the second underscore in the name
			for (int i = 0; i < name.length(); i++)
			{
				if (name[i] == '_')
				{
					j++;
					if (j == 4)
					{
						pos1 = i;
					}
					if (j == 5)
					{
						pos2 = i;
						break;
					}
				}
			}
			name = name.substr(pos1 + 1, pos2 - pos1 - 1);
		}
		return;
	}

	void ESP::AddFilter(const std::string& section, const std::string& name, game::IEntityFilter* filter)
	{
		if (m_Sections.count(section) == 0)
			m_Sections[section] = {};

		auto& filters = m_Sections[section];
		esp::ESPItem newItem(name, ImColor(120, 120, 120, 255), {}, name);
		filters.push_back({ config::CreateField<esp::ESPItem>(name, name, fmt::format("ESP::Filters::{}", section), false, newItem), filter });
	}

	void ESP::DrawSection(const std::string& section, const Filters& filters)
	{
		std::vector<const FilterInfo*> validFilters;

		for (auto& info : filters)
		{
			const auto& filterName = info.first.value().m_Name;

			auto it = std::search(
				filterName.begin(), filterName.end(),
				m_Search.begin(), m_Search.end(),
				[](char ch1, char ch2) { return std::tolower(ch1) == std::tolower(ch2); }
			);

			if (it != filterName.end())
				validFilters.push_back(&info);
		}

		if (validFilters.empty())
			return;

		bool checked = std::all_of(validFilters.begin(), validFilters.end(), [](const FilterInfo* filter) {  return filter->first.value().m_Enabled; });
		bool changed = false;

		if (ImGui::BeginSelectableGroupPanel(section.c_str(), checked, changed, true))
		{
			for (auto& info : validFilters)
			{
				ImGui::PushID(info);
				DrawFilterField(info->first);
				ImGui::PopID();
			}

			ImGui::Spacing();

			if (ImGui::TreeNode(this, u8"快捷键"))
			{
				for (auto& info : validFilters)
				{
					auto& field = info->first;
					ImGui::PushID(info);

					auto& hotkey = field.value().m_Hotkey;
					if (InputHotkey(field.name().c_str(), &hotkey, true))
						field.FireChanged();

					ImGui::PopID();
				}

				ImGui::TreePop();
			}
		}
		ImGui::EndSelectableGroupPanel();

		if (changed)
		{
			for (auto& info : validFilters)
			{
				info->first.value().m_Enabled = checked;
				info->first.FireChanged();
			}
		}
	}

	std::string Unsplit(const std::string& value)
	{
		std::stringstream in(value);
		std::stringstream out;
		std::string word;
		while (in >> word)
			out << word;
		return out.str();
	}

	void FilterItemSelector(const char* label, ImTextureID image, const config::Field<esp::ESPItem>& field, const ImVec2& size = ImVec2(200, 0), float icon_size = 30);

	void ESP::DrawFilterField(const config::Field<esp::ESPItem>& field)
	{
		auto imageInfo = ImageLoader::GetImage(Unsplit(field.value().m_Name));
		FilterItemSelector(field.value().m_Name.c_str(), imageInfo ? imageInfo->textureID : nullptr, field);
	}

	void ESP::DrawExternal()
	{
		auto& esp = ESP::GetInstance();
		if (!esp.f_Enabled)
			return;

		esp::render::PrepareFrame();

		auto& entityManager = game::EntityManager::instance();

		for (auto& entity : entityManager.entities())
		{
			if (entityManager.avatar()->distance(entity) > esp.f_Range)
				continue;

			for (auto& [section, filters] : m_Sections)
			{
				for (auto& [field, filter] : filters)
				{
					auto& entry = field.value();
					if (!entry.m_Enabled || !m_FilterExecutor.ApplyFilter(entity, filter))
						continue;

					if (entry.m_Name == "Npc" || "AvatarOwn" || "AvatarTeammate")
					{
						if (entity->type() == app::EntityType__Enum_1::Avatar || entity->type() == app::EntityType__Enum_1::NPC)
						{
							std::string name = entity->name();
							GetNpcName(name);
							esp::render::DrawEntity(name, entity, entry.m_Color, entry.m_ContrastColor);
							break;
						}
					}
					esp::render::DrawEntity(entry.m_Name, entity, entry.m_Color, entry.m_ContrastColor);
					break;
				}
			}
		}
	}

	void ESP::OnKeyUp(short key, bool& cancelled)
	{
		for (auto& [section, filters] : m_Sections)
		{
			for (auto& [field, filter] : filters)
			{
				auto& entry = field.value();
				if (entry.m_Hotkey.IsPressed(key))
				{
					entry.m_Enabled = !entry.m_Enabled;
					field.FireChanged();
				}
			}
		}
	}

	void FilterItemSelector(const char* label, ImTextureID image, const config::Field<esp::ESPItem>& field, const ImVec2& size, float icon_size)
	{

		// Init ImGui
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (window->SkipItems)
			return;

		ImGuiContext& g = *GImGui;
		ImGuiIO& io = g.IO;
		const ImGuiStyle& style = g.Style;
		const ImGuiID id = window->GetID(label);

		const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
		const ImVec2 item_size = ImGui::CalcItemSize(size, ImGui::CalcItemWidth(), label_size.y + style.FramePadding.y * 2.0f + 20.0f);

		float region_max_x = ImGui::GetContentRegionMaxAbs().x;
		if (region_max_x - window->DC.CursorPos.x < item_size.x)
			ImGui::Spacing();

		const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + item_size);
		const ImRect total_bb(window->DC.CursorPos, { frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Max.y });

		ImGui::ItemSize(total_bb, style.FramePadding.y);
		if (!ImGui::ItemAdd(total_bb, id))
		{
			ImGui::SameLine();
			return;
		}
		const bool hovered = ImGui::ItemHoverable(frame_bb, id);
		if (hovered)
		{
			ImGui::SetHoveredID(id);
			g.MouseCursor = ImGuiMouseCursor_Hand;
		}

		const bool lmb_click = hovered && io.MouseClicked[0];
		if (lmb_click)
		{
			field.value().m_Enabled = !field.value().m_Enabled;
			field.FireChanged();
			ImGui::FocusWindow(window);
			memset(io.MouseDown, 0, sizeof(io.MouseDown));
		}

		const bool rmb_click = hovered && io.MouseClicked[ImGuiMouseButton_Right];

		ImGuiWindow* picker_active_window = NULL;

		static bool color_changed = false;
		static ImGuiID opened_id = 0;
		if (rmb_click)
		{
			// Store current color and open a picker
			g.ColorPickerRef = ImVec4(field.value().m_Color);
			ImGui::OpenPopup("picker");
			ImGui::SetNextWindowPos(g.LastItemData.Rect.GetBL() + ImVec2(0.0f, style.ItemSpacing.y));
			opened_id = id;
			memset(io.MouseDown, 0, sizeof(io.MouseDown));
		}

		if (ImGui::BeginPopup("picker"))
		{
			picker_active_window = g.CurrentWindow;
			ImGuiColorEditFlags picker_flags_to_forward = ImGuiColorEditFlags_DataTypeMask_ | ImGuiColorEditFlags_PickerMask_ | ImGuiColorEditFlags_InputMask_ | ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_AlphaBar;
			ImGuiColorEditFlags picker_flags = ImGuiColorEditFlags_DisplayMask_ | ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaPreviewHalf;
			ImGui::SetNextItemWidth(ImGui::GetFrameHeight() * 12.0f); // Use 256 + bar sizes?
			color_changed |= ImGui::ColorPicker4("##picker", reinterpret_cast<float*>(&field.value().m_Color), picker_flags, &g.ColorPickerRef.x);
			ImGui::EndPopup();
		}

		bool popup_closed = id == opened_id && picker_active_window == NULL;
		if (popup_closed)
		{
			opened_id = 0;
			if (color_changed)
			{
				field.value().m_ContrastColor = ImGui::CalcContrastColor(field.value().m_Color);
				field.FireChanged();
				color_changed = false;
			}
		}

		const ImU32 border_color = ImGui::GetColorU32(g.ActiveId == id ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg);
		const ImRect clip_rect(frame_bb.Min.x, frame_bb.Min.y, frame_bb.Min.x + item_size.x, frame_bb.Min.y + item_size.y); // Not using frame_bb.Max because we have adjusted size

		float border_size = 2.0f;
		float border_rounding = 10.0f;

		ImVec2 render_pos = frame_bb.Min + style.FramePadding;
		ImGui::RenderFrame(clip_rect.Min, clip_rect.Max, field.value().m_Color, false, border_rounding);

		if (field.value().m_Enabled)
		{
			float check_mark_size = 13.0f;
			ImVec2 checkStart = { clip_rect.Max.x - check_mark_size - border_size, clip_rect.Min.y };
			ImGui::RenderFrame(checkStart - ImVec2(2.0f, 0), checkStart + ImVec2(check_mark_size + border_size, check_mark_size + border_size + 2.0f),
				ImGui::GetColorU32(ImGuiCol_FrameBg), false, border_rounding);
			ImGui::RenderCheckMark(window->DrawList, checkStart + ImVec2(0, 1.0f), 0xFFFFFFFF, check_mark_size - 1.0f);
		}

		window->DrawList->AddRect(clip_rect.Min, clip_rect.Max, border_color, border_rounding - 1.0f, 0, border_size);

		float y_center = frame_bb.Min.y + (frame_bb.Max.y - frame_bb.Min.y) / 2;

		ImVec2 image_start(frame_bb.Min.x + style.FramePadding.x + 5.0f, y_center - icon_size / 2);
		ImVec2 image_end(image_start.x + icon_size, image_start.y + icon_size);
		if (image != nullptr)
			window->DrawList->AddImageRounded(image, image_start, image_start + ImVec2(icon_size, icon_size), { 0.0f, 0.0f }, { 1.0f, 1.0f },
				ImColor(1.0f, 1.0f, 1.0f), 0.3f);

		bool pushed = ImGui::PushStyleColorWithContrast(field.value().m_Color, ImGuiCol_Text, ImColor(0, 0, 0), 2.0f);

		ImVec2 text_end(frame_bb.Max.x - style.FramePadding.x - border_size, y_center + label_size.y / 2);
		ImVec2 text_start(ImMax(image_end.x + style.FramePadding.x, text_end.x - label_size.x), y_center - label_size.y / 2);
		ImGui::RenderTextClipped(text_start, text_end, label, NULL, NULL, { 0, 0 }, &clip_rect);

		if (pushed)
			ImGui::PopStyleColor();

		ImGui::SameLine();

		return;
	}

#define ADD_FILTER_FIELD(section, name) AddFilter(util::MakeCapital(#section), util::SplitWords(#name), &game::filters::##section##::##name##)
	void ESP::InstallFilters()
	{
		ADD_FILTER_FIELD(collection, Book);
		ADD_FILTER_FIELD(collection, Viewpoint);
		ADD_FILTER_FIELD(collection, RadiantSpincrystal);
		ADD_FILTER_FIELD(collection, BookPage);
		ADD_FILTER_FIELD(collection, QuestInteract);
		ADD_FILTER_FIELD(collection, WoodenCrate);
		ADD_FILTER_FIELD(collection, GeoSigil);

		// Regular Chests
		ADD_FILTER_FIELD(chest, CommonChest);
		ADD_FILTER_FIELD(chest, ExquisiteChest);
		ADD_FILTER_FIELD(chest, PreciousChest);
		ADD_FILTER_FIELD(chest, LuxuriousChest);
		ADD_FILTER_FIELD(chest, RemarkableChest);
		// Other Chests
		//ADD_FILTER_FIELD(chest, BuriedChest); // Shared name, commented for now
		ADD_FILTER_FIELD(chest, SearchPoint);
		

		ADD_FILTER_FIELD(featured, Anemoculus);
		ADD_FILTER_FIELD(featured, CrimsonAgate);
		ADD_FILTER_FIELD(featured, Dendroculus);
		ADD_FILTER_FIELD(featured, Electroculus);
		ADD_FILTER_FIELD(featured, Electrogranum);
		ADD_FILTER_FIELD(featured, FishingPoint);
		ADD_FILTER_FIELD(featured, Geoculus);
		ADD_FILTER_FIELD(featured, ItemDrops);
		ADD_FILTER_FIELD(featured, KeySigil);
		ADD_FILTER_FIELD(featured, Lumenspar);
		ADD_FILTER_FIELD(featured, ShrineOfDepth);
		ADD_FILTER_FIELD(featured, TimeTrialChallenge);

		ADD_FILTER_FIELD(guide, BouncyMushroom);
		ADD_FILTER_FIELD(guide, CampfireTorch);
		ADD_FILTER_FIELD(guide, ClusterleafOfCultivation);
		ADD_FILTER_FIELD(guide, DayNightSwitchingMechanism);
		ADD_FILTER_FIELD(guide, DendroGranum);
		ADD_FILTER_FIELD(guide, DendroPile);
		ADD_FILTER_FIELD(guide, DendroRock);
		ADD_FILTER_FIELD(guide, EnkanomiyaPhaseGate);
		ADD_FILTER_FIELD(guide, MysteriousCarvings);
		ADD_FILTER_FIELD(guide, PhaseGate);
		ADD_FILTER_FIELD(guide, PlacesofEssenceWorship);
		ADD_FILTER_FIELD(guide, Pot);
		ADD_FILTER_FIELD(guide, RuinBrazier);
		ADD_FILTER_FIELD(guide, Stormstone);
		ADD_FILTER_FIELD(guide, TriangularMechanism);

		ADD_FILTER_FIELD(living, AvatarTeammate);
		ADD_FILTER_FIELD(living, AvatarOwn);
		ADD_FILTER_FIELD(living, BakeDanuki);
		ADD_FILTER_FIELD(living, BirdEgg);
		ADD_FILTER_FIELD(living, Boar);
		ADD_FILTER_FIELD(living, ButterflyWings);
		ADD_FILTER_FIELD(living, Cat);
		ADD_FILTER_FIELD(living, Crab);
		ADD_FILTER_FIELD(living, Crane);
		ADD_FILTER_FIELD(living, Crow);
		ADD_FILTER_FIELD(living, CrystalCore);
		ADD_FILTER_FIELD(living, Dog);
		ADD_FILTER_FIELD(living, DuskBird);
		ADD_FILTER_FIELD(living, Eel);
		ADD_FILTER_FIELD(living, Falcon);
		ADD_FILTER_FIELD(living, Finch);
		ADD_FILTER_FIELD(living, Fish);
		ADD_FILTER_FIELD(living, Fox);
		ADD_FILTER_FIELD(living, Frog);
		ADD_FILTER_FIELD(living, Inu);
		ADD_FILTER_FIELD(living, Kitsune);
		ADD_FILTER_FIELD(living, LizardTail);
		ADD_FILTER_FIELD(living, LucklightFly);
		ADD_FILTER_FIELD(living, LuminescentSpine);
		ADD_FILTER_FIELD(living, Npc);
		ADD_FILTER_FIELD(living, Onikabuto);
		ADD_FILTER_FIELD(living, Pigeon);
		ADD_FILTER_FIELD(living, Salamander);
		ADD_FILTER_FIELD(living, Squirrel);
		ADD_FILTER_FIELD(living, Starconch);
		ADD_FILTER_FIELD(living, Weasel);
		ADD_FILTER_FIELD(living, Wigeon);

		ADD_FILTER_FIELD(mineral, AmethystLump);
		ADD_FILTER_FIELD(mineral, ArchaicStone);
		ADD_FILTER_FIELD(mineral, CorLapis);
		ADD_FILTER_FIELD(mineral, CrystalChunk);
		ADD_FILTER_FIELD(mineral, CrystalMarrow);
		ADD_FILTER_FIELD(mineral, DunlinsTooth);
		ADD_FILTER_FIELD(mineral, ElectroCrystal);
		ADD_FILTER_FIELD(mineral, IronChunk);
		ADD_FILTER_FIELD(mineral, NoctilucousJade);
		ADD_FILTER_FIELD(mineral, MagicalCrystalChunk);
		ADD_FILTER_FIELD(mineral, ScarletQuartz);
		ADD_FILTER_FIELD(mineral, Starsilver);
		ADD_FILTER_FIELD(mineral, WhiteIronChunk);

		// Trounce. Arranged by appearance in-game.
		ADD_FILTER_FIELD(monster, Dvalin);
		ADD_FILTER_FIELD(monster, Andrius);
		ADD_FILTER_FIELD(monster, Tartaglia);
		ADD_FILTER_FIELD(monster, Azhdaha);
		ADD_FILTER_FIELD(monster, Signora);
		ADD_FILTER_FIELD(monster, Shogun);
		// Bosses. Arranged by "type" then alphabetical.
		// Regisvines.
		ADD_FILTER_FIELD(monster, CryoRegisvine);
		ADD_FILTER_FIELD(monster, ElectroRegisvine);
		ADD_FILTER_FIELD(monster, PyroRegisvine);
		// Hypostases.
		ADD_FILTER_FIELD(monster, AnemoHypostasis);
		ADD_FILTER_FIELD(monster, CryoHypostasis);
		ADD_FILTER_FIELD(monster, ElectroHypostasis);
		ADD_FILTER_FIELD(monster, GeoHypostasis);
		ADD_FILTER_FIELD(monster, HydroHypostasis);
		ADD_FILTER_FIELD(monster, PyroHypostasis);
		// Vishaps.
		ADD_FILTER_FIELD(monster, CryoBathysmalVishap);
		ADD_FILTER_FIELD(monster, ElectroBathysmalVishap);
		ADD_FILTER_FIELD(monster, PrimoGeovishap);
		// Oceanids.
		ADD_FILTER_FIELD(monster, Oceanid);
		ADD_FILTER_FIELD(monster, ThunderManifestation);
		// Khaenri'ah.
		ADD_FILTER_FIELD(monster, PerpetualMechanicalArray);
		ADD_FILTER_FIELD(monster, RuinSerpent);
		// Others.
		ADD_FILTER_FIELD(monster, GoldenWolflord);
		ADD_FILTER_FIELD(monster, MaguuKenki);
		// Sumeru
		ADD_FILTER_FIELD(monster, JadeplumeTerrorshroom);
		// Regular. Alphabetical.
		ADD_FILTER_FIELD(monster, AbyssMage);
		ADD_FILTER_FIELD(monster, BlackSerpentKnight);
		ADD_FILTER_FIELD(monster, Cicin);
		ADD_FILTER_FIELD(monster, ElectroAbyssLector);
		ADD_FILTER_FIELD(monster, Eremite);
		ADD_FILTER_FIELD(monster, EyeOfTheStorm);
		ADD_FILTER_FIELD(monster, FatuiAgent);
		ADD_FILTER_FIELD(monster, FatuiCicinMage);
		ADD_FILTER_FIELD(monster, FatuiMirrorMaiden);
		ADD_FILTER_FIELD(monster, FatuiSkirmisher);
		ADD_FILTER_FIELD(monster, FloatingFungus);
		ADD_FILTER_FIELD(monster, Geovishap);
		ADD_FILTER_FIELD(monster, GeovishapHatchling);
		ADD_FILTER_FIELD(monster, GroundedShroom);
		ADD_FILTER_FIELD(monster, Hilichurl);
		ADD_FILTER_FIELD(monster, HydroAbyssHerald);
		ADD_FILTER_FIELD(monster, HydroBathysmalVishap);
		ADD_FILTER_FIELD(monster, Kairagi);
		ADD_FILTER_FIELD(monster, Millelith);
		ADD_FILTER_FIELD(monster, Mitachurl);
		ADD_FILTER_FIELD(monster, Nobushi);
		ADD_FILTER_FIELD(monster, OceanidBoar);
		ADD_FILTER_FIELD(monster, OceanidCrab);
		ADD_FILTER_FIELD(monster, OceanidCrane);
		ADD_FILTER_FIELD(monster, OceanidFalcon);
		ADD_FILTER_FIELD(monster, OceanidFinch);
		ADD_FILTER_FIELD(monster, OceanidFrog);
		ADD_FILTER_FIELD(monster, OceanidSquirrel);
		ADD_FILTER_FIELD(monster, OceanidWigeon);
		ADD_FILTER_FIELD(monster, PyroAbyssLector);
		ADD_FILTER_FIELD(monster, Rifthound);
		ADD_FILTER_FIELD(monster, RifthoundWhelp);
		ADD_FILTER_FIELD(monster, RishbolandTiger);
		ADD_FILTER_FIELD(monster, RuinDrake);
		ADD_FILTER_FIELD(monster, RuinGrader);
		ADD_FILTER_FIELD(monster, RuinGuard);
		ADD_FILTER_FIELD(monster, RuinHunter);
		ADD_FILTER_FIELD(monster, RuinSentinel);
		ADD_FILTER_FIELD(monster, Samachurl);
		ADD_FILTER_FIELD(monster, SangonomiyaCohort);
		ADD_FILTER_FIELD(monster, ShadowyHusk);
		ADD_FILTER_FIELD(monster, ShaggySumpterBeast);
		ADD_FILTER_FIELD(monster, ShogunateInfantry);
		ADD_FILTER_FIELD(monster, Slime);
		ADD_FILTER_FIELD(monster, Specter);
		ADD_FILTER_FIELD(monster, Spincrocodile);
		ADD_FILTER_FIELD(monster, StretchyFungus);
		ADD_FILTER_FIELD(monster, TreasureHoarder);
		ADD_FILTER_FIELD(monster, UnusualHilichurl);
		ADD_FILTER_FIELD(monster, WhirlingFungus);
		ADD_FILTER_FIELD(monster, Whopperflower);
		ADD_FILTER_FIELD(monster, WingedShroom);

		ADD_FILTER_FIELD(plant, AmakumoFruit);
		ADD_FILTER_FIELD(plant, Apple);
		ADD_FILTER_FIELD(plant, BambooShoot);
		ADD_FILTER_FIELD(plant, Berry);
		ADD_FILTER_FIELD(plant, CallaLily);
		ADD_FILTER_FIELD(plant, Carrot);
		ADD_FILTER_FIELD(plant, Cecilia);
		ADD_FILTER_FIELD(plant, DandelionSeed);
		ADD_FILTER_FIELD(plant, Dendrobium);
		ADD_FILTER_FIELD(plant, FlamingFlowerStamen);
		ADD_FILTER_FIELD(plant, FluorescentFungus);
		ADD_FILTER_FIELD(plant, GlazeLily);
		ADD_FILTER_FIELD(plant, HarraFruit);
		ADD_FILTER_FIELD(plant, Horsetail);
		ADD_FILTER_FIELD(plant, JueyunChili);
		ADD_FILTER_FIELD(plant, KalpalataLotus);
		ADD_FILTER_FIELD(plant, LavenderMelon);
		ADD_FILTER_FIELD(plant, LotusHead);
		ADD_FILTER_FIELD(plant, Matsutake);
		ADD_FILTER_FIELD(plant, Mint);
		ADD_FILTER_FIELD(plant, MistFlowerCorolla);
		ADD_FILTER_FIELD(plant, Mushroom);
		ADD_FILTER_FIELD(plant, NakuWeed);
		ADD_FILTER_FIELD(plant, NilotpalaLotus);
		ADD_FILTER_FIELD(plant, Padisarah);
		ADD_FILTER_FIELD(plant, PhilanemoMushroom);
		ADD_FILTER_FIELD(plant, Pinecone);
		ADD_FILTER_FIELD(plant, Qingxin);
		ADD_FILTER_FIELD(plant, Radish);
		ADD_FILTER_FIELD(plant, RukkhashavaMushroom);
		ADD_FILTER_FIELD(plant, SakuraBloom);
		ADD_FILTER_FIELD(plant, SangoPearl);
		ADD_FILTER_FIELD(plant, SeaGanoderma);
		ADD_FILTER_FIELD(plant, Seagrass);
		ADD_FILTER_FIELD(plant, SilkFlower);
		ADD_FILTER_FIELD(plant, SmallLampGrass);
		ADD_FILTER_FIELD(plant, Snapdragon);
		ADD_FILTER_FIELD(plant, SumeruRose);
		ADD_FILTER_FIELD(plant, Sunsettia);
		ADD_FILTER_FIELD(plant, SweetFlower);
		ADD_FILTER_FIELD(plant, Valberry);
		ADD_FILTER_FIELD(plant, Violetgrass);
		ADD_FILTER_FIELD(plant, Viparyas);
		ADD_FILTER_FIELD(plant, WindwheelAster);
		ADD_FILTER_FIELD(plant, Wolfhook);
		ADD_FILTER_FIELD(plant, ZaytunPeach);

		ADD_FILTER_FIELD(puzzle, AncientRime);
		ADD_FILTER_FIELD(puzzle, BakeDanuki);
		ADD_FILTER_FIELD(puzzle, BloattyFloatty);
		ADD_FILTER_FIELD(puzzle, CubeDevices);
		ADD_FILTER_FIELD(puzzle, DendroPuzzles);
		ADD_FILTER_FIELD(puzzle, DreadfulWithering);
		ADD_FILTER_FIELD(puzzle, EightStoneTablets);
		ADD_FILTER_FIELD(puzzle, ElectricConduction);
		ADD_FILTER_FIELD(puzzle, ElectroSeelie);
		ADD_FILTER_FIELD(puzzle, ElementalMonument);
		ADD_FILTER_FIELD(puzzle, FloatingAnemoSlime);
		ADD_FILTER_FIELD(puzzle, Geogranum);
		ADD_FILTER_FIELD(puzzle, GeoPuzzle);
		ADD_FILTER_FIELD(puzzle, LargeRockPile);
		ADD_FILTER_FIELD(puzzle, LightningStrikeProbe);
		ADD_FILTER_FIELD(puzzle, LightUpTilePuzzle);
		ADD_FILTER_FIELD(puzzle, LumenCage);
		ADD_FILTER_FIELD(puzzle, LuminousSeelie);
		ADD_FILTER_FIELD(puzzle, MistBubble);
		ADD_FILTER_FIELD(puzzle, NurseriesInTheWilds);
		ADD_FILTER_FIELD(puzzle, OozingConcretions);
		ADD_FILTER_FIELD(puzzle, PhantasmalGate);
		ADD_FILTER_FIELD(puzzle, PirateHelm);
		ADD_FILTER_FIELD(puzzle, PressurePlate);
		ADD_FILTER_FIELD(puzzle, RelayStone);
		ADD_FILTER_FIELD(puzzle, SaghiraMachine);
		ADD_FILTER_FIELD(puzzle, SealLocations);
		ADD_FILTER_FIELD(puzzle, Seelie);
		ADD_FILTER_FIELD(puzzle, SeelieLamp);
		ADD_FILTER_FIELD(puzzle, SmallRockPile);
		ADD_FILTER_FIELD(puzzle, StonePillarSeal);
		ADD_FILTER_FIELD(puzzle, StormBarrier);
		ADD_FILTER_FIELD(puzzle, SwordHilt);
		ADD_FILTER_FIELD(puzzle, Temari);
		ADD_FILTER_FIELD(puzzle, TorchPuzzle);
		ADD_FILTER_FIELD(puzzle, TriYanaSeeds);
		ADD_FILTER_FIELD(puzzle, UniqueRocks);
		ADD_FILTER_FIELD(puzzle, WarmingSeelie);
		ADD_FILTER_FIELD(puzzle, WindmillMechanism);
	}
#undef ADD_FILTER_FIELD
}
