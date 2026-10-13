# Printing one object with several nozzles

A printer with several tool heads, like the Snapmaker U1, can print **one object with nozzles of different
sizes and layers of different heights**. The usual reason is speed without losing looks: a fine nozzle prints
the outside of the object in thin layers, so its surface stays smooth, and a large nozzle prints the inside in
thick layers, which takes a fraction of the time.

Snapmaker Orca Mac opens this guide from **Help > Multi-nozzle Printing Guide**.

## The idea in one picture

The object is cut into thin **object layers** (for example 0.12 mm). Every nozzle prints on top of those:

| Nozzle | Nozzle size | Its layer height | Prints on | Prints |
|---|---|---|---|---|
| Nozzle 1 | 0.4 mm | 0.12 mm | every layer | outer walls, top and bottom surfaces |
| Nozzle 2 | 0.8 mm | 0.36 mm | every 3rd layer | inner walls and infill |

Nozzle 2 waits for three object layers to be printed around it, then fills them with one thick 0.36 mm layer.
Its lines are three times as tall and twice as wide, so it moves far less to fill the same volume.

**Each nozzle's layer height must be a whole multiple of the object layer height** (0.12, 0.24, 0.36... for
0.12 mm object layers). When the heights you choose share no such grid, the app picks a finer object layer
height that fits all of them; the sidebar tells you when that happens.

## Before you start

1. **Install the nozzles** on the printer: the sizes you choose in the app must be the nozzles really on the
   tool heads. A wrong size makes every line of that nozzle too thick or too thin.
2. **Load a filament on each tool head you use.** For a plain multi-nozzle print, load the same material (for
   example the same PLA) on both. On the U1, filament 1 is on nozzle 1, filament 2 on nozzle 2, and so on.
3. **Pick the printer preset of your fine nozzle** (for example *Snapmaker U1 (0.4 nozzle)*) and a process
   preset with the object layer height you want (for example *0.12mm Standard*).

## Quick setup

Click **Set up multi-nozzle printing...** under the nozzle tabs in the sidebar (or **Help > Set Up Multi-nozzle
Printing...**), then:

1. Choose a **recipe**:
   - **Fine outside, fast inside** - outer walls and top and bottom surfaces on the fine nozzle; inner walls and
     all infill on the coarse one. The best balance of looks and speed.
   - **Fine walls, fast infill** - every wall and surface on the fine nozzle; solid and sparse infill on the coarse
     one. Stronger walls, still much faster.
   - **Fast sparse infill** - only the sparse infill on the coarse nozzle. The safest start: the outside is untouched.
   - **Off** - everything back to the object's own filament, one layer height.
2. Check the **nozzle sizes** (they must match the printer) and the **layer height** of each nozzle. The list
   offers the heights that fit the object layer height.
3. Read **What will happen**: it says the object layer height, which nozzle prints what, how often, and warns
   about heights a nozzle cannot print well.
4. Click **Apply**, then slice.

The recipes treat the first nozzle as the fine one and the largest other nozzle as the coarse one, and give **both**
a layer height of their own (see *Not set* below for why that matters).

## Setting it up by hand

Everything the dialog does can be set directly:

| What | Where |
|---|---|
| Nozzle size of each tool head | Sidebar, printer section, nozzle tabs: **Diameter** |
| Layer height of each nozzle | Sidebar nozzle tabs: **Preferred layer height** (*Default* = not set, see below) |
| Which nozzle prints which feature | Process settings > **Multimaterial** > **Filament for Features**: outer walls, inner walls, sparse infill, internal solid infill, top surface, bottom surface |
| A whole part on another nozzle | Object list: the part's **filament** column |
| Supports | Process settings > **Support** > **Filament for Supports**, and **Support nozzle diameter** |

A feature set to **Default** prints with the filament of the object or part it belongs to.

Under the nozzle tabs, **What each nozzle prints on this plate** sums it up: each nozzle's size, and for each layer
height it prints, how often and what. It is worked out by the same rules the slicer uses, so it shows what will
really be printed. Orange notes explain anything that may go wrong: a layer too thick for its nozzle, a layer height
a nozzle cannot print, or a layer height set on a nozzle that prints nothing.

## Checking the result

After slicing, in the preview:

- **Layer Height** (view drop-down at the top of the legend) colours each line by its height: the coarse
  nozzle's lines show the thick height, the fine nozzle's the thin one.
- **Tool** colours each line by the nozzle that prints it.
- Move the **layer slider**: the coarse nozzle's features appear only on every 2nd, 3rd... layer.

## How the heights are worked out

- A nozzle left at **Default** (*Not set* in the setup dialog) has no layer height of its own: **it follows the
  nozzle it shares a part with**. Each part prints with one layer pitch set by its walls, so if the coarse nozzle
  prints the inner walls at 0.36 mm and the fine nozzle is left at Default, the fine nozzle's outer walls print
  0.36 mm layers too - too thick for a 0.4 mm nozzle. Give the fine nozzle the object layer height (for example
  0.12 mm) to keep it fine; the recipes always do.
- A preferred height that is a whole multiple of the object layer height prints exactly as entered.
- Otherwise the app looks for the coarsest object layer height on which every preferred height lands within
  0.01 mm of a whole multiple, and prints the heights rounded to it. The sidebar shows the height each nozzle
  then prints, in orange when it differs from what you entered.
- **Exact preferred layer heights** (printer settings, or *Print exact heights* in the sidebar) instead keeps every
  height as entered, which can make the object layer height very fine (0.13 and 0.37 mm share only 0.01 mm) and
  slicing slow.
- Where the geometry does not allow a coarse layer (a thin rib, the first layers, the top of a slope, a band of
  solid infill only a few layers thick), that part of the feature prints thinner layers, never thicker ones.
- Only the walls, top surfaces, solid infill and sparse infill can print thicker on their own, and only in whole
  multiples of the part's pitch. With 0.2 mm walls, a 0.3 mm sparse infill cannot be printed: it prints 0.2 mm
  layers (the summary says so). Choose heights that are whole multiples of each other.

## Limits

- A layer should be at most about **3/4 of its nozzle size** (0.6 mm for a 0.8 mm nozzle) and at least the
  nozzle's minimum layer height.
- Per-nozzle layer heights do not work together with **spiral vase**, **interface shells**, **interlocking beams**,
  **variable layer height** or **height range modifiers that change the layer height**. Slicing says so and how
  to fix it.
- With the **prime tower** on, every tool change purges on the tower; the coarse nozzle changes less often, so
  the extra time is small.
- Outer and inner walls on nozzles with different heights print together at the lower height, unless one
  height is a whole multiple of the other (then they split cleanly). **Adjust wall layer height**
  (Multimaterial > Filament for Features) can round one of them so they split.

## When something is off

| You see | Why | What to do |
|---|---|---|
| The coarse nozzle prints every layer | Nothing it prints can combine, or its height is Default | Give it a preferred layer height, and assign it a feature in Filament for Features |
| The fine nozzle prints thick layers (orange note: *thicker than its nozzle prints well*) | Its layer height is Default, so it follows the coarse nozzle in the parts they share | Set its layer height to the object layer height, or use a recipe |
| *... is set to X mm layers but prints Y mm layers* | Its height is not a whole multiple of the part's wall pitch | Choose heights that are whole multiples of each other |
| *... is not on this plate's layer grid* (orange, sidebar) | Your heights share no common grid at the object layer height | Pick heights from the list, or click *Print exact heights* |
| *The layer height of extruder N cannot exceed its nozzle diameter* | The height is thicker than the nozzle | Choose a lower height or a larger nozzle |
| *Some object parts print ... mm layers with filament ... whose maximum layer height is ... mm* | That filament's nozzle prints thicker layers than its profile allows | Lower the height, or set the nozzle size in the sidebar so its profile's limits apply |
| Slicing takes very long | The object layer height became very fine | Choose heights that are whole multiples of each other (0.12 and 0.36, 0.2 and 0.4) |
