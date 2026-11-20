# Crafting - AutoPatch

An SKSE add-on for **Weapons Armor Clothing and Clutter Fixes** (WACCF, Nexus 18994), **Armor and Clothing Extension** (Nexus 19002) and **Complete Crafting Overhaul Remastered** (Nexus 28608), all by kryptopyr. Version 0.1.0, first build.

WACCF defines a keyword system (`WAF_TreatAsMaterial<Material>`, `WAF_TemperBonus<Material>Perk`, ...) that the smithing and tempering perks and Complete Crafting Overhaul (CCOR) use. The weapons and armor that other mods add know nothing about it, so a patch plugin has to be made for each mod. This add-on does that work at game start, for every mod, with no patch plugin.

## What it does

1. **Learns from the game's own items.** It reads the live records of the weapons and armor the game and the Unofficial Skyrim Special Edition Patch define (so the Unofficial Patch's changes, and WACCF's, are already in what it learns) and works out which material keyword goes with which crafting ingredient and which word in an item's name.
2. **Finds the weapons and armor other mods add** and works out each one's material: its own material keyword, else the ingredients of its crafting recipes, else its name.
3. **Tags them** with the keywords WACCF uses for the same material, so the smithing and tempering perks and CCOR treat them like the game's own gear.
4. **Lists everything it did** in the SKSE Menu Framework (Overview, Mods, Weapons, Armor, Skipped) and in `Data/SKSE/Plugins/CraftingAutoPatch/report.json`, with the reason for every item it left alone.

Nothing is written to your save and no plugin is added: the changes are made in memory while the game loads. Remove the add-on and everything is as the mods made it.

Items already tagged by WACCF, ACE, CCOR or another patch, items the game and the Unofficial Patch define, jewelry (CCOR handles it), staves and unplayable items are never changed. A material found only from an item's name is only listed ("Plan only") unless you switch on *Also use item names*. Clothing protection tiers (ACE's armor value for clothing) are matched to the nearest similar reference clothing by price and are only listed unless you set *Clothing tiers* to *on*.

## Requirements

- Skyrim Special Edition 1.5.97 or later (an older and a newer build are planned; this first build is for the 1.7.x runtime)
- SKSE, Address Library
- Weapons Armor Clothing and Clutter Fixes
- Optional and recommended: Unofficial Skyrim Special Edition Patch, Armor and Clothing Extension, Complete Crafting Overhaul Remastered, SKSE Menu Framework 3 (for the pages)

## Settings

`Data/SKSE/Plugins/CraftingAutoPatch/config.json` is created the first time you change a switch in the menu:

| Key | Default | |
|---|---|---|
| `apply` | `true` | `false`: only write the report, change nothing |
| `weapons`, `armor` | `true` | switch a kind off |
| `clothing` | `"plan"` | `"plan"`, `"on"` or `"off"` |
| `useNames` | `true` | also apply a material that is only a guess (from the name, or from the closest vanilla pieces by armor rating or damage) |
| `excludePlugins` | `[]` | plugin names (lower case) whose items are never touched |

## Credits and permissions

Weapons Armor Clothing and Clutter Fixes, Armor and Clothing Extension and Complete Crafting Overhaul Remastered are by **kryptopyr**. Their permissions allow compatibility patches and the inclusion of keywords and other record edits for compatibility; this add-on ships none of their records, scripts or assets and only reads them while the game runs. Please endorse and support the original mods:

- https://www.nexusmods.com/skyrimspecialedition/mods/18994
- https://www.nexusmods.com/skyrimspecialedition/mods/19002
- https://www.nexusmods.com/skyrimspecialedition/mods/28608

SKSE Menu Framework by Thiago099 (the menu header in `include/`). Built on CommonLibSSE-NG.

## Building

`ng.cmake` plugs into the shared CommonLibSSE-NG build tree (`ng_plugin`). Sources are in `src/`.

## License

MIT; see `LICENSE`.
