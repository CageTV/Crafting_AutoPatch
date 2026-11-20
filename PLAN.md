# Crafting AutoPatch: map-out (2026-11-20)

Working title. Next auto patcher after Gourmet AutoPatch and CACO AutoPatch. Targets the kryptopyr "WAF" family:

| Mod | Nexus | Installed | Plugin |
|---|---|---|---|
| Weapons Armor Clothing and Clutter Fixes (WACCF) | 18994 | 2.11.1 (2.11.4 out) | `weapons armor clothing & clutter fixes.esp` |
| Armor and Clothing Extension (ACE) | 19002 | 1.6.3 | `waccf_armor and clothing extension.esp` |
| Complete Crafting Overhaul Remastered (CCOR) | 28608 | 2.6.4 | `complete crafting overhaul_remastered.esp` |

Related and installed, not asked for: Smithing Perks Overhaul SE (CCOR's load-order notes name it). Live instance: `C:\Modlists\Tabula`, profile `tabula rasa!`.

## 1. Permissions (what we may do)

- **CCOR** (README, quoted): no redistribution, no assets without their creators' permission, no collections, nothing sold. **"You are allowed to create compatibility patches for this mod."** "Any keywords, global variables, conditions, or other functions that are needed for compatibility with CCOR may be included in other mods"; credit and a mention of the compatibility required in the description.
- **ACE** (README, quoted): same, but a patch is allowed **"as long as the patch requires that the user download the main file from this page in order to work."**
- **WACCF**: no README ships with it. **Not verified.** Same author and the same template, so probably the same, but the user should quote the Nexus permissions box before release (as was done for CACO).
- So: a separate DLL that ships none of their records or assets and only reads them at runtime. Credit kryptopyr and link the three pages. Same model as Gourmet and CACO.

## 2. What each mod does (from the plugins and READMEs)

- **WACCF** (3,267 records; WEAP 1,019, ARMO 751, LVLI 580, KYWD 181, COBJ 76, PERK 26). Fixes vanilla weapons, armor, clothing and clutter, and **defines the keyword system** everyone else builds on. 180 of its keywords are injected into `Update.esm` (IDs `AF0xxx:Update.esm`), like the Last Seed and CACO keywords.
- **ACE** (827 records; OTFT 160, ARMO 159, ARMA 128, TXST 99, LVLI 68, COBJ 43, NPC_ 10, QUST 9). More varied NPC robes and outfits, jarls and appointees change clothes with the Civil War, new mage boots and hoods, optional armor value on clothing (uses `WAF_ProtectiveClothingCCF_NN` keywords). Needs Rustic Clothing.
- **CCOR** (5,244 records; COBJ 2,356, ARMA 618, ACTI 583, GLOB 404, ARMO 352, MISC 278, PERK 27). SkyUI crafting categories, reworked and added recipes, jewelry and circlets, **breakdown recipes** (item to ingots or leather), temper and upgrade recipes, craft-your-own faction gear and artifact replicas, MCM toggles. Master list includes WACCF. 7 of its keywords are also injected into `Update.esm`.

## 3. The integration surface (what a third-party item needs)

Read from the three plugins:

- **Smithing and temper perks accept an item by its vanilla material keyword OR by `WAF_TreatAsMaterial<Iron|Steel|Dwarven|Elven|Orcish|Glass|Ebony|Daedric|Dragon>`** (12 refs each in WACCF perks). `WAF_TemperBonus<Steel|Dwarven|Elven|Orcish|Glass|Ebony|Daedric|Dragon|Advanced>Perk` carries the temper bonus. `WAF_IgnoreVanillaMaterialKeywordPerks` opts an item out of the vanilla keyword route.
- **Armor and clothing classes:** `WAF_ArmorMaterial*`, `WAF_Material*` (Silver, Nordic, Faction, Bonemold, Chitin), `WAF_ArmorPerkSilver/Gold`, `WAF_JewelryExquisite`, `WAF_FingerlessGauntletsBracers`, `WAF_ClothingCloak/Pouch/Accessories`, `WAF_VisuallyDarkArmor`, and **30 `WAF_ProtectiveClothingCCF_NN`** keywords that the ACE clothing-armor feature runs on.
- **Crafting:** `CCO_MenuCategory` (menu rows), `CraftingDisabledRecipe_CCO/_WACCF` (hides a recipe), `NoCraftingExperience` (no smithing XP). The CCOR breakdown recipe (verified in `BreakdownArmorSteelCuirassA_CCO`): a COBJ at the smelter bench whose ingredient is the item itself, whose product is the material (for example 3 steel ingots), gated by MCM globals and the PRUFEI favorite/equipped protection conditions.
- Because the keywords live in `Update.esm`, the DLL can look them up by EditorID or by `Update.esm` FormID the way Gourmet does.

## 4. Census of the C list (294 enabled plugins; scripts in `tools/`)

- 9,239 new ARMO and 2,474 new WEAP across the list, but most armor is cosmetic (BSAssets.esm alone adds 5,801).
- **Craftable third-party weapons and armor (created by some COBJ): 1,338 in 19 plugins**, none vanilla, none WACCF/CCOR/ACE:
  Legacy of the Dragonborn 536 (museum replicas), BS Heartland 105, BS Assets 91, Campfire 79 (67 already carry `WAF_ClothingPouch`), Hunterborn 50, Rigmor 18, Hunting in Skyrim 18, Rigmor Cyrodiil 12, NewBosmerHouse 12, CACO 11, a few CC and small mods.
- Almost none carry a WAF or material keyword (BS Heartland 32 of 105 carry a material keyword; LOTD 0 of 536). Many show an empty keyword list in the raw record (probably templated items); the runtime has to read the effective keywords.
- **Reverse direction is clean:** of the 2,281 weapon and armor records WACCF, ACE and CCOR touch, only **8** are overridden later by a non-family plugin (BeardMaskFix 5, SurvivalModeImproved 3), and those 8 lose their WAF keywords. No list-wide "restore WACCF" problem.
- No CCOR, WACCF or ACE patch plugins exist in the C list. SPO and CCOR are both present.

## 5. Candidate features, by tier

**Tier 1: classify and tag third-party gear (recommended core).**
- At `kDataLoaded`, for every ARMO and WEAP that has no WAF material or class keyword, work out the material and class and add the matching `WAF_*` keywords, so the smithing and temper perks, silver and gold armor perks, protective-clothing armor and menu rows treat it like vanilla gear.
- Classification, strongest signal first: the item's own recipe ingredients (a COBJ that consumes steel ingots means steel) and its temper recipe ingredient; keywords it already has (vanilla material, Heavy/Light, Clothing); template item; name tokens ("Dwarven", "Leather", "Robes") as the fallback. Reuse the Gourmet and CACO Classifier layout: rule table, config, `report.json`, "Skipped" list.
- Per-mod and per-item override rules in `config.json`, exclusions, a dry-run report first (like CACO 0.1.0).

**Tier 2: ACE outfit and NPC re-assertion (small).**
- ACE changes the default outfits of 10 NPCs. If another plugin later overrides one of those NPCs' outfit, re-apply ACE's. The census found few conflicts, so this may need only a check and a report, not code. Needs a per-NPC look at the real winners.

**Tier 3: recipes for third-party items (the open research question).**
- CCOR gives vanilla items breakdown, temper and upgrade recipes. Third-party gear gets none. Doing this at runtime means creating `TESConstructibleObject` forms with condition lists in the DLL. **Gourmet's notes say runtime-created perks and books are unsafe (dynamic FormIDs); runtime COBJs may be safe because nothing in a save refers to them, but that is unproven.** Needs a spike: create one COBJ with conditions at `kDataLoaded`, check the crafting menu and a save and reload.
- Alternative: generate a small patch plugin from a Mutagen tool (like the Wildlife Placer or Groundskeeper apps). Safer and ESL-able, but it is a different shape from Gourmet and CACO.

**Tier 4: crafting-menu categories and `NoCraftingExperience`** for third-party recipes, if the SkyUI categories are driven by keywords on the created item (to be confirmed from the CCOR BSA / SkyUI category config).

## 6. Architecture (reuse from Gourmet and CACO)

- New repo `E:\WorkSpace\Crafting-AutoPatch` (local git; GitHub and release only after the user's go-ahead). DLL `CraftingAutoPatch`, nlohmann-json, ng build (`E:\WorkSpace\ng-build`) plus the legacy build; SKSE Menu Framework page (Overview, Items, Skipped); `config.json` and `report.json` under `Data/SKSE/Plugins/CraftingAutoPatch/`; FOMOD installer with the new and older builds.
- Phase 1 = dry run (report only, changes nothing), as CACO 0.1.0 did. Phase 2 = apply Tier 1. Then decide Tier 2 and Tier 3.
- Keywords are added as plain pointer changes at `kDataLoaded`. Lessons carried over: lookup by `LookupModByName` (covers ESL), leveled-list style arrays have a size header, check config before launch, bisect with config switches if another plugin crashes after our edits.

## 7. Risks and unknowns

- Templated items: raw KWDA is often empty, so the runtime must read what the engine reports.
- Replica and museum items (LOTD, 536 of the 1,338) should probably be skipped or left alone by default: they are display or replica items, not new gear.
- Items other mods already tag (Campfire backpacks) must be left alone: only add, never replace.
- SPO overlap: SPO may add its own keyword or perk routes; check before tagging.
- CCOR MCM toggles are globals: the patcher should respect "CCOR off" modes, and not tag anything if WACCF is missing.
- 2.11.4 of WACCF exists; installed is 2.11.1. Check the changelog for keyword changes before coding.

## 8. Open decisions for the user

1. Scope: Tier 1 only, Tier 1 plus Tier 2, or include a Tier 3 spike?
2. Should Smithing Perks Overhaul SE be in the first release?
3. WACCF permissions: please quote the Nexus box.
4. Name: "Crafting AutoPatch" or "WAF AutoPatch" or other.
5. Default for Legacy of the Dragonborn replicas: skip, or classify too?

## 9. Next steps

1. Finish the Tier 1 design: material and class rule table from the WACCF perks (done in part: section 3).
2. Dry-run DLL that writes `report.json` for the C list (what it would tag and why).
3. Spike for Tier 3 once the user decides.
