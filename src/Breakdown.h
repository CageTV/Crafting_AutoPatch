/*
 * Crafting - AutoPatch
 * Copyright (c) 2026 CageTV
 *
 * Released under the MIT License; see LICENSE.
 */
#pragma once

#include <string>

namespace Breakdown
{
	// Complete Crafting Overhaul Remastered lets the player break gear down at the smelter or the tanning rack, one recipe per item.
	// This finds the weapons and armor of other mods that have no such recipe, works out the product and the amount from CCOR's own
	// recipes and from the item's own crafting recipe, and (mode "on") adds the recipes with CCOR's conditions copied from a reference
	// recipe of the same material. Mode "plan" only writes Data/SKSE/Plugins/CraftingAutoPatch/breakdown.json.
	// mode: "off", "plan" or "on".
	void Run(const std::string& a_mode);
}
