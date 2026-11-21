/*
 * Crafting - AutoPatch
 * Copyright (c) 2026 CageTV
 *
 * Released under the GNU General Public License, version 3 or later; see LICENSE and NOTICE.txt.
 */
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

namespace Core
{
	// Data/SKSE/Plugins/CraftingAutoPatch/config.json
	struct Config
	{
		bool                  apply = true;      // false: only write the report
		bool                  weapons = true;    // tag other mods' weapons
		bool                  armor = true;      // tag other mods' armor
		std::string           clothing = "plan"; // "plan": clothing protection tiers are only planned; "on": applied; "off": ignored
		bool                  useNames = true;
		bool                  classKeywords = true;  // SPIKE and matching-set keywords learned per material  // a material found only from the item's name is applied (otherwise only planned)
		std::string           breakdown = "on";  // CCOR breakdown recipes for other mods' gear: "off", "plan" (report only) or "on"
		std::set<std::string> exclude;           // plugin names (lower case) never touched
	};

	enum class Status { Changed, Planned, Left };

	// one weapon or armor that was looked at
	struct Row
	{
		bool        weapon = false;
		Status      status = Status::Left;
		float       confidence = 0.0f;
		std::string name, plugin, kind, material, how, reason, now, planned;
	};

	struct PluginStat
	{
		int items = 0, changed = 0, planned = 0, left = 0;
	};

	struct Summary
	{
		bool                           ran = false, waccf = false, ace = false, ccor = false, ussep = false, spo = false;
		int                            referenceItems = 0, referenceGroups = 0, learnedIngredients = 0, learnedWords = 0, clothingReferences = 0, classRules = 0;
		std::map<std::string, int>     skipped;  // reason -> how many
		std::map<std::string, PluginStat> plugins;  // plugin -> counts
	};

	// Learns what Weapons Armor Clothing and Clutter Fixes (and the game itself, with the Unofficial Patch) do to the items they know, finds other mods'
	// weapons and armor, works out their material from their own keywords, their crafting recipes and their names, adds the keywords the smithing
	// perks and Complete Crafting Overhaul need (unless config.json says apply:false) and writes Data/SKSE/Plugins/CraftingAutoPatch/report.json.
	void Run();

	Config&                 Cfg();
	void                    SaveConfig();
	const Summary&          Info();
	const std::vector<Row>& Rows();
}
