#include "ToolHeadUse.hpp"

#include "Model.hpp"
#include "Print.hpp"
#include "PrintConfig.hpp"
#include "Slicing.hpp"

#include <algorithm>
#include <cmath>

// Marks the feature names for translation; the GUI translates them.
#define L(s) s

namespace Slic3r {

static const char *const SUPPORTS = L("Supports");

const std::vector<ToolHeadFeature> &tool_head_features()
{
    static const std::vector<ToolHeadFeature> features = {
        {"outer_wall_filament_id",     L("Outer walls")},
        {"inner_wall_filament_id",     L("Inner walls")},
        {"top_surface_filament_id",    L("Top surfaces")},
        {"bottom_surface_filament_id", L("Bottom surfaces")},
        {"internal_solid_filament_id", L("Solid infill")},
        {"sparse_infill_filament_id",  L("Sparse infill")},
    };
    return features;
}

size_t tool_head_of_filament(const DynamicPrintConfig &config, int filament)
{
    const auto  *nozzles = config.option<ConfigOptionFloats>("nozzle_diameter");
    const size_t heads   = nozzles != nullptr && !nozzles->values.empty() ? nozzles->values.size() : 1;
    // The engine prints filament N with tool N (Print::extruder_index_of(), the T<N> of the G-code):
    // on a tool changer like the U1 filament N is loaded on tool head N. Filaments beyond the tool
    // heads wrap around, as there is no head of their own.
    return size_t(std::max(1, filament) - 1) % heads;
}

ToolHeadUses tool_head_uses(const Print &print, const std::vector<double> &head_nozzles)
{
    ToolHeadUses out;
    const size_t heads = std::max<size_t>(1, head_nozzles.size());
    out.heads.resize(heads);
    for (size_t head = 0; head < heads; ++head)
        out.heads[head].nozzle = head < head_nozzles.size() ? head_nozzles[head] : 0.;

    const PrintConfig &config    = print.config();
    const size_t       filaments = config.filament_diameter.values.size();
    // The tool head printing a 0-based filament: the tool of the same number (Print::extruder_index_of(),
    // the T<N> of the G-code). `heads` for a filament without a tool head of its own, and for a
    // Snapmaker mixed filament, a virtual id that prints with a different tool head on every layer.
    auto head_of = [filaments, heads](unsigned int filament) -> size_t {
        return filament < filaments && filament < heads ? size_t(filament) : heads;
    };

    for (const PrintObject *object : print.objects()) {
        const double h = object->config().layer_height.value;
        if (h <= EPSILON)
            continue;
        if (out.grid <= 0.)
            out.grid = h;
        // The preferred layer heights as the engine reads them (planned, per filament).
        for (unsigned int filament = 0; filament < filaments; ++filament)
            if (const size_t head = head_of(filament); head < heads && out.heads[head].preferred <= 0.)
                out.heads[head].preferred = object->extruder_preferred_layer_height(filament + 1);

        auto add = [&](int filament_id, const char *feature, double height) {
            const size_t head = head_of(feature_filament_idx(filament_id));
            if (head >= heads)
                return;
            std::vector<ToolHeadFeatureUse> &features = out.heads[head].features;
            const bool known = std::any_of(features.begin(), features.end(), [feature, height](const ToolHeadFeatureUse &use) {
                return use.feature == feature && std::abs(use.height - height) < EPSILON;
            });
            if (!known)
                features.push_back({feature, height, height > EPSILON ? (unsigned int)std::max(1L, std::lround(height / h)) : 1u});
        };

        // Mirrors the heights Print::validate() derives for its per-extruder layer height checks: the
        // region pitch (region_layer_height_multiplier()), the walls' own pitch or split pitches, and
        // the top surfaces, solid infill and sparse infill combining on their own while the region
        // prints at the object layer height (PrintObject::combine_top_surfaces(),
        // combine_internal_solid_infill(), combine_infill()).
        for (const PrintRegion &region : object->all_regions()) {
            const PrintRegionConfig &rc           = region.config();
            const unsigned int       region_m     = object->region_layer_height_multiplier(region);
            const double             region_pitch = region_m * h;
            const unsigned int       wall_m       = region_m > 1 ? 1 : object->wall_layer_height_multiplier(region);
            unsigned int             fine = 0, coarse = 0;
            bool                     coarse_is_outer = false;
            const bool split = region_m == 1 && object->wall_split_pitches(region, fine, coarse, coarse_is_outer);
            const double walls = wall_m > 1 ? wall_m * h : region_pitch;
            // The pitch a feature filament combines its own surfaces to while the region prints every layer.
            auto own_pitch = [&](int filament_id) {
                if (region_m > 1)
                    return region_pitch;
                const unsigned int m = object->layer_height_multiplier_for_filament((unsigned int)std::max(0, filament_id));
                return m > 1 ? m * h : region_pitch;
            };

            if (rc.wall_loops.value > 0) {
                add(rc.outer_wall_filament_id.value, "Outer walls", split ? (coarse_is_outer ? coarse : fine) * h : walls);
                if (PrintObject::region_prints_inner_walls(rc))
                    add(rc.inner_wall_filament_id.value, "Inner walls", split ? (coarse_is_outer ? fine : coarse) * h : walls);
            }
            if (rc.top_shell_layers.value > 0)
                add(rc.top_surface_filament_id.value, "Top surfaces",
                    rc.top_surface_density.value >= 100. - EPSILON ? own_pitch(rc.top_surface_filament_id.value) : region_pitch);
            if (rc.bottom_shell_layers.value > 0)
                add(rc.bottom_surface_filament_id.value, "Bottom surfaces", region_pitch);
            const bool solid_interior = std::abs(rc.sparse_infill_density.value - 100.) < EPSILON;
            if (!solid_interior && (rc.top_shell_layers.value > 0 || rc.bottom_shell_layers.value > 0))
                add(rc.internal_solid_filament_id.value, "Solid infill", own_pitch(rc.internal_solid_filament_id.value));
            if (rc.sparse_infill_density.value > 0.) {
                // At 100% density the interior is solid infill of the internal solid filament.
                const int filament_id = std::max(1, solid_interior ? rc.internal_solid_filament_id.value : rc.sparse_infill_filament_id.value);
                double    height      = region_pitch;
                if (region_m == 1) {
                    const double preferred = object->extruder_preferred_layer_height((unsigned int)filament_id);
                    double       cap       = 0.;
                    if (preferred > h + EPSILON) {
                        cap = std::min(preferred, config.nozzle_diameter.get_at(size_t(filament_id - 1)));
                    } else if (rc.infill_combination.value) {
                        for (int id : {rc.outer_wall_filament_id.value, rc.inner_wall_filament_id.value,
                                       rc.sparse_infill_filament_id.value, rc.internal_solid_filament_id.value})
                            cap = cap <= 0. ? config.nozzle_diameter.get_at(feature_filament_idx(id)) :
                                              std::min(cap, config.nozzle_diameter.get_at(feature_filament_idx(id)));
                        if (const double limit = rc.infill_combination_max_layer_height.get_abs_value(cap); limit > 0.)
                            cap = std::min(cap, limit);
                    }
                    if (cap > 0.)
                        height = std::max(1., std::floor(cap / h + EPSILON)) * h;
                }
                add(filament_id, solid_interior ? "Solid infill" : "Sparse infill", height);
            }
        }

        if (object->has_support_material())
            for (const bool interface : {false, true}) {
                const int    configured = interface ? object->config().support_interface_filament.value : object->config().support_filament.value;
                const unsigned int id   = configured > 0 ? (unsigned int)configured : object->resolved_default_support_filament(interface);
                if (id > 0) {
                    add(int(id), SUPPORTS, 0.);
                } else {
                    // Printed with whichever filament is loaded at the time: one of the object's own.
                    for (const PrintRegion &region : object->all_regions())
                        add(region.config().outer_wall_filament_id.value, SUPPORTS, 0.);
                }
            }
    }

    // Fixed order: the features as tool_head_features() lists them, then supports; each by height.
    auto rank = [](const std::string &feature) {
        const std::vector<ToolHeadFeature> &features = tool_head_features();
        for (size_t i = 0; i < features.size(); ++i)
            if (feature == features[i].name)
                return i;
        return features.size();
    };
    for (ToolHeadUse &head : out.heads)
        std::stable_sort(head.features.begin(), head.features.end(), [&rank](const ToolHeadFeatureUse &a, const ToolHeadFeatureUse &b) {
            const size_t ra = rank(a.feature), rb = rank(b.feature);
            return ra != rb ? ra < rb : a.height < b.height;
        });
    return out;
}

ToolHeadUses tool_head_uses(const Model &model, DynamicPrintConfig config)
{
    apply_extruder_layer_height_plan(config);
    const auto *nozzles = config.option<ConfigOptionFloats>("nozzle_diameter");
    const std::vector<double> head_nozzles = nozzles != nullptr ? nozzles->values : std::vector<double>{};
    Print print;
    try {
        print.apply(model, config);
    } catch (const std::exception &) {
        // An inconsistent config: nothing to tell beyond the nozzles.
        ToolHeadUses out;
        out.grid = config.opt_float("layer_height");
        out.heads.resize(std::max<size_t>(1, head_nozzles.size()));
        for (size_t head = 0; head < head_nozzles.size(); ++head)
            out.heads[head].nozzle = head_nozzles[head];
        return out;
    }
    return tool_head_uses(print, head_nozzles);
}

} // namespace Slic3r
