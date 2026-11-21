/*
 * Crafting - AutoPatch
 * Copyright (c) 2026 CageTV
 *
 * Released under the MIT License; see LICENSE.
 */
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

namespace Breakdown
{
	// Complete Crafting Overhaul Remastered lets the player break gear down at the smelter or the tanning rack, one recipe per item.
	// This finds the weapons and armor of other mods that have no such recipe, works out the product and the amount from CCOR's own
	// recipes and from the item's own crafting recipe, and (mode "on") adds the recipes with CCOR's conditions copied from a reference
	// recipe of the same material. Mode "plan" only writes Data/SKSE/Plugins/CraftingAutoPatch/breakdown.json.
	// mode: "off", "plan" or "on".
	// one piece of gear that got (mode "on") or would get (mode "plan") a breakdown recipe
	struct Row
	{
		std::string name, plugin, material, product, bench, via, recipe, item;  // item: the form ID of the gear, 8 hex digits
		int         need = 0, yield = 0;
	};

	struct Summary
	{
		bool                       ran = false;
		std::string                mode;  // what this game session used
		int                        references = 0, enchantedReferences = 0, alreadyCovered = 0, considered = 0, made = 0;
		std::map<std::string, int> skipped;  // reason -> items
		std::map<std::string, int> how;      // how the product was found -> items
	};

	// plugins in a_exclude (lower case file names, the Mods page) are never touched
	void Run(const std::string& a_mode, const std::set<std::string>& a_exclude);

	const Summary&          Info();
	const std::vector<Row>& Rows();
}
