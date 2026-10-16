#ifndef slic3r_ToolHeadUse_hpp_
#define slic3r_ToolHeadUse_hpp_

#include <string>
#include <vector>

namespace Slic3r {

class DynamicPrintConfig;
class Model;
class Print;

// Snapmaker Orca Mac: what each tool head of a multi-nozzle printer prints on a plate and at which
// layer height, decided by the same PrintObject functions the slicing engine combines layers with
// (region_layer_height_multiplier(), wall_split_pitches(), ...), so the sidebar summary and the
// multi-nozzle setup dialog never describe a print other than the one that is sliced.

// One feature a tool head prints, at one layer height.
struct ToolHeadFeatureUse
{
    std::string  feature;     // untranslated, see tool_head_features(); "Supports" for support material
    double       height = 0.; // the layer height it prints with, mm; 0 for supports (they follow the support layers)
    unsigned int every  = 1;  // prints on every `every`-th object layer
    // Sparse infill combined up to Infill combination's maximum layer height (the engine's own rule,
    // capped by the nozzles), not to a preferred layer height.
    bool         combined = false;
};

struct ToolHeadUse
{
    double nozzle    = 0.; // nozzle diameter, mm
    double preferred = 0.; // the preferred layer height planned for the tool head, mm; 0 = Default
    // In the order of tool_head_features(), then supports; a feature printed at several heights (by
    // several objects or parts) is listed once per height and object layer height. Empty: the tool
    // head prints nothing.
    std::vector<ToolHeadFeatureUse> features;
};

struct ToolHeadUses
{
    double                   grid = 0.; // the object layer height (of the first object)
    std::vector<ToolHeadUse> heads;     // one per tool head
};

// `print` after Print::apply(): the objects, their regions and the config are read, nothing is
// sliced. `head_nozzles`: the nozzle diameter of each tool head (the printer's nozzle_diameter).
ToolHeadUses tool_head_uses(const Print &print, const std::vector<double> &head_nozzles);

// The same for the objects of `model` printed with `config`, a full print config whose preferred
// layer heights are planned here (as PresetBundle::full_config_for_print() plans them).
ToolHeadUses tool_head_uses(const Model &model, DynamicPrintConfig config);

// The tool head (0-based) that prints filament `filament` (1-based): the tool of the same number; the
// first tool head for a filament beyond them, as the engine reads per-tool settings for it.
size_t tool_head_of_filament(const DynamicPrintConfig &config, int filament);

// The feature filament keys of the features tool_head_uses() reports, with their untranslated
// names, in its order.
struct ToolHeadFeature
{
    const char *key;
    const char *name;
};
const std::vector<ToolHeadFeature> &tool_head_features();

} // namespace Slic3r

#endif // slic3r_ToolHeadUse_hpp_
