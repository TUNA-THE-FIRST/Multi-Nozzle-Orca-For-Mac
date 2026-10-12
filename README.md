# Snapmaker Orca Mac

Snapmaker Orca Mac is a macOS-only build of [Snapmaker Orca](https://github.com/Snapmaker/OrcaSlicer), based on
LixNix's 2.5 line with per-extruder layer heights for printers whose tool heads carry different nozzle sizes
(like the Snapmaker U1). It targets Apple Silicon Macs and is built, tested and packaged for macOS only.

It is not made or supported by Snapmaker. Report problems with this build in this repository's
[issues](../../issues), not to Snapmaker support.

## Download

Every change to `main` is built on GitHub's Apple Silicon Macs:

1. Open the [Actions tab](../../actions/workflows/build_macos.yml) and click the newest green run.
2. Under **Artifacts**, download `Snapmaker_Orca_Mac_arm64_...`. GitHub hands it over zipped; double-click the zip to get the DMG.
3. Open the DMG and drag **Snapmaker Orca Mac** into **Applications**.

### Opening it the first time

The app is not signed with an Apple Developer ID, so macOS stops it the first time with
"Apple could not verify Snapmaker Orca Mac is free of malware". Either:

- open **System Settings → Privacy & Security**, scroll down to the message about Snapmaker Orca Mac and click
  **Open Anyway**; or
- run this once in Terminal:
  ```console
  xattr -dr com.apple.quarantine "/Applications/Snapmaker Orca Mac.app"
  ```

### Next to official Snapmaker Orca

Snapmaker Orca Mac has its own app identifier and its own settings folder,
`~/Library/Application Support/Snapmaker_Orca_Mac`, so it installs next to official Snapmaker Orca and the two never
overwrite each other's presets. On its first launch it offers to copy your printers, filaments, process presets and
preferences from Snapmaker Orca; official Snapmaker Orca is left as it is.

### When something goes wrong

Snapmaker Orca Mac sends no crash reports or usage logs anywhere. To report a problem, attach:

- the steps that lead to it, and a screenshot;
- for a crash, the report macOS saves: open the **Console** app → **Crash Reports** → the newest
  `Snapmaker_Orca` entry (the same files are in `~/Library/Logs/DiagnosticReports`);
- the app's own log: the newest file in `~/Library/Application Support/Snapmaker_Orca_Mac/log`.

[docs/mac/TESTING.md](docs/mac/TESTING.md) is the checklist used to test each build.

## Building from source

You need an Apple Silicon Mac with Xcode, CMake and Homebrew:

```console
brew install cmake gettext libtool automake autoconf texinfo pkgconf yasm nasm
./build_release_macos.sh -d -x        # dependencies, once (an hour or more)
./build_release_macos.sh -s -x        # the app: build/arm64/Snapmaker_Orca/Snapmaker Orca Mac.app
./build_release_macos.sh -s -x -T     # the app and the unit tests, which then run
./scripts/package_macos.sh "build/arm64/Snapmaker_Orca/Snapmaker Orca Mac.app" Snapmaker_Orca_Mac.dmg
```

To debug in Xcode, configure with the default generator instead (`./build_release_macos.sh -s`) and open
`build/arm64/Snapmaker_Orca.xcodeproj`; pick the `Snapmaker_Orca` scheme and set its Run configuration to
`RelWithDebInfo`.

## Klipper printers

If you're running Klipper, it's recommended to add the following configuration to your `printer.cfg` file.
```
# Enable object exclusion
[exclude_object]

# Enable arcs support
[gcode_arcs]
resolution: 0.1
```


## Some background
Snapmaker Orca is originally forked from Snapmaker_Orca.

Snapmaker_Orca is originally forked from Bambu Studio, it was previously known as BambuStudio-SoftFever.
Bambu Studio is forked from [PrusaSlicer](https://github.com/prusa3d/PrusaSlicer) by Prusa Research, which is from [Slic3r](https://github.com/Slic3r/Slic3r) by Alessandro Ranellucci and the RepRap community. 
Orca Slicer incorporates a lot of features from SuperSlicer by @supermerill
Orca Slicer's logo is designed by community member Justin Levine(@freejstnalxndr)  


# License
Snapmaker Orca is licensed under the GNU Affero General Public License, version 3. Snapmaker_Orca is based on Orca Slicer by SoftFever

Orca Slicer is licensed under the GNU Affero General Public License, version 3. Orca Slicer is based on Bambu Studio by BambuLab.

Bambu Studio is licensed under the GNU Affero General Public License, version 3. Bambu Studio is based on PrusaSlicer by PrusaResearch.

PrusaSlicer is licensed under the GNU Affero General Public License, version 3. PrusaSlicer is owned by Prusa Research. PrusaSlicer is originally based on Slic3r by Alessandro Ranellucci.

Slic3r is licensed under the GNU Affero General Public License, version 3. Slic3r was created by Alessandro Ranellucci with the help of many other contributors.

The GNU Affero General Public License, version 3 ensures that if you use any part of this software in any way (even behind a web server), your software must be released under the same license.

Orca Slicer includes a pressure advance calibration pattern test adapted from Andrew Ellis' generator, which is licensed under GNU General Public License, version 3. Ellis' generator is itself adapted from a generator developed by Sineos for Marlin, which is licensed under GNU General Public License, version 3.

The Bambu networking plugin is based on non-free libraries from BambuLab. It is optional to the Orca Slicer and provides extended functionalities for Bambulab printer users.

