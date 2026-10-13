#ifndef slic3r_GUI_MultiNozzle_hpp_
#define slic3r_GUI_MultiNozzle_hpp_

// Snapmaker Orca Mac: printing one object with several nozzle sizes and layer heights, made
// understandable: the texts of the sidebar summary, the "Set up multi-nozzle printing" dialog and
// the guide. What each tool head prints is decided in libslic3r/ToolHeadUse.hpp, by the functions
// the slicing engine itself uses.

#include <cstddef>
#include <vector>

#include <wx/string.h>

namespace Slic3r {

struct ToolHeadUse;
struct ToolHeadUses;

namespace GUI {

// A length in mm as the sidebar writes it ("0.36"), without the unit.
wxString mm_number(double value);
// Tool head `head` (0-based) named as its sidebar nozzle tab names it.
wxString nozzle_display_name(size_t head, size_t count);
// What a tool head prints, one line per layer height:
// "0.36 mm layers, every 3rd layer: inner walls, sparse infill"; supports last, without a height.
std::vector<wxString> tool_head_print_lines(const ToolHeadUse &use, double grid);
// What may go wrong with these uses, explained with what to do: a layer thicker than a nozzle
// prints well or below the minimum it prints reliably, a preferred layer height a tool head does
// not print, a preferred height on a tool head that prints nothing. `min_heights`: the printer's
// min_layer_height per tool head.
std::vector<wxString> tool_head_warnings(const ToolHeadUses &uses, const std::vector<double> &min_heights);
// What each tool head prints on the current plate, read from the plate's print as it was last
// applied for slicing.
ToolHeadUses plate_tool_head_uses();

// Opens the "Set up multi-nozzle printing" dialog for the selected printer.
void open_multi_nozzle_setup();
// Opens the multi-nozzle printing guide in the browser.
void open_multi_nozzle_guide();

} // namespace GUI
} // namespace Slic3r

#endif // slic3r_GUI_MultiNozzle_hpp_
