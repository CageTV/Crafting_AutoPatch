/*
 * Crafting - AutoPatch
 * Copyright (c) 2026 CageTV
 *
 * Released under the MIT License; see LICENSE.
 */
#include "PCH.h"
#include "Menu.h"
#include "Core.h"

#include "SKSEMenuFramework.h"

namespace Menu
{
	namespace
	{
		using namespace ImGuiMCP;

		constexpr ImU32 Col(int r, int g, int b, int a = 255) { return (static_cast<ImU32>(a) << 24) | (static_cast<ImU32>(b) << 16) | (static_cast<ImU32>(g) << 8) | static_cast<ImU32>(r); }

		constexpr ImU32 kChanged = Col(94, 140, 98);
		constexpr ImU32 kPlanned = Col(214, 156, 62);
		constexpr ImU32 kLeft = Col(110, 110, 110);

		const char* StatusName(Core::Status s) { return s == Core::Status::Changed ? "Tagged" : s == Core::Status::Planned ? "Plan only" : "Left alone"; }
		ImU32       StatusColor(Core::Status s) { return s == Core::Status::Changed ? kChanged : s == Core::Status::Planned ? kPlanned : kLeft; }

		void Pill(const char* text, ImU32 color)
		{
			const auto pos = GetCursorScreenPos();
			const auto size = CalcTextSize(text);
			auto*      dl = GetWindowDrawList();
			ImDrawListManager::AddRectFilled(dl, ImVec2(pos.x - 2.0f, pos.y - 1.0f), ImVec2(pos.x + size.x + 6.0f, pos.y + size.y + 1.0f), color, 4.0f, 0);
			SetCursorPosX(GetCursorPosX() + 2.0f);
			TextColored(ImVec4(1, 1, 1, 1), "%s", text);
		}

		std::string Lower(std::string s)
		{
			std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return s;
		}

		bool needsRestart = false;

		void Tip(const char* tip)
		{
			if (IsItemHovered()) {
				BeginTooltip();
				PushTextWrapPos(420.0f);
				TextUnformatted(tip);
				PopTextWrapPos();
				EndTooltip();
			}
		}

		int Count(bool a_weapon, Core::Status a_status)
		{
			int n = 0;
			for (const auto& r : Core::Rows()) {
				n += (r.weapon == a_weapon && r.status == a_status) ? 1 : 0;
			}
			return n;
		}

		void Bar(int a_changed, int a_planned, int a_left)
		{
			const int   total = a_changed + a_planned + a_left;
			const float width = std::min(GetContentRegionAvail().x - 8.0f, 640.0f);
			const auto  start = GetCursorScreenPos();
			auto*       dl = GetWindowDrawList();
			ImDrawListManager::AddRectFilled(dl, ImVec2(start.x - 1, start.y - 1), ImVec2(start.x + width + 1, start.y + 21), Col(30, 30, 30), 3.0f, 0);
			float       x = start.x;
			const int   n[] = { a_changed, a_planned, a_left };
			const ImU32 c[] = { kChanged, kPlanned, kLeft };
			for (int i = 0; i < 3; ++i) {
				if (n[i] == 0 || total == 0) {
					continue;
				}
				const float w = width * static_cast<float>(n[i]) / static_cast<float>(total);
				ImDrawListManager::AddRectFilled(dl, ImVec2(x, start.y), ImVec2(x + w, start.y + 20), c[i], 0.0f, 0);
				x += w;
			}
			Dummy(ImVec2(width, 24.0f));
		}

		void Legend(ImU32 a_color, const char* a_label, int a_n, const char* a_what)
		{
			Pill(a_label, a_color);
			SameLine();
			Text("%d  %s", a_n, a_what);
		}

		void Flag(const char* a_name, bool a_on)
		{
			TextColored(a_on ? ImVec4(0.55f, 0.85f, 0.6f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s %s", a_on ? "loaded:" : "not loaded:", a_name);
		}

		// ---- Overview -------------------------------------------------------------------------------------------------------------
		void __stdcall RenderOverview()
		{
			const auto& info = Core::Info();
			SeparatorText("Crafting AutoPatch");
			if (!info.ran) {
				TextColored(ImVec4(1.0f, 0.55f, 0.45f, 1.0f), "Weapons Armor Clothing and Clutter Fixes is not loaded: nothing is changed.");
				TextWrapped("See CraftingAutoPatch.log in your SKSE logs folder.");
				return;
			}
			TextWrapped("At game start the weapons and armor that other mods add are given the keywords Weapons Armor Clothing and Clutter Fixes uses, "
			            "so the smithing and tempering perks and Complete Crafting Overhaul treat them like the game's own gear and they need no patch plugin. "
			            "Nothing is written to your save and nothing is permanent: switch it off and everything is as the mods made it.");
			Spacing();
			Flag("Weapons Armor Clothing and Clutter Fixes", info.waccf);
			Flag("Armor and Clothing Extension", info.ace);
			Flag("Complete Crafting Overhaul Remastered", info.ccor);
			Flag("Unofficial Skyrim Special Edition Patch (its changes are part of what is learned)", info.ussep);
			Flag("Smithing Perks Overhaul SE", info.spo);
			Spacing();

			auto& cfg = Core::Cfg();
			SeparatorText("Switches (applied at the next game start)");
			if (Checkbox("Apply the changes", &cfg.apply)) {
				Core::SaveConfig();
				needsRestart = true;
			}
			Tip("Off: the pages below still show the plan, but nothing is changed in the game.");
			if (Checkbox("Weapons", &cfg.weapons)) {
				Core::SaveConfig();
				needsRestart = true;
			}
			SameLine();
			if (Checkbox("Armor", &cfg.armor)) {
				Core::SaveConfig();
				needsRestart = true;
			}
			static const char* kCloth[] = { "plan", "on", "off" };
			int                ci = cfg.clothing == "on" ? 1 : cfg.clothing == "off" ? 2 : 0;
			SetNextItemWidth(160.0f);
			if (Combo("Clothing tiers", &ci, kCloth, 3)) {
				cfg.clothing = kCloth[ci];
				Core::SaveConfig();
				needsRestart = true;
			}
			Tip("plan: clothing protection tiers (matched to the nearest similar clothing by price) are only listed.\non: they are applied.\noff: clothing is ignored.");
			if (Checkbox("Also use item names", &cfg.useNames)) {
				Core::SaveConfig();
				needsRestart = true;
			}
			Tip("A material found only from the words in an item's name is less certain than one found from its keywords or its crafting ingredients. "
			    "Off: such items are only listed as 'Plan only'.");
			if (needsRestart) {
				TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "Saved. Restart the game for it to take effect.");
			}
			Spacing();

			SeparatorText("Weapons");
			Bar(Count(true, Core::Status::Changed), Count(true, Core::Status::Planned), Count(true, Core::Status::Left));
			Legend(kChanged, "Tagged", Count(true, Core::Status::Changed), "given the material keywords");
			Legend(kPlanned, "Plan only", Count(true, Core::Status::Planned), "would be tagged, but apply or names are off");
			Legend(kLeft, "Left alone", Count(true, Core::Status::Left), "(see the Skipped page for why)");
			Spacing();
			SeparatorText("Armor and clothing");
			Bar(Count(false, Core::Status::Changed), Count(false, Core::Status::Planned), Count(false, Core::Status::Left));
			Legend(kChanged, "Tagged", Count(false, Core::Status::Changed), "given the material keywords");
			Legend(kPlanned, "Plan only", Count(false, Core::Status::Planned), "would be tagged, but apply, names or clothing are off");
			Legend(kLeft, "Left alone", Count(false, Core::Status::Left), "(see the Skipped page for why)");
			Spacing();
			Text("Learned from %d of the game's own items in %d material groups: %d crafting ingredients, %d name words, %d clothing references.", info.referenceItems,
				info.referenceGroups, info.learnedIngredients, info.learnedWords, info.clothingReferences);
			TextDisabled("The game's own items and the items of the mods named above are never touched. Full detail: Data/SKSE/Plugins/CraftingAutoPatch/report.json");
		}

		// ---- Mods ---------------------------------------------------------------------------------------------------------------------
		void __stdcall RenderMods()
		{
			const auto& info = Core::Info();
			if (!info.ran) {
				TextDisabled("Nothing to show.");
				return;
			}
			SeparatorText("Mods that add weapons or armor");
			TextWrapped("Tick a mod to leave all its items alone (applied at the next game start).");
			auto& cfg = Core::Cfg();
			if (BeginTable("##mods", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
			               ImVec2(0.0f, std::max(220.0f, GetContentRegionAvail().y - 20.0f)))) {
				TableSetupScrollFreeze(0, 1);
				TableSetupColumn("Mod", ImGuiTableColumnFlags_WidthStretch, 5.0f);
				TableSetupColumn("Items", ImGuiTableColumnFlags_WidthFixed, 60.0f);
				TableSetupColumn("Tagged", ImGuiTableColumnFlags_WidthFixed, 60.0f);
				TableSetupColumn("Plan", ImGuiTableColumnFlags_WidthFixed, 60.0f);
				TableSetupColumn("Left", ImGuiTableColumnFlags_WidthFixed, 60.0f);
				TableSetupColumn("Exclude", ImGuiTableColumnFlags_WidthFixed, 70.0f);
				TableHeadersRow();
				std::vector<std::pair<std::string, Core::PluginStat>> rows(info.plugins.begin(), info.plugins.end());
				std::sort(rows.begin(), rows.end(), [](const auto& a, const auto& b) { return a.second.items > b.second.items; });
				for (const auto& [name, s] : rows) {
					PushID(name.c_str());
					TableNextRow();
					TableSetColumnIndex(0);
					TextUnformatted(name.c_str());
					TableSetColumnIndex(1);
					Text("%d", s.items);
					TableSetColumnIndex(2);
					Text("%d", s.changed);
					TableSetColumnIndex(3);
					Text("%d", s.planned);
					TableSetColumnIndex(4);
					Text("%d", s.left);
					TableSetColumnIndex(5);
					bool ex = cfg.exclude.count(name) > 0;
					if (Checkbox("##ex", &ex)) {
						if (ex) {
							cfg.exclude.insert(name);
						} else {
							cfg.exclude.erase(name);
						}
						Core::SaveConfig();
						needsRestart = true;
					}
					PopID();
				}
				EndTable();
			}
		}

		// ---- the item pages -----------------------------------------------------------------------------------------------------------
		struct PageState
		{
			int              filter = 0;
			char             search[64] = "";
			const Core::Row* selected = nullptr;
		};
		PageState wpn, arm;

		void RenderItems(bool a_weapon)
		{
			auto& st = a_weapon ? wpn : arm;
			if (!Core::Info().ran) {
				TextDisabled("Nothing to show: Weapons Armor Clothing and Clutter Fixes is not loaded.");
				return;
			}
			static const char* kFilters[] = { "Everything", "Tagged", "Plan only", "Left alone" };
			SetNextItemWidth(180.0f);
			Combo(a_weapon ? "Show##w" : "Show##a", &st.filter, kFilters, 4);
			SameLine();
			SetNextItemWidth(220.0f);
			InputText(a_weapon ? "Search##w" : "Search##a", st.search, sizeof(st.search));
			const std::string needle = Lower(st.search);

			std::vector<const Core::Row*> shown;
			int                           total = 0;
			for (const auto& r : Core::Rows()) {
				if (r.weapon != a_weapon) {
					continue;
				}
				if (st.filter == 1 && r.status != Core::Status::Changed) {
					continue;
				}
				if (st.filter == 2 && r.status != Core::Status::Planned) {
					continue;
				}
				if (st.filter == 3 && r.status != Core::Status::Left) {
					continue;
				}
				if (!needle.empty() && Lower(r.name).find(needle) == std::string::npos && Lower(r.plugin).find(needle) == std::string::npos &&
					Lower(r.reason).find(needle) == std::string::npos && Lower(r.material).find(needle) == std::string::npos) {
					continue;
				}
				++total;
				if (shown.size() < 400) {
					shown.push_back(&r);
				}
			}
			Text("%d match%s%s", total, total == 1 ? "" : "es", total > 400 ? " (the first 400 are listed: narrow the search)" : "");

			const float tableHeight = std::max(180.0f, GetContentRegionAvail().y - 170.0f);
			if (BeginTable(a_weapon ? "##weapons" : "##armor", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp,
			               ImVec2(0.0f, tableHeight))) {
				TableSetupScrollFreeze(0, 1);
				TableSetupColumn(a_weapon ? "Weapon" : "Armor", ImGuiTableColumnFlags_WidthStretch, 3.0f);
				TableSetupColumn("Mod", ImGuiTableColumnFlags_WidthStretch, 2.2f);
				TableSetupColumn("Material", ImGuiTableColumnFlags_WidthFixed, 100.0f);
				TableSetupColumn("What", ImGuiTableColumnFlags_WidthFixed, 90.0f);
				TableSetupColumn("Why", ImGuiTableColumnFlags_WidthStretch, 3.0f);
				TableHeadersRow();
				for (const auto* r : shown) {
					PushID(r);
					TableNextRow();
					TableSetColumnIndex(0);
					if (Selectable(r->name.c_str(), st.selected == r, ImGuiSelectableFlags_SpanAllColumns)) {
						st.selected = r;
					}
					TableSetColumnIndex(1);
					TextUnformatted(r->plugin.c_str());
					TableSetColumnIndex(2);
					TextUnformatted(r->material.c_str());
					TableSetColumnIndex(3);
					Pill(StatusName(r->status), StatusColor(r->status));
					TableSetColumnIndex(4);
					TextUnformatted(r->reason.c_str());
					PopID();
				}
				EndTable();
			}
			Separator();
			if (st.selected) {
				Text("%s  (%s, %s)", st.selected->name.c_str(), st.selected->plugin.c_str(), st.selected->kind.c_str());
				TextWrapped("Found by: %s%s", st.selected->how.empty() ? "-" : st.selected->how.c_str(),
					st.selected->confidence > 0.0f ? (st.selected->confidence >= 0.85f ? "  (certain)" : "  (less certain)") : "");
				TextWrapped("Now:     %s", st.selected->now.c_str());
				if (!st.selected->planned.empty()) {
					TextWrapped("%s %s", st.selected->status == Core::Status::Changed ? "After:  " : "Plan:   ", st.selected->planned.c_str());
				}
			} else {
				TextDisabled("Click a line to see how the material was found and what changes.");
			}
		}

		void __stdcall RenderWeapons() { RenderItems(true); }
		void __stdcall RenderArmor() { RenderItems(false); }

		// ---- Skipped --------------------------------------------------------------------------------------------------------------------
		void __stdcall RenderSkipped()
		{
			const auto& info = Core::Info();
			if (!info.ran) {
				TextDisabled("Nothing to show.");
				return;
			}
			SeparatorText("Why items were left alone");
			if (BeginTable("##skipped", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp)) {
				TableSetupColumn("Reason", ImGuiTableColumnFlags_WidthStretch, 5.0f);
				TableSetupColumn("Items", ImGuiTableColumnFlags_WidthFixed, 70.0f);
				TableHeadersRow();
				for (const auto& [reason, n] : info.skipped) {
					TableNextRow();
					TableSetColumnIndex(0);
					TextUnformatted(reason.c_str());
					TableSetColumnIndex(1);
					Text("%d", n);
				}
				EndTable();
			}
		}
	}

	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled()) {
			SKSE::log::warn("SKSE Menu Framework not found: CraftingAutoPatch still works, it just has no pages (see report.json)");
			return;
		}
		SKSEMenuFramework::SetSection("Crafting AutoPatch");
		SKSEMenuFramework::AddSectionItem("Overview", RenderOverview);
		SKSEMenuFramework::AddSectionItem("Mods", RenderMods);
		SKSEMenuFramework::AddSectionItem("Weapons", RenderWeapons);
		SKSEMenuFramework::AddSectionItem("Armor", RenderArmor);
		SKSEMenuFramework::AddSectionItem("Skipped", RenderSkipped);
		SKSE::log::info("Registered with SKSE Menu Framework {}", SKSEMenuFramework::GetMenuFrameworkVersion());
	}
}
