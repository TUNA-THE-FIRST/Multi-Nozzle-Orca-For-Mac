# Testing a Snapmaker Orca Mac build

Each build is checked three ways.

**Automatically, on every pull request and every push to `main`** ([Build macOS](../../.github/workflows/build_macos.yml)), on
GitHub's Apple Silicon Macs (macOS 26, Xcode 26):

- the app and the profile validator build;
- the unit tests run (`tests/`, the multi-nozzle layer height and per-head process suites included);
- every system profile loads, and a two-colour cube is sliced through every shipped printer, expanding every custom
  G-code template;
- the app is signed and packed into the DMG the run offers for download.

**On the engine alone**, the slicing tests can run on any machine that builds `libslic3r`, which is the quick loop for
changes to slicing.

**By hand on a Mac**, with the checklist below, because window, menu, dark mode and printer behaviour only show in the
running app. Go through it for each DMG that changes the app; a build that only touches slicing needs sections 3 and 4.

## Checklist

Note the macOS version and Mac model once per round (Apple menu → About This Mac).

### 1. Install and first launch

- [ ] The DMG opens and the app copies to Applications.
- [ ] The first launch is blocked by Gatekeeper with "Open Anyway" offered in System Settings → Privacy & Security
      (not "damaged and can't be opened"), and opens after that.
- [ ] With official Snapmaker Orca's settings on this Mac, the first launch asks whether to copy them. After
      **Copy my settings** your printers and presets are there; official Snapmaker Orca still opens with its own.
- [ ] The app menu reads "Snapmaker Orca Mac"; About, Settings (⌘,) and Quit (⌘Q) work.
- [ ] The setup wizard lists the Snapmaker printers first and finishes on filament selection.
- [ ] Quit and reopen: the app opens on Home and remembers the printer.

### 2. Files

- [ ] Import an STL, a 3MF project, a STEP file (File → Import, and drag & drop onto the plate).
- [ ] Open a 3MF whose file name has accents (`peça teste.3mf`) by double-clicking it in Finder, once with the app
      closed and once with it already open.
- [ ] Save the project, close it, reopen it from File → Recent Projects.
- [ ] Export G-code and "Show in Finder" after exporting.

### 3. Slicing and preview

- [ ] Slice a single-colour model on the U1 profile; the preview shows layers, the legend, and the estimated time.
- [ ] Paint or assign a second filament; slice; the preview shows both and the prime tower.
- [ ] Per-extruder layer heights: give two tool heads different nozzle sizes and preferred layer heights, slice one
      object that uses both, and check the layer slider and the G-code show the coarser head printing every Nth layer.
- [ ] Arrange, auto-orient, cut, and the variable layer height tool do what they say.

### 4. Printer

- [ ] Add the U1 by its IP address; the Device tab shows its state and the camera.
- [ ] Send a sliced plate to the printer (upload only is enough); it arrives with its thumbnail.
- [ ] Unplug the network cable or turn the printer off while connected: the app says so and does not freeze or crash.

### 5. Looks and windows

- [ ] Switch macOS between Light and Dark (System Settings → Appearance) while the app is open: every tab, the
      sidebar, dialogs and the 3D view follow.
- [ ] Full screen (⌃⌘F) and back; minimize and restore; the title bar text stays readable.
- [ ] With an external monitor: move the window between screens; icons and text stay sharp on both.
- [ ] Trackpad: pinch to zoom, two-finger drag to rotate, Shift + two-finger drag to pan; mouse wheel zoom.
- [ ] Keyboard: ⌘Z/⌘⇧Z, ⌘C/⌘V on objects, Delete removes an object, typing in a settings field does not
      trigger shortcuts.

## Reporting a problem

Write down the checklist item, the steps to reproduce it and what you expected, and attach:

- a screenshot or screen recording (⌘⇧5);
- for a crash, the report from the **Console** app → **Crash Reports** → newest `Snapmaker_Orca` entry
  (also in `~/Library/Logs/DiagnosticReports`);
- the newest log in `~/Library/Application Support/Snapmaker_Orca_Mac/log`;
- for a slicing problem, the 3MF project (File → Save Project As).
