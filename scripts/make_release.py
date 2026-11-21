"""Assembles the Crafting - AutoPatch release: one FOMOD installer zip (new and older DLL), the page texts and a source zip, all in one folder.

Usage: python scripts/make_release.py <version> [out dir]
The new DLL comes from the shared ng-build tree, the older DLL from legacy/build (run legacy/build.cmd first). Nothing is uploaded.
"""
import hashlib
import os
import shutil
import subprocess
import sys
import xml.dom.minidom
import zipfile

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VERSION = sys.argv[1] if len(sys.argv) > 1 else "0.2.0"
OUT = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, "..", "Claude outputs", f"CraftingAutoPatch-{VERSION}")
OUT = os.path.abspath(OUT)
NAME = "Crafting AutoPatch"
NEW_VER = "1.6.1170.0"
DLLS = {"new": os.path.join(HERE, "..", "ng-build", "build", "release", "out", "CraftingAutoPatch", "CraftingAutoPatch.dll"),
        "older": os.path.join(HERE, "legacy", "build", "CraftingAutoPatchLegacy.dll")}
for _k, _d in DLLS.items():
    assert os.path.isfile(_d), f"missing {_k} DLL: {_d}"

STAGE = os.path.join(OUT, "_stage")
if os.path.exists(OUT):
    shutil.rmtree(OUT)
os.makedirs(os.path.join(STAGE, "fomod"))


def read(p):
    return open(os.path.join(HERE, p), encoding="utf-8").read()


def write(p, text):
    os.makedirs(os.path.dirname(p), exist_ok=True)
    open(p, "w", encoding="utf-8", newline="\n").write(text)


# ---- files installed beside the DLL: licence and third-party notices (rule: they travel with the DLL)
plugins = os.path.join(STAGE, "common", "SKSE", "Plugins")
write(os.path.join(plugins, "CraftingAutoPatch-LICENSE.txt"), read("LICENSE"))
write(os.path.join(plugins, "CraftingAutoPatch-THIRD-PARTY.txt"), read("NOTICE.txt") + "\n\n" + "=" * 78 + "\nnlohmann/json license\n\n" + read(os.path.join("include", "nlohmann", "LICENSE.MIT")))
for k, d in DLLS.items():
    t = os.path.join(STAGE, k, "SKSE", "Plugins")
    os.makedirs(t)
    shutil.copyfile(d, os.path.join(t, "CraftingAutoPatch.dll"))

readme = ("Crafting AutoPatch installer: it asks which CraftingAutoPatch.dll (new: Skyrim 1.6.1170 and newer; older: Skyrim VR or 1.6.1130 and older; "
          "both work on 1.6.1170).\n\n" + read("README.md"))
write(os.path.join(STAGE, "README.txt"), readme)

config = f"""<config xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="http://qconsulting.ca/fo3/ModConfig5.0.xsd">
  <moduleName>{NAME}</moduleName>
  <requiredInstallFiles>
    <folder source="common" destination=""/>
    <file source="README.txt" destination="{NAME} README.txt"/>
  </requiredInstallFiles>
  <installSteps order="Explicit">
    <installStep name="Game version">
      <optionalFileGroups order="Explicit">
        <group name="Which CraftingAutoPatch.dll do you want?" type="SelectExactlyOne">
          <plugins order="Explicit">
            <plugin name="Skyrim 1.6.1170 and newer (SE / AE)">
              <description>The new build. Works on Skyrim SE/AE 1.6.1170 and on every later version, including 1.7.x. Pre-selected when the installer sees a game version of 1.6.1170 or newer.</description>
              <conditionFlags><flag name="dll">new</flag></conditionFlags>
              <typeDescriptor><dependencyType><defaultType name="Optional"/><patterns>
                <pattern><dependencies><gameDependency version="{NEW_VER}"/></dependencies><type name="Recommended"/></pattern>
              </patterns></dependencyType></typeDescriptor>
            </plugin>
            <plugin name="Skyrim VR, or 1.6.1130 and older">
              <description>The older build, for Skyrim VR and for SE/AE 1.6.1130 and older. Pre-selected when the installer sees an older game version or Skyrim VR. On 1.6.1170 either build works. On Skyrim VR the breakdown recipes are not available.</description>
              <conditionFlags><flag name="dll">older</flag></conditionFlags>
              <typeDescriptor><dependencyType><defaultType name="Recommended"/><patterns>
                <pattern><dependencies><gameDependency version="{NEW_VER}"/></dependencies><type name="Optional"/></pattern>
              </patterns></dependencyType></typeDescriptor>
            </plugin>
          </plugins>
        </group>
      </optionalFileGroups>
    </installStep>
  </installSteps>
  <conditionalFileInstalls>
    <patterns>
      <pattern>
        <dependencies><flagDependency flag="dll" value="new"/></dependencies>
        <files><folder source="new" destination=""/></files>
      </pattern>
      <pattern>
        <dependencies><flagDependency flag="dll" value="older"/></dependencies>
        <files><folder source="older" destination=""/></files>
      </pattern>
    </patterns>
  </conditionalFileInstalls>
</config>
"""
info = (f"<fomod>\n  <Name>{NAME}</Name>\n  <Author>CageTV</Author>\n  <Version>{VERSION}</Version>\n"
        "  <Website>https://github.com/CageTV/Crafting_AutoPatch</Website>\n"
        "  <Description>Gives other mods' weapons and armor the keywords and the breakdown recipes of Weapons Armor Clothing and Clutter Fixes and Complete Crafting Overhaul at game start, with no patch plugins.</Description>\n</fomod>\n")
xml.dom.minidom.parseString(config)
xml.dom.minidom.parseString(info)
write(os.path.join(STAGE, "fomod", "ModuleConfig.xml"), config)
write(os.path.join(STAGE, "fomod", "info.xml"), info)

# ---- the installer zip
zpath = os.path.join(OUT, f"{NAME} {VERSION}.zip")
with zipfile.ZipFile(zpath, "w", zipfile.ZIP_DEFLATED) as z:
    for dp, _, fs in os.walk(STAGE):
        for fn in sorted(fs):
            p = os.path.join(dp, fn)
            z.write(p, os.path.relpath(p, STAGE).replace("\\", "/"))
shutil.rmtree(STAGE)

# ---- the source: the tracked files of HEAD, its own download
spath = os.path.join(OUT, f"{NAME} {VERSION} - Source.zip")
subprocess.run(["git", "-C", HERE, "archive", "--format=zip", f"--prefix=Crafting_AutoPatch-{VERSION}/", "-o", spath, "HEAD"], check=True)

# ---- the page texts (changes only; no roadmap)
for fn in ("CHANGELOG.txt", "Nexus-description.txt", "Nexus-summary.txt"):
    src = os.path.join(HERE, "release", fn)
    assert os.path.isfile(src), f"missing release text: {src}"
    shutil.copyfile(src, os.path.join(OUT, fn))

# ---- hashes of what is handed over
lines = []
for fn in sorted(os.listdir(OUT)):
    p = os.path.join(OUT, fn)
    if os.path.isfile(p):
        lines.append(f"{hashlib.sha256(open(p, 'rb').read()).hexdigest()}  {os.path.getsize(p):>9}  {fn}")
write(os.path.join(OUT, "SHA256SUMS.txt"), "\n".join(lines) + "\n")
with zipfile.ZipFile(zpath) as z:
    print(f"{zpath}: {len(z.namelist())} entries")
    for n in sorted(z.namelist()):
        print("  ", n)
print("\n".join(lines))
