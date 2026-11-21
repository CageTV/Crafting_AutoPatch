# Crafting - AutoPatch

An SKSE add-on for **Weapons Armor Clothing and Clutter Fixes** (WACCF, Nexus 18994), **Armor and Clothing Extension** (Nexus 19002) and **Complete Crafting Overhaul Remastered** (CCOR, Nexus 28608), all by kryptopyr. Version 0.2.0.

The weapons and armor that other mods add know nothing about the keywords and recipes these mods use, so a patch plugin has to be made for each mod. This add-on does that work at game start, for every mod, with no patch plugin.

## What it does

**1. Material keywords.** WACCF defines a keyword system (`WAF_TreatAsMaterial<Material>`, `WAF_TemperBonus<Material>Perk`, ...) that the smithing and tempering perks and CCOR use.

1. It learns from the game's own items. It reads the live records of the weapons and armor the game and the Unofficial Skyrim Special Edition Patch define and works out which material keyword goes with which crafting ingredient and which word in an item's name. It also learns the class keywords the game's own pieces of a material carry (the fist-perk keywords on gauntlets, `WAF_Material...` markers).
2. It finds the weapons and armor other mods add and works out each one's material: its own material keyword, else the ingredients of its crafting recipes, else its name.
3. It tags them with the keywords WACCF uses for the same material.

**2. Breakdown recipes.** CCOR lets you take gear apart at the smelter or the tanning rack for its metal or leather, but only the game's own gear has those recipes. This adds the same recipes for the gear other mods add.

- The product comes from the item's own crafting recipe when it can (an ingot or leather the recipe uses), otherwise from what CCOR's own recipes give for the same material and slot. The amount follows CCOR's own rule (the crafting amount minus one, at least one), or the usual amount for that material and slot when the item has no crafting recipe.
- The recipes carry CCOR's conditions: its category and master switches, and equipped and favourite items are protected.
- Enchanted gear is left out, because CCOR's own recipes are for plain pieces.
- Items that already have a breakdown recipe are left alone.
- The recipes are made in memory at every game start. Nothing is written to your save.

Everything is listed in the SKSE Menu Framework (Overview, Mods, Weapons, Armor, Breakdown, Skipped) and in `Data/SKSE/Plugins/CraftingAutoPatch/report.json` and `breakdown.json`, with the reason for every item it left alone. Remove the add-on and everything is as the mods made it.

Items already tagged by WACCF, ACE, CCOR or another patch, items the game and the Unofficial Patch define, jewelry (CCOR handles it), staves and unplayable items are never given material keywords. A material found only from an item's name is only listed ("Plan only") unless *Also apply guesses* is on (it is by default). Clothing protection tiers (ACE's armor value for clothing) are matched to the nearest similar reference clothing by price and are only listed unless you set *Clothing tiers* to *on*.

## Requirements

- Skyrim Special Edition, Anniversary Edition or VR. The installer asks which DLL to install: the new build (Skyrim 1.6.1170 and newer, including 1.7.x) or the older build (Skyrim VR and 1.6.1130 and older). The breakdown recipes are not available in the older build on Skyrim VR.
- SKSE, Address Library
- Weapons Armor Clothing and Clutter Fixes (required); Complete Crafting Overhaul Remastered for the breakdown recipes
- Optional and recommended: Unofficial Skyrim Special Edition Patch, Armor and Clothing Extension, SKSE Menu Framework 3 (for the pages and switches)

## Settings

`Data/SKSE/Plugins/CraftingAutoPatch/config.json` is created the first time you change a switch in the menu. A switch takes effect at the next game start.

| Key | Default | |
|---|---|---|
| `apply` | `true` | `false`: only write the report, change nothing |
| `weapons`, `armor` | `true` | switch a kind off |
| `clothing` | `"plan"` | `"plan"`, `"on"` or `"off"` |
| `useNames` | `true` | also apply a material that is only a guess (from the name, or from the closest vanilla pieces by armor rating or damage) |
| `classKeywords` | `true` | the extra class keywords learned per material |
| `breakdown` | `"on"` | `"on"`, `"plan"` (only listed) or `"off"` |
| `excludePlugins` | `[]` | plugin names (lower case) whose items are never touched, by either feature |

## Credits and permissions

Weapons Armor Clothing and Clutter Fixes, Armor and Clothing Extension and Complete Crafting Overhaul Remastered are by **kryptopyr**. Their permissions allow compatibility patches and the inclusion of keywords and other record edits for compatibility; this add-on ships none of their records, scripts or assets and only reads them while the game runs. Please endorse and support the original mods:

- https://www.nexusmods.com/skyrimspecialedition/mods/18994
- https://www.nexusmods.com/skyrimspecialedition/mods/19002
- https://www.nexusmods.com/skyrimspecialedition/mods/28608

SKSE Menu Framework by Thiago099 (the menu header in `include/`). Built on CommonLibSSE-NG (alandtse for the new build, CharmedBaryon for the older one).

## Building

- New build: `ng.cmake` plugs into a shared CommonLibSSE-NG build tree (`ng_plugin`).
- Older build: `legacy/` (`legacy/build.cmd`; it expects CommonLibSSE, spdlog and fmt installed through vcpkg, see the comment in the script).
- Sources are in `src/`; `scripts/make_release.py` assembles the installer package.

## License

GNU General Public License, version 3 or later; see `LICENSE` and `NOTICE.txt` (which also lists the third-party code in the DLLs). The complete source of every download is in this repository.
