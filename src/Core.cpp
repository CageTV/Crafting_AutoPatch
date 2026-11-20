/*
 * Crafting - AutoPatch
 * Copyright (c) 2026 CageTV
 *
 * Released under the MIT License; see LICENSE.
 */
#include "PCH.h"
#include "Core.h"

#include <nlohmann/json.hpp>

// What this does, in short.
//  1. Learn from the items the game and the Unofficial Patch define: which material keyword each one carries, which crafting ingredients and which
//     words in the name go with which material. (The live records are read, so the Unofficial Patch's changes and Weapons Armor Clothing and
//     Clutter Fixes' changes are already in what is learned.)
//  2. Find the weapons and armor that other mods add. Work out the material of each: its own material keyword, else its crafting recipes' ingredients,
//     else its name.
//  3. Give it the keywords Weapons Armor Clothing and Clutter Fixes (WACCF) uses for the same thing (WAF_TreatAsMaterial<Material>,
//     WAF_TemperBonus<Material>Perk), so the smithing and tempering perks and Complete Crafting Overhaul treat it like the game's own gear.
//  Nothing is written to a save and the records are only changed in memory: switch it off and everything is as the mods made it.
namespace Core
{
	namespace
	{
		Config                g_cfg;
		Summary               g_info;
		std::vector<Row>      g_rows;

		std::filesystem::path Dir() { return std::filesystem::path("Data/SKSE/Plugins/CraftingAutoPatch"); }

		std::string Lower(std::string a_s)
		{
			std::transform(a_s.begin(), a_s.end(), a_s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return a_s;
		}

		bool Contains(const std::string& a_s, const char* a_part) { return a_s.find(a_part) != std::string::npos; }
		bool StartsWith(const std::string& a_s, const char* a_part) { return a_s.rfind(a_part, 0) == 0; }

		std::string Join(const std::vector<std::string>& a_v, const char* a_sep)
		{
			std::string out;
			for (std::size_t i = 0; i < a_v.size(); ++i) {
				out += (i ? a_sep : "") + a_v[i];
			}
			return out;
		}

		// The plugins whose items are the reference or are already part of the system: never tagged.
		const std::set<std::string> kVanilla = { "skyrim.esm", "update.esm", "dawnguard.esm", "hearthfires.esm", "dragonborn.esm",
			"unofficial skyrim special edition patch.esp" };
		const std::set<std::string> kFamily = { "weapons armor clothing & clutter fixes.esp", "waccf_armor and clothing extension.esp",
			"complete crafting overhaul_remastered.esp", "smithing perks overhaul se.esp" };

		// ---------------------------------------------------------------- config

		void LoadConfig()
		{
			g_cfg = Config{};
			const auto path = Dir() / "config.json";
			std::ifstream in(path);
			if (!in) {
				return;
			}
			const auto j = nlohmann::json::parse(in, nullptr, false, true);
			if (j.is_discarded()) {
				SKSE::log::error("{} is not valid JSON: using the defaults", path.string());
				return;
			}
			g_cfg.apply = j.value("apply", true);
			g_cfg.weapons = j.value("weapons", true);
			g_cfg.armor = j.value("armor", true);
			g_cfg.clothing = j.value("clothing", std::string("plan"));
			g_cfg.useNames = j.value("useNames", true);
			for (const auto& e : j.value("excludePlugins", nlohmann::json::array())) {
				g_cfg.exclude.insert(Lower(e.get<std::string>()));
			}
		}

		// ---------------------------------------------------------------- keywords and items

		struct Item
		{
			RE::TESBoundObject* form{ nullptr };
			RE::BGSKeywordForm* kf{ nullptr };
			bool                weapon{ false };
			int                 armorType{ 0 };  // 0 light, 1 heavy, 2 clothing (armor only)
			int                 slotClass{ 4 };  // 0 body, 1 feet, 2 hands, 3 head, 4 other (armor only)
			bool                shield{ false };
			int                 weaponType{ 0 };
			std::string         plugin, name;
			int                 value{ 0 };
			int                 power{ 0 };  // armor rating or weapon damage
			bool                playable{ true };
		};

		std::vector<std::string> KeywordNames(const RE::BGSKeywordForm* a_kf)
		{
			std::vector<std::string> out;
			if (!a_kf || !a_kf->keywords) {
				return out;
			}
			for (std::uint32_t i = 0; i < a_kf->numKeywords; ++i) {
				if (const auto* k = a_kf->keywords[i]; k && k->GetFormEditorID() && *k->GetFormEditorID()) {
					out.emplace_back(k->GetFormEditorID());
				}
			}
			return out;
		}

		bool HasName(const std::vector<std::string>& a_names, const char* a_lowerName)
		{
			for (const auto& n : a_names) {
				if (Lower(n) == a_lowerName) {
					return true;
				}
			}
			return false;
		}

		// the vanilla-style material keywords of an item (EditorID contains "material", not one of WACCF's own WAF_ ones)
		std::vector<std::string> MaterialNames(const std::vector<std::string>& a_kw)
		{
			std::vector<std::string> out;
			for (const auto& n : a_kw) {
				const auto l = Lower(n);
				if (Contains(l, "material") && !StartsWith(l, "waf_")) {
					out.push_back(n);
				}
			}
			std::sort(out.begin(), out.end());
			return out;
		}

		std::string Prefix(const Item& a_it) { return a_it.weapon ? "W:" : "A:"; }

		const char* WeaponKind(int a_type)
		{
			switch (static_cast<RE::WEAPON_TYPE>(a_type)) {
			case RE::WEAPON_TYPE::kOneHandSword: return "One-handed sword";
			case RE::WEAPON_TYPE::kOneHandDagger: return "Dagger";
			case RE::WEAPON_TYPE::kOneHandAxe: return "One-handed axe";
			case RE::WEAPON_TYPE::kOneHandMace: return "One-handed mace";
			case RE::WEAPON_TYPE::kTwoHandSword: return "Greatsword";
			case RE::WEAPON_TYPE::kTwoHandAxe: return "Battleaxe or warhammer";
			case RE::WEAPON_TYPE::kBow: return "Bow";
			case RE::WEAPON_TYPE::kCrossbow: return "Crossbow";
			case RE::WEAPON_TYPE::kStaff: return "Staff";
			default: return "Weapon";
			}
		}

		std::string KindOf(const Item& a_it)
		{
			if (a_it.weapon) {
				return WeaponKind(a_it.weaponType);
			}
			if (a_it.shield) {
				return "Shield";
			}
			return a_it.armorType == 0 ? "Light armor" : a_it.armorType == 1 ? "Heavy armor" : "Clothing";
		}

		// the part of a material keyword after "Material": ArmorMaterialSteelPlate -> SteelPlate
		std::string Suffix(const std::string& a_name)
		{
			const auto l = Lower(a_name);
			const auto p = l.find("material");
			return p == std::string::npos ? a_name : a_name.substr(p + 8);
		}

		// SteelPlate -> {SteelPlate, Steel}; ImperialHeavy -> {ImperialHeavy, Imperial}
		std::vector<std::string> SuffixCandidates(const std::string& a_suffix)
		{
			static const std::map<std::string, std::string> kAlias = { { "dragonbone", "Dragon" }, { "dragonscale", "Dragon" }, { "dragonplate", "Dragon" },
				{ "malachite", "Glass" }, { "stalhrimlight", "Stalhrim" }, { "stalhrimheavy", "Stalhrim" } };
			std::vector<std::string> out;
			std::string              cur = a_suffix;
			while (!cur.empty()) {
				out.push_back(cur);
				if (const auto a = kAlias.find(Lower(cur)); a != kAlias.end()) {
					out.push_back(a->second);
				}
				std::size_t cut = std::string::npos;
				for (std::size_t i = cur.size(); i-- > 1;) {
					if (std::isupper(static_cast<unsigned char>(cur[i]))) {
						cut = i;
						break;
					}
				}
				if (cut == std::string::npos) {
					break;
				}
				cur = cur.substr(0, cut);
			}
			return out;
		}

		struct Votes
		{
			std::map<std::string, int> n;  // group key -> votes
			int                        total = 0;
			void Add(const std::string& a_key)
			{
				++n[a_key];
				++total;
			}
			std::pair<std::string, float> Best() const
			{
				std::string best;
				int         top = 0;
				for (const auto& [k, c] : n) {
					if (c > top) {
						top = c;
						best = k;
					}
				}
				return { best, total ? static_cast<float>(top) / static_cast<float>(total) : 0.0f };
			}
		};

		// the main ingredient(s) of a recipe: the one with the highest count (ingots, not the strip of leather or the firewood that goes with them)
		std::vector<RE::TESBoundObject*> MainIngredients(const RE::BGSConstructibleObject* a_c)
		{
			std::vector<RE::TESBoundObject*> out;
			std::int32_t                     top = 0;
			for (std::uint32_t i = 0; i < a_c->requiredItems.numContainerObjects; ++i) {
				if (const auto* e = a_c->requiredItems.containerObjects[i]; e && e->obj) {
					top = std::max(top, e->count);
				}
			}
			for (std::uint32_t i = 0; i < a_c->requiredItems.numContainerObjects; ++i) {
				if (const auto* e = a_c->requiredItems.containerObjects[i]; e && e->obj && e->count == top) {
					out.push_back(e->obj);
				}
			}
			return out;
		}

		struct ClothRef
		{
			int         slotClass;
			int         value;
			std::string keyword;
		};
	}

	Config&                 Cfg() { return g_cfg; }
	const Summary&          Info() { return g_info; }
	const std::vector<Row>& Rows() { return g_rows; }

	void SaveConfig()
	{
		std::error_code ec;
		std::filesystem::create_directories(Dir(), ec);
		nlohmann::json j;
		j["apply"] = g_cfg.apply;
		j["weapons"] = g_cfg.weapons;
		j["armor"] = g_cfg.armor;
		j["clothing"] = g_cfg.clothing;
		j["useNames"] = g_cfg.useNames;
		j["excludePlugins"] = nlohmann::json::array();
		for (const auto& e : g_cfg.exclude) {
			j["excludePlugins"].push_back(e);
		}
		std::ofstream out(Dir() / "config.json");
		out << j.dump(2);
	}

	void Run()
	{
		auto* dh = RE::TESDataHandler::GetSingleton();
		if (!dh) {
			return;
		}
		LoadConfig();
		g_rows.clear();
		g_info = Summary{};
		const auto have = [&](const char* a_name) { return dh->LookupModByName(a_name) != nullptr; };
		g_info.waccf = have("Weapons Armor Clothing & Clutter Fixes.esp");
		g_info.ace = have("WACCF_Armor and Clothing Extension.esp");
		g_info.ccor = have("Complete Crafting Overhaul_Remastered.esp");
		g_info.ussep = have("unofficial skyrim special edition patch.esp");
		g_info.spo = have("Smithing Perks Overhaul SE.esp");
		SKSE::log::info("Loaded: WACCF {}, ACE {}, CCOR {}, USSEP {}, Smithing Perks Overhaul {}", g_info.waccf, g_info.ace, g_info.ccor, g_info.ussep, g_info.spo);
		if (!g_info.waccf) {
			SKSE::log::info("Weapons Armor Clothing & Clutter Fixes is not loaded: nothing to do");
			return;
		}
		g_info.ran = true;

		// ---- the keyword table
		std::unordered_map<std::string, RE::BGSKeyword*> kwByName;
		for (auto* k : dh->GetFormArray<RE::BGSKeyword>()) {
			if (k && k->GetFormEditorID() && *k->GetFormEditorID()) {
				kwByName[Lower(k->GetFormEditorID())] = k;
			}
		}

		// ---- every weapon and armor
		std::vector<Item> items;
		items.reserve(14000);
		const auto defining = [](const RE::TESForm* f) -> std::string {
			const auto* file = f ? f->GetFile(0) : nullptr;
			return file ? Lower(std::string(file->GetFilename())) : std::string();
		};
		for (auto* a : dh->GetFormArray<RE::TESObjectARMO>()) {
			if (!a || a->IsDeleted()) {
				continue;
			}
			Item it;
			it.form = a;
			it.kf = a;
			it.weapon = false;
			it.armorType = static_cast<int>(a->bipedModelData.armorType.get());
			using S = RE::BGSBipedObjectForm::BipedObjectSlot;
			it.shield = a->HasPartOf(S::kShield);
			it.slotClass = a->HasPartOf(S::kBody) ? 0 : a->HasPartOf(S::kFeet) ? 1 : a->HasPartOf(S::kHands) ? 2
				: (a->HasPartOf(S::kHead) || a->HasPartOf(S::kHair) || a->HasPartOf(S::kCirclet)) ? 3 : 4;
			it.plugin = defining(a);
			it.name = a->GetName() ? a->GetName() : "";
			it.value = a->value;
			it.power = static_cast<int>(a->armorRating / 100);
			it.playable = a->GetPlayable();
			items.push_back(std::move(it));
		}
		for (auto* w : dh->GetFormArray<RE::TESObjectWEAP>()) {
			if (!w || w->IsDeleted()) {
				continue;
			}
			Item it;
			it.form = w;
			it.kf = w;
			it.weapon = true;
			it.weaponType = static_cast<int>(w->GetWeaponType());
			it.plugin = defining(w);
			it.name = w->GetName() ? w->GetName() : "";
			it.value = w->value;
			it.power = static_cast<int>(w->attackDamage);
			it.playable = w->GetPlayable();
			items.push_back(std::move(it));
		}
		SKSE::log::info("{} weapons and armor in the load order", items.size());

		// ---- the recipes, by the item they create
		std::unordered_map<RE::FormID, std::vector<RE::BGSConstructibleObject*>> recipesFor;
		for (auto* c : dh->GetFormArray<RE::BGSConstructibleObject>()) {
			if (c && c->createdItem) {
				recipesFor[c->createdItem->GetFormID()].push_back(c);
			}
		}

		// ---- 1. learn from the game's own items
		std::map<std::string, int>                           groupSize;
		std::unordered_map<RE::FormID, Votes>                ingredientVotes;  // key: ingredient form id (prefix inside the group key)
		std::unordered_map<RE::FormID, std::string>          ingredientName;
		std::unordered_map<std::string, Votes>               wordVotes;
		std::vector<ClothRef>                                clothRefs;
		struct StatRef
		{
			bool        weapon;
			int         cls, power;
			std::string group;
		};
		std::vector<StatRef> statRefs;
		static const std::set<std::string> kStop = { "armor", "armour", "boots", "gauntlets", "gloves", "helmet", "shield", "cuirass", "sword", "dagger", "axe", "mace",
			"bow", "greatsword", "battleaxe", "warhammer", "hammer", "war", "and", "the", "with", "hood", "robes", "greaves", "crossbow", "staff", "plate",
			"banded", "light", "heavy", "arrow", "bolt", "bracers", "cap", "mask", "set", "quality", "of" };
		const auto words = [](const std::string& a_name) {
			std::vector<std::string> out;
			std::string              cur;
			for (unsigned char c : a_name) {
				if (std::isalpha(c)) {
					cur += static_cast<char>(std::tolower(c));
				} else {
					if (cur.size() >= 3) {
						out.push_back(cur);
					}
					cur.clear();
				}
			}
			if (cur.size() >= 3) {
				out.push_back(cur);
			}
			return out;
		};
		for (const auto& it : items) {
			if (!kVanilla.count(it.plugin) || !it.playable) {
				continue;
			}
			const auto kw = KeywordNames(it.kf);
			if (!it.weapon && it.armorType == 2) {
				for (const auto& n : kw) {
					if (StartsWith(Lower(n), "waf_protectiveclothingccf_")) {
						clothRefs.push_back({ it.slotClass, it.value, n });
						break;
					}
				}
				continue;
			}
			const auto mat = MaterialNames(kw);
			if (mat.empty()) {
				continue;
			}
			const std::string group = Prefix(it) + Join(mat, "+");
			++groupSize[group];
			++g_info.referenceItems;
			if (const auto r = recipesFor.find(it.form->GetFormID()); r != recipesFor.end()) {
				for (const auto* c : r->second) {
					for (auto* obj : MainIngredients(c)) {
						ingredientVotes[obj->GetFormID()].Add(group);
						if (!ingredientName.count(obj->GetFormID()) && obj->GetName()) {
							ingredientName[obj->GetFormID()] = obj->GetName();
						}
					}
				}
			}
			for (const auto& wd : words(it.name)) {
				if (!kStop.count(wd)) {
					wordVotes[Prefix(it) + wd].Add(group);
				}
			}
			if (it.power > 0 && !it.shield) {
				statRefs.push_back({ it.weapon, it.weapon ? it.weaponType : it.armorType * 10 + it.slotClass, it.power, group });
			}
		}
		g_info.referenceGroups = static_cast<int>(groupSize.size());
		g_info.clothingReferences = static_cast<int>(clothRefs.size());

		nlohmann::json learnedIng = nlohmann::json::array(), learnedWordsJson = nlohmann::json::array();
		// an ingredient or a word counts when the votes agree (armor and weapons are kept apart by the "A:" / "W:" prefix in the group key)
		std::unordered_map<std::string, std::pair<std::string, float>> ingredientMaterial;  // "A:<formid>" -> group, purity
		for (const auto& [id, v] : ingredientVotes) {
			// split the votes by prefix
			for (const char* pre : { "A:", "W:" }) {
				Votes sub;
				for (const auto& [k, c] : v.n) {
					if (StartsWith(k, pre)) {
						for (int i = 0; i < c; ++i) {
							sub.Add(k);
						}
					}
				}
				if (sub.total >= 3) {
					const auto [best, purity] = sub.Best();
					if (purity >= 0.7f) {
						ingredientMaterial[std::string(pre) + std::to_string(id)] = { best, purity };
						learnedIng.push_back({ { "ingredient", ingredientName.count(id) ? ingredientName[id] : std::to_string(id) }, { "for", best }, { "purity", purity }, { "votes", sub.total } });
						++g_info.learnedIngredients;
					}
				}
			}
		}
		std::unordered_map<std::string, std::pair<std::string, float>> wordMaterial;  // "A:steel" -> group, purity
		for (const auto& [w, v] : wordVotes) {
			if (v.total >= 2) {
				const auto [best, purity] = v.Best();
				if (purity >= 0.7f) {
					wordMaterial[w] = { best, purity };
					learnedWordsJson.push_back({ { "word", w }, { "for", best }, { "purity", purity }, { "votes", v.total } });
					++g_info.learnedWords;
				}
			}
		}
		SKSE::log::info("Learned from {} reference items in {} material groups: {} ingredients, {} name words, {} clothing references", g_info.referenceItems,
			g_info.referenceGroups, g_info.learnedIngredients, g_info.learnedWords, g_info.clothingReferences);

		// ---- 2 and 3: the other mods' weapons and armor
		const auto wafKeywords = [&](const std::string& a_group, std::vector<RE::BGSKeyword*>& a_out, std::string& a_material) {
			const auto names = [&]() {
				std::vector<std::string> n;
				std::string              cur;
				for (std::size_t i = 2; i < a_group.size(); ++i) {
					if (a_group[i] == '+') {
						n.push_back(cur);
						cur.clear();
					} else {
						cur += a_group[i];
					}
				}
				n.push_back(cur);
				return n;
			}();
			for (const auto& n : names) {
				const auto sfx = Suffix(n);
				if (a_material.empty()) {
					a_material = sfx;
				}
				for (const auto& cand : SuffixCandidates(sfx)) {
					bool found = false;
					for (const auto& tpl : { std::string("waf_treatasmaterial"), std::string("waf_temperbonus") }) {
						const std::string key = tpl + Lower(cand) + (tpl == "waf_temperbonus" ? "perk" : "");
						if (const auto k = kwByName.find(key); k != kwByName.end()) {
							if (std::find(a_out.begin(), a_out.end(), k->second) == a_out.end()) {
								a_out.push_back(k->second);
							}
							found = true;
						}
					}
					if (found) {
						break;
					}
				}
			}
		};

		const auto note = [&](Row& r, Status s, const std::string& why) {
			r.status = s;
			r.reason = why;
			if (s == Status::Left) {
				++g_info.skipped[why];
			}
		};

		for (const auto& it : items) {
			if (kVanilla.count(it.plugin) || kFamily.count(it.plugin) || it.plugin.empty()) {
				continue;
			}
			if (!it.playable) {
				++g_info.skipped["not playable (NPC outfit or hidden item)"];
				continue;
			}
			if (it.name.empty()) {
				++g_info.skipped["no name"];
				continue;
			}
			if (!recipesFor.count(it.form->GetFormID())) {
				++g_info.skipped["no crafting or tempering recipe (smithing perks only act on items you can craft or temper)"];
				continue;
			}
			if (it.weapon && (it.weaponType == static_cast<int>(RE::WEAPON_TYPE::kStaff) || it.weaponType == static_cast<int>(RE::WEAPON_TYPE::kHandToHandMelee))) {
				++g_info.skipped["staff or fists (no smithing material)"];
				continue;
			}
			Row r;
			r.weapon = it.weapon;
			r.name = it.name;
			r.plugin = it.plugin;
			r.kind = KindOf(it);
			const auto kw = KeywordNames(it.kf);
			auto&      ps = g_info.plugins[it.plugin];
			++ps.items;

			const auto finish = [&](Status s, const std::string& why) {
				note(r, s, why);
				if (s == Status::Changed) {
					++ps.changed;
				} else if (s == Status::Planned) {
					++ps.planned;
				} else {
					++ps.left;
				}
				g_rows.push_back(r);
			};

			std::string family;
			for (const auto& n : kw) {
				if (StartsWith(Lower(n), "waf_")) {
					family += (family.empty() ? "" : ", ") + n;
				}
			}
			r.now = family.empty() ? "no WAF keywords" : family;

			if (g_cfg.exclude.count(it.plugin)) {
				finish(Status::Left, "mod is excluded in config.json");
				continue;
			}
			if ((it.weapon && !g_cfg.weapons) || (!it.weapon && !g_cfg.armor)) {
				finish(Status::Left, it.weapon ? "weapons are switched off" : "armor is switched off");
				continue;
			}
			if (!family.empty()) {
				finish(Status::Left, "already carries WAF keywords");
				continue;
			}

			// ---- clothing: a protection tier from the nearest reference clothing
			if (!it.weapon && it.armorType == 2) {
				if (g_cfg.clothing == "off") {
					finish(Status::Left, "clothing is switched off");
					continue;
				}
				const ClothRef* best = nullptr;
				int             bestGap = 1 << 30;
				for (const auto& c : clothRefs) {
					if (c.slotClass != it.slotClass) {
						continue;
					}
					const int gap = std::abs(c.value - it.value);
					if (gap < bestGap) {
						bestGap = gap;
						best = &c;
					}
				}
				if (!best) {
					finish(Status::Left, "clothing: no reference piece for this slot");
					continue;
				}
				r.how = "nearest reference clothing by value (" + std::to_string(it.value) + " gold)";
				r.material = best->keyword;
				r.confidence = 0.4f;
				r.planned = "+ " + best->keyword;
				if (g_cfg.clothing == "on" && g_cfg.apply) {
					if (auto* k = kwByName.count(Lower(best->keyword)) ? kwByName[Lower(best->keyword)] : nullptr) {
						static_cast<RE::BGSKeywordForm*>(it.kf)->AddKeyword(k);
					}
					finish(Status::Changed, "clothing given a protection tier by value");
				} else {
					finish(Status::Planned, "clothing tiers are only planned (clothing is set to plan)");
				}
				continue;
			}

			// ---- jewelry is left to Complete Crafting Overhaul
			if (!it.weapon && (HasName(kw, "armorjewelry") || HasName(kw, "clothingring") || HasName(kw, "clothingnecklace") || HasName(kw, "clothingcirclet"))) {
				finish(Status::Left, "jewelry: Complete Crafting Overhaul handles jewelry itself");
				continue;
			}

			// ---- material
			const auto  own = MaterialNames(kw);
			std::string group;
			float       conf = 0.0f;
			if (!own.empty()) {
				group = Prefix(it) + Join(own, "+");
				conf = 1.0f;
				r.how = "its own material keyword";
				std::string mat;
				std::vector<RE::BGSKeyword*> unused;
				wafKeywords(group, unused, mat);
				r.material = mat;
				finish(Status::Left, "already follows the game's material keywords");
				continue;
			}
			if (const auto rec = recipesFor.find(it.form->GetFormID()); rec != recipesFor.end()) {
				Votes v;
				std::string firstIngredient;
				for (const auto* c : rec->second) {
					for (auto* obj : MainIngredients(c)) {
						if (const auto m = ingredientMaterial.find(Prefix(it) + std::to_string(obj->GetFormID())); m != ingredientMaterial.end()) {
							v.Add(m->second.first);
							if (firstIngredient.empty() && obj->GetName()) {
								firstIngredient = obj->GetName();
							}
						}
					}
				}
				if (v.total > 0) {
					group = v.Best().first;
					conf = 0.85f;
					r.how = "crafting ingredient: " + firstIngredient;
				}
			}
			if (group.empty()) {
				Votes v;
				std::string firstWord;
				for (const auto& wd : words(it.name)) {
					if (const auto m = wordMaterial.find(Prefix(it) + wd); m != wordMaterial.end()) {
						v.Add(m->second.first);
						if (firstWord.empty()) {
							firstWord = wd;
						}
					}
				}
				if (v.total > 0) {
					group = v.Best().first;
					conf = 0.6f;
					r.how = "name word: " + firstWord;
				}
			}
			if (group.empty() && it.power > 0 && !it.shield) {
				// no ingredient, no name word: the material of the vanilla pieces closest in kind and strength (the 5 nearest vote)
				const int cls = it.weapon ? it.weaponType : it.armorType * 10 + it.slotClass;
				std::vector<std::pair<int, const StatRef*>> closest;
				for (const auto& s : statRefs) {
					if (s.weapon == it.weapon && s.cls == cls) {
						closest.push_back({ std::abs(s.power - it.power), &s });
					}
				}
				std::sort(closest.begin(), closest.end(), [](const auto& x, const auto& y) { return x.first < y.first; });
				Votes v;
				for (std::size_t i = 0; i < closest.size() && i < 5; ++i) {
					v.Add(closest[i].second->group);
				}
				if (v.total >= 3) {
					const auto [best, purity] = v.Best();
					if (purity >= 0.6f) {
						group = best;
						conf = 0.5f;
						r.how = "closest vanilla pieces by " + std::string(it.weapon ? "damage " : "armor rating ") + std::to_string(it.power);
					}
				}
			}
			if (group.empty()) {
				r.how = "nothing recognised";
				finish(Status::Left, "material not recognised (no matching ingredient or name word)");
				continue;
			}
			std::vector<RE::BGSKeyword*> wanted;
			std::string                  mat;
			wafKeywords(group, wanted, mat);
			r.material = mat;
			r.confidence = conf;
			if (wanted.empty()) {
				// WACCF has no keyword for this material (leather, hide, chitin ...): give the item the game's own material keyword
				for (std::size_t i = 2, s = 2; i <= group.size(); ++i) {
					if (i == group.size() || group[i] == '+') {
						if (const auto k = kwByName.find(Lower(group.substr(s, i - s))); k != kwByName.end()) {
							wanted.push_back(k->second);
						}
						s = i + 1;
					}
				}
			}
			if (wanted.empty()) {
				finish(Status::Left, "no keyword exists for " + mat);
				continue;
			}
			std::string plan;
			for (const auto* k : wanted) {
				plan += (plan.empty() ? "+ " : ", + ") + std::string(k->GetFormEditorID());
			}
			r.planned = plan;
			const bool sure = conf >= 0.85f || g_cfg.useNames;
			if (sure && g_cfg.apply) {
				for (auto* k : wanted) {
					it.kf->AddKeyword(k);
				}
				finish(Status::Changed, "given the " + mat + " keywords");
			} else if (!sure) {
				finish(Status::Planned, "material is only a guess (name or closest vanilla pieces): planned, switch on 'Also apply guesses' to apply");
			} else {
				finish(Status::Planned, "apply is off: planned");
			}
		}

		int changed = 0, planned = 0, left = 0;
		for (const auto& r : g_rows) {
			(r.status == Status::Changed ? changed : r.status == Status::Planned ? planned : left)++;
		}
		SKSE::log::info("Looked at {} weapons and armor from other mods: {} changed, {} planned, {} left alone", g_rows.size(), changed, planned, left);

		// ---- the report
		try {
			std::error_code ec;
			std::filesystem::create_directories(Dir(), ec);
			nlohmann::json j;
			j["mods"] = { { "waccf", g_info.waccf }, { "ace", g_info.ace }, { "ccor", g_info.ccor }, { "ussep", g_info.ussep }, { "smithingPerksOverhaul", g_info.spo } };
			j["learned"] = { { "referenceItems", g_info.referenceItems }, { "materialGroups", g_info.referenceGroups }, { "ingredients", g_info.learnedIngredients },
				{ "nameWords", g_info.learnedWords }, { "clothingReferences", g_info.clothingReferences } };
			j["config"] = { { "apply", g_cfg.apply }, { "weapons", g_cfg.weapons }, { "armor", g_cfg.armor }, { "clothing", g_cfg.clothing }, { "useNames", g_cfg.useNames } };
			j["totals"] = { { "changed", changed }, { "planned", planned }, { "left", left } };
			j["skipped"] = g_info.skipped;
			j["learnedIngredients"] = learnedIng;
			j["learnedWords"] = learnedWordsJson;
			auto& pl = j["plugins"] = nlohmann::json::object();
			for (const auto& [p, s] : g_info.plugins) {
				pl[p] = { { "items", s.items }, { "changed", s.changed }, { "planned", s.planned }, { "left", s.left } };
			}
			auto& rows = j["items"] = nlohmann::json::array();
			for (const auto& r : g_rows) {
				rows.push_back({ { "name", r.name }, { "plugin", r.plugin }, { "kind", r.kind }, { "material", r.material }, { "how", r.how }, { "confidence", r.confidence },
					{ "status", r.status == Status::Changed ? "changed" : r.status == Status::Planned ? "planned" : "left" }, { "reason", r.reason }, { "now", r.now },
					{ "planned", r.planned } });
			}
			std::ofstream out(Dir() / "report.json");
			out << j.dump(1);
		} catch (const std::exception& e) {
			SKSE::log::warn("could not write report.json: {}", e.what());
		}
	}
}
