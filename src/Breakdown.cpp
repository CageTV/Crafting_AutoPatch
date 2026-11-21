/*
 * Crafting - AutoPatch
 * Copyright (c) 2026 CageTV
 *
 * Released under the MIT License; see LICENSE.
 */
#include "PCH.h"
#include "Breakdown.h"

#include <nlohmann/json.hpp>

namespace Breakdown
{
	namespace
	{
		std::filesystem::path Dir() { return std::filesystem::path("Data/SKSE/Plugins/CraftingAutoPatch"); }

		std::string Lower(std::string a_s)
		{
			std::transform(a_s.begin(), a_s.end(), a_s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return a_s;
		}

		const std::set<std::string> kSkipPlugins = { "skyrim.esm", "update.esm", "dawnguard.esm", "hearthfires.esm", "dragonborn.esm",
			"unofficial skyrim special edition patch.esp", "weapons armor clothing & clutter fixes.esp", "waccf_armor and clothing extension.esp",
			"complete crafting overhaul_remastered.esp", "smithing perks overhaul se.esp" };

		// alandtse's CommonLibSSE-NG has TESDataHandler::AddFormToDataHandler
		bool AddForm(RE::TESDataHandler* a_dh, RE::TESForm* a_form) { return a_dh->AddFormToDataHandler(a_form); }

		std::string FileOf(const RE::TESForm* a_form)
		{
			const auto* file = a_form ? a_form->GetFile(0) : nullptr;
			return file ? Lower(std::string(file->GetFilename())) : std::string();
		}

		// the vanilla-style material keywords of an item, sorted and joined ("ArmorMaterialIron", "WeapMaterialSteel", ...)
		std::string MaterialKey(const RE::BGSKeywordForm* a_kf)
		{
			std::vector<std::string> out;
			if (a_kf && a_kf->keywords) {
				for (std::uint32_t i = 0; i < a_kf->numKeywords; ++i) {
					const auto* k = a_kf->keywords[i];
					if (!k || !k->GetFormEditorID()) {
						continue;
					}
					const std::string n = k->GetFormEditorID();
					const auto        l = Lower(n);
					if (l.find("material") != std::string::npos && l.rfind("waf_", 0) != 0) {
						out.push_back(n);
					}
				}
			}
			std::sort(out.begin(), out.end());
			std::string key;
			for (const auto& n : out) {
				key += (key.empty() ? "" : "+") + n;
			}
			return key;
		}

		// 0 body, 1 feet, 2 hands, 3 head, 4 other, 5 shield; weapons: 10 + type
		int SlotOf(const RE::TESBoundObject* a_obj)
		{
			if (const auto* a = a_obj->As<RE::TESObjectARMO>()) {
				using S = RE::BGSBipedObjectForm::BipedObjectSlot;
				if (a->HasPartOf(S::kShield)) {
					return 5;
				}
				return a->HasPartOf(S::kBody) ? 0 : a->HasPartOf(S::kFeet) ? 1 : a->HasPartOf(S::kHands) ? 2 :
				       (a->HasPartOf(S::kHead) || a->HasPartOf(S::kHair) || a->HasPartOf(S::kCirclet)) ? 3 : 4;
			}
			if (const auto* w = a_obj->As<RE::TESObjectWEAP>()) {
				return 10 + static_cast<int>(w->GetWeaponType());
			}
			return -1;
		}

		// the keyword form of a piece of gear (armor and weapon both derive from it; a cross-cast from the bound object is not reliable)
		const RE::BGSKeywordForm* KeywordsOf(RE::TESBoundObject* a_obj)
		{
			if (auto* a = a_obj ? a_obj->As<RE::TESObjectARMO>() : nullptr) {
				return a;
			}
			if (auto* w = a_obj ? a_obj->As<RE::TESObjectWEAP>() : nullptr) {
				return w;
			}
			return nullptr;
		}

		bool IsGear(const RE::TESForm* a_form) { return a_form && (a_form->Is(RE::FormType::Armor) || a_form->Is(RE::FormType::Weapon)); }

		// the one required item of a recipe, or null when it has none or several
		RE::TESBoundObject* SingleRequired(const RE::BGSConstructibleObject* a_c)
		{
			if (a_c->requiredItems.numContainerObjects != 1 || !a_c->requiredItems.containerObjects || !a_c->requiredItems.containerObjects[0]) {
				return nullptr;
			}
			return a_c->requiredItems.containerObjects[0]->obj;
		}

		// the highest-count ingredient of a crafting recipe, with its count
		std::pair<RE::TESBoundObject*, int> MainIngredient(const RE::BGSConstructibleObject* a_c)
		{
			RE::TESBoundObject* best = nullptr;
			int                 top = 0;
			for (std::uint32_t i = 0; i < a_c->requiredItems.numContainerObjects; ++i) {
				if (const auto* e = a_c->requiredItems.containerObjects[i]; e && e->obj && e->count > top) {
					top = e->count;
					best = e->obj;
				}
			}
			return { best, top };
		}

		struct Template
		{
			const RE::BGSConstructibleObject* recipe{ nullptr };
			RE::TESBoundObject*               item{ nullptr };
			RE::TESBoundObject*               product{ nullptr };
			RE::BGSKeyword*                   bench{ nullptr };
			int                               count{ 1 };
		};

		// a copy of a reference recipe's conditions with the reference item swapped for the new item
		RE::TESConditionItem* CloneConditions(const RE::BGSConstructibleObject* a_from, RE::TESForm* a_oldItem, RE::TESForm* a_newItem)
		{
			RE::TESConditionItem* head = nullptr;
			RE::TESConditionItem* tail = nullptr;
			for (auto* c = a_from->conditions.head; c; c = c->next) {
				auto* n = new RE::TESConditionItem;
				n->data = c->data;
				n->next = nullptr;
				if (n->data.functionData.params[0] == static_cast<void*>(a_oldItem)) {
					n->data.functionData.params[0] = a_newItem;
				}
				if (n->data.functionData.params[1] == static_cast<void*>(a_oldItem)) {
					n->data.functionData.params[1] = a_newItem;
				}
				(tail ? tail->next : head) = n;
				tail = n;
			}
			return head;
		}

		int YieldFor(int a_need) { return std::max(1, a_need - 1); }
	}

	void Run(const std::string& a_mode)
	{
		if (a_mode == "off") {
			SKSE::log::info("Breakdown: switched off in config.json");
			return;
		}
		auto* dh = RE::TESDataHandler::GetSingleton();
		if (!dh || !dh->LookupModByName("Complete Crafting Overhaul_Remastered.esp")) {
			SKSE::log::info("Breakdown: Complete Crafting Overhaul Remastered is not loaded, nothing to do");
			return;
		}
		auto* smelter = dh->LookupForm<RE::BGSKeyword>(0x0A5CCE, "Skyrim.esm");
		auto* tanning = dh->LookupForm<RE::BGSKeyword>(0x07866A, "Skyrim.esm");
		if (!smelter || !tanning) {
			return;
		}
		const bool apply = a_mode == "on";

		// ---- 1. CCOR's own breakdown recipes: one required piece of gear, made at the smelter or the tanning rack
		std::vector<Template>              refs;
		std::unordered_set<RE::FormID>     hasBreakdown;  // gear that already has a recipe taking it apart at one of the two benches
		for (auto* c : dh->GetFormArray<RE::BGSConstructibleObject>()) {
			if (!c || (c->benchKeyword != smelter && c->benchKeyword != tanning) || !c->createdItem) {
				continue;
			}
			auto* item = SingleRequired(c);
			if (!item || !IsGear(item)) {
				continue;
			}
			hasBreakdown.insert(item->GetFormID());
			if (FileOf(c) == "complete crafting overhaul_remastered.esp" && c->conditions.head) {
				refs.push_back({ c, item, c->createdItem->As<RE::TESBoundObject>(), c->benchKeyword, static_cast<int>(c->data.numConstructed) });
			}
		}
		SKSE::log::info("Breakdown: {} reference recipes from Complete Crafting Overhaul, {} pieces of gear already have one", refs.size(), hasBreakdown.size());
		if (refs.empty()) {
			return;
		}

		// reference by (weapon or armor, material, slot): the product the reference pieces agree on
		struct Group
		{
			std::map<RE::FormID, int> product;
			const Template*           first{ nullptr };
		};
		std::map<std::string, Group> groups;
		const auto                   gkey = [](const RE::TESBoundObject* a_item, const std::string& a_mat, int a_slot) {
            return std::string(a_item->Is(RE::FormType::Weapon) ? "W|" : "A|") + a_mat + "|" + std::to_string(a_slot);
		};
		for (const auto& t : refs) {
			const auto* kf = KeywordsOf(t.item);
			const auto  mat = MaterialKey(kf);
			if (mat.empty() || !t.product) {
				continue;
			}
			auto& g = groups[gkey(t.item, mat, SlotOf(t.item))];
			++g.product[t.product->GetFormID()];
			if (!g.first) {
				g.first = &t;
			}
		}

		// ---- 2. the crafting recipes of the other mods' gear: item -> main ingredient and its count
		std::unordered_map<RE::FormID, std::pair<RE::TESBoundObject*, int>> craft;
		for (auto* c : dh->GetFormArray<RE::BGSConstructibleObject>()) {
			if (!c || !c->createdItem || c->benchKeyword == smelter || c->benchKeyword == tanning || !c->createdItem->As<RE::TESBoundObject>()) {
				continue;
			}
			if (!IsGear(c->createdItem)) {
				continue;
			}
			const auto m = MainIngredient(c);
			if (m.first && !craft.count(c->createdItem->GetFormID())) {
				craft[c->createdItem->GetFormID()] = m;
			}
		}

		// ---- 3. every other mod's piece of gear without a breakdown recipe
		nlohmann::json planned = nlohmann::json::array();
		std::map<std::string, int> skipped;
		// reason -> plugin -> { count, craftable, samples }
		std::map<std::string, std::map<std::string, nlohmann::json>> detail;
		const auto note = [&](const std::string& a_reason, const std::string& a_plugin, const RE::TESBoundObject* a_obj, const char* a_name) {
			++skipped[a_reason];
			auto& d = detail[a_reason][a_plugin];
			if (d.is_null()) {
				d = { { "count", 0 }, { "craftable", 0 }, { "samples", nlohmann::json::array() } };
			}
			d["count"] = d["count"].get<int>() + 1;
			if (craft.count(a_obj->GetFormID())) {
				d["craftable"] = d["craftable"].get<int>() + 1;
			}
			if (d["samples"].size() < 6) {
				d["samples"].push_back(a_name ? a_name : "");
			}
		};
		int                        made = 0, considered = 0;
		auto&                      cobjs = dh->GetFormArray<RE::BGSConstructibleObject>();
		const auto                 consider = [&](RE::TESBoundObject* a_obj, const char* a_name, bool a_playable) {
            const auto plugin = FileOf(a_obj);
            if (plugin.empty() || kSkipPlugins.count(plugin)) {
                return;
            }
            ++considered;
            if (!a_playable || !a_name || !*a_name) {
                note("not playable or unnamed", plugin, a_obj, a_name);
                return;
            }
            if (hasBreakdown.count(a_obj->GetFormID())) {
                note("already has a breakdown recipe", plugin, a_obj, a_name);
                return;
            }
            const auto* kf = KeywordsOf(a_obj);
            const auto  mat = MaterialKey(kf);
            if (mat.empty()) {
                note("no material keyword", plugin, a_obj, a_name);
                return;
            }
            const auto g = groups.find(gkey(a_obj, mat, SlotOf(a_obj)));
            if (g == groups.end()) {
                note("no reference recipe for this material and slot", plugin, a_obj, a_name);
                return;
            }
            const auto& grp = g->second;
            // the reference pieces must agree on the product
            RE::FormID bestProduct = 0;
            int        bestN = 0, total = 0;
            for (const auto& [id, n] : grp.product) {
                total += n;
                if (n > bestN) {
                    bestN = n;
                    bestProduct = id;
                }
            }
            if (bestN * 10 < total * 8) {
                note("reference recipes disagree on the product", plugin, a_obj, a_name);
                return;
            }
            const auto cr = craft.find(a_obj->GetFormID());
            if (cr == craft.end() || !cr->second.first || cr->second.first->GetFormID() != bestProduct) {
                note("no crafting recipe made of the reference product (amount unknown)", plugin, a_obj, a_name);
                return;
            }
            const int          yield = YieldFor(cr->second.second);
            const Template&    t = *grp.first;
            nlohmann::json     row = { { "name", a_name }, { "plugin", plugin }, { "material", mat }, { "product", t.product ? (t.product->GetName() ? t.product->GetName() : "") : "" },
                { "need", cr->second.second }, { "yield", yield }, { "bench", t.bench == smelter ? "smelter" : "tanning rack" } };
            if (apply) {
                auto* factory = RE::IFormFactory::GetConcreteFormFactoryByType<RE::BGSConstructibleObject>();
                auto* r = factory ? factory->Create() : nullptr;
                if (!r) {
                    ++skipped["could not create the recipe"];
                    return;
                }
                r->requiredItems.AddObjectToContainer(a_obj, 1, nullptr);
                r->createdItem = t.product;
                r->benchKeyword = t.bench;
                r->data.numConstructed = static_cast<std::uint16_t>(yield);
                r->conditions.head = CloneConditions(t.recipe, t.item, a_obj);
                if (!AddForm(dh, r)) {
                    ++skipped["could not register the recipe"];
                    return;
                }
                ++made;
                row["recipe"] = std::format("{:08X}", r->GetFormID());
            }
            planned.push_back(std::move(row));
        };
		for (auto* a : dh->GetFormArray<RE::TESObjectARMO>()) {
			if (a) {
				consider(a, a->GetName(), a->GetPlayable());
			}
		}
		for (auto* w : dh->GetFormArray<RE::TESObjectWEAP>()) {
			if (w) {
				consider(w, w->GetName(), w->GetPlayable());
			}
		}
		SKSE::log::info("Breakdown ({}): looked at {} pieces of gear from other mods, {} {} ({} left); COBJ array now {}", a_mode, considered, planned.size(),
			apply ? "recipes added" : "recipes planned", considered - static_cast<int>(planned.size()), cobjs.size());

		try {
			std::error_code ec;
			std::filesystem::create_directories(Dir(), ec);
			nlohmann::json j;
			j["mode"] = a_mode;
			j["references"] = refs.size();
			j["alreadyCovered"] = hasBreakdown.size();
			j["considered"] = considered;
			j["skipped"] = skipped;
			j["skippedDetail"] = detail;
			j["recipes"] = planned;
			std::ofstream out(Dir() / "breakdown.json");
			out << j.dump(1);
		} catch (const std::exception& e) {
			SKSE::log::warn("could not write breakdown.json: {}", e.what());
		}
	}
}
