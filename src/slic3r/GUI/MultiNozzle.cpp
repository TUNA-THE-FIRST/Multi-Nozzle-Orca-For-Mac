#include "MultiNozzle.hpp"

#include "GUI.hpp"
#include "GUI_App.hpp"
#include "GUI_Utils.hpp"
#include "I18N.hpp"
#include "MainFrame.hpp"
#include "MsgDialog.hpp"
#include "OptionsGroup.hpp"
#include "PartPlate.hpp"
#include "Plater.hpp"
#include "Tab.hpp"
#include "format.hpp"
#include "Widgets/ComboBox.hpp"
#include "Widgets/DialogButtons.hpp"
#include "Widgets/Label.hpp"
#include "Widgets/StateColor.hpp"

#include "libslic3r/Model.hpp"
#include "libslic3r/PresetBundle.hpp"
#include "libslic3r/PrintConfig.hpp"
#include "libslic3r/Slicing.hpp"
#include "libslic3r/ToolHeadUse.hpp"
#include "libslic3r/Utils.hpp"

#include <boost/filesystem.hpp>

#include "md4c/src/md4c-html.h"

#include <wx/display.h>
#include <wx/filename.h>
#include <wx/scrolwin.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/utils.h>

#include <algorithm>
#include <cmath>
#include <locale>
#include <map>
#include <sstream>

namespace Slic3r { namespace GUI {

wxString mm_number(double value)
{
    // Rounded to the 5 um the layer heights are planned in; the C locale, as the sidebar fields.
    std::ostringstream oss;
    oss.imbue(std::locale::classic());
    oss << std::round(value * 1000.) / 1000.;
    return wxString(oss.str());
}

// A size or height as the presets write it ("0.4"), whatever the app's number locale (a decimal comma
// in Portuguese or German would make std::atof() stop at the point).
static double parse_mm(const std::string &text)
{
    std::istringstream iss(text);
    iss.imbue(std::locale::classic());
    double value = 0.;
    iss >> value;
    return iss.fail() ? 0. : value;
}

wxString nozzle_display_name(size_t head, size_t count)
{
    if (count == 2)
        return head == 0 ? _L("Left Nozzle") : _L("Right Nozzle");
    return count <= 1 ? _L("Nozzle") : wxString(_L("Nozzle")) + wxString::Format(" %d", int(head + 1));
}

// "Outer walls, Top surfaces": the features printed at `height` (every `every` object layers when it
// is not 0), translated. Not lowercased: that would break translations such as German nouns.
static wxString feature_list(const ToolHeadUse &use, double height, unsigned int every = 0)
{
    wxString list;
    for (const ToolHeadFeatureUse &feature : use.features) {
        if (std::abs(feature.height - height) > EPSILON || (every != 0 && feature.every != every))
            continue;
        const wxString name = _(wxString::FromUTF8(feature.feature));
        if (!list.Contains(name))
            list += (list.empty() ? "" : ", ") + name;
    }
    return list;
}

static wxString layers_text(double height, unsigned int every)
{
    return every <= 1 ? format_wxstr(_L("%1% mm layers, every layer"), mm_number(height)) :
                        // TRN In the multi-nozzle texts: the nozzle prints only every Nth layer of the object.
                        format_wxstr(_L("%1% mm layers, every %2% layers"), mm_number(height), every);
}

std::vector<wxString> tool_head_print_lines(const ToolHeadUse &use, double /*grid*/)
{
    // One line per layer height and object layer pitch: objects with layer heights of their own print
    // the same height every other number of their layers.
    std::vector<std::pair<double, unsigned int>> pitches;
    bool                                         supports = false;
    for (const ToolHeadFeatureUse &feature : use.features) {
        if (feature.height <= EPSILON)
            supports = true;
        else if (std::none_of(pitches.begin(), pitches.end(), [&feature](const std::pair<double, unsigned int> &p) {
                     return std::abs(p.first - feature.height) < EPSILON && p.second == feature.every;
                 }))
            pitches.emplace_back(feature.height, feature.every);
    }
    std::sort(pitches.begin(), pitches.end());
    std::vector<wxString> lines;
    for (const auto &[height, every] : pitches)
        lines.push_back(layers_text(height, every) + ": " + feature_list(use, height, every));
    if (supports)
        lines.push_back(_L("Supports"));
    return lines;
}

std::vector<wxString> tool_head_warnings(const ToolHeadUses &uses, const std::vector<double> &min_heights)
{
    std::vector<wxString> warnings;
    const size_t heads = uses.heads.size();
    for (size_t head = 0; head < heads; ++head) {
        const ToolHeadUse &use  = uses.heads[head];
        const wxString     name = nozzle_display_name(head, heads);
        if (use.features.empty()) {
            if (use.preferred > EPSILON)
                warnings.push_back(format_wxstr(_L("%1% is set to %2% mm layers, but nothing on this plate prints with it, so the "
                                                   "setting has no effect. Choose what it prints with \"Set up multi-nozzle printing\"."),
                                                name, mm_number(use.preferred)));
            continue;
        }
        double thickest = 0., thinnest = 0., thickest_own = 0.;
        bool   prints_preferred = false;
        for (const ToolHeadFeatureUse &feature : use.features) {
            if (feature.height <= EPSILON)
                continue;
            thickest = std::max(thickest, feature.height);
            thinnest = thinnest <= 0. ? feature.height : std::min(thinnest, feature.height);
            prints_preferred |= std::abs(feature.height - use.preferred) < EPSILON;
            // Infill combination's height is the engine's own rule, capped at the nozzles: no warning.
            if (!feature.combined)
                thickest_own = std::max(thickest_own, feature.height);
        }
        if (thickest <= 0.)
            continue; // supports only
        if (use.nozzle > EPSILON && thickest_own > 0.75 * use.nozzle + 1e-6) {
            wxString text = format_wxstr(_L("%1% prints %2% in %3% mm layers, thicker than its %4% mm nozzle prints well (about "
                                            "3/4 of the nozzle at most): the lines may not stick to each other."),
                                         name, feature_list(use, thickest_own), mm_number(thickest_own), mm_number(use.nozzle));
            if (thickest_own <= uses.grid + EPSILON)
                // The object layer height itself is too thick for this nozzle.
                text += " " + format_wxstr(_L("The object layer height, %1% mm, is already that thick: choose a thinner layer "
                                              "height in the process settings, or a larger nozzle."),
                                           mm_number(uses.grid));
            else if (use.preferred <= EPSILON)
                text += " " + format_wxstr(_L("This happens because %1% has no layer height of its own: it then follows the "
                                              "coarser nozzle it shares these parts with. Give it its own layer height (the "
                                              "object layer height, %2% mm) in its nozzle tab or with \"Set up multi-nozzle printing\"."),
                                           name, mm_number(uses.grid));
            else
                text += " " + _L("Choose a thinner layer height or a larger nozzle.");
            warnings.push_back(text);
        }
        if (use.preferred > EPSILON && !prints_preferred)
            warnings.push_back(format_wxstr(_L("%1% is set to %2% mm layers but prints %3% mm layers: a part prints with one "
                                               "layer pitch set by its walls, and only the walls, top surfaces and infill can "
                                               "print thicker on their own when it is a whole multiple of that pitch. Choose "
                                               "heights that are whole multiples of each other (for example 0.12 and 0.36 mm)."),
                                            name, mm_number(use.preferred), mm_number(thickest)));
        const double min = head < min_heights.size() ? min_heights[head] : 0.;
        if (min > EPSILON && thinnest < min - 1e-6)
            warnings.push_back(format_wxstr(_L("%1% prints %2% in %3% mm layers, below the %4% mm its nozzle prints reliably: "
                                               "the lines may come out uneven. Choose a coarser object layer height in the "
                                               "process settings, or let a finer nozzle print these features."),
                                            name, feature_list(use, thinnest), mm_number(thinnest), mm_number(min)));
    }
    return warnings;
}

// The objects on the current plate, as a model the setup dialog can try configs on.
static Model plate_model()
{
    Model      model;
    Plater    *plater = wxGetApp().plater();
    PartPlate *plate  = plater != nullptr ? plater->get_partplate_list().get_curr_plate() : nullptr;
    if (plate != nullptr)
        for (const ModelObject *object : plate->get_objects_on_this_plate())
            if (object != nullptr)
                model.add_object(*object);
    return model;
}

// The nozzle diameter of each tool head of the selected printer.
static std::vector<double> head_nozzles()
{
    const auto *nozzles = wxGetApp().preset_bundle->printers.get_edited_preset().config.option<ConfigOptionFloats>("nozzle_diameter");
    return nozzles != nullptr ? nozzles->values : std::vector<double>{};
}

ToolHeadUses plate_tool_head_uses()
{
    Plater *plater = wxGetApp().plater();
    if (plater == nullptr || wxGetApp().preset_bundle == nullptr)
        return {};
    // The plate's print as the last Print::apply() left it: its regions and config are only
    // replaced by the next apply, on this (the UI) thread, never while slicing.
    return tool_head_uses(plater->get_partplate_list().get_current_fff_print(), head_nozzles());
}

void open_multi_nozzle_guide()
{
    // The guide is Markdown, readable in the repository too; shown as a page rendered from it.
    const boost::filesystem::path source = boost::filesystem::path(resources_dir()) / "guides" / "multi_nozzle.md";
    // Both helpers throw on a missing or unwritable file.
    auto read = [](const boost::filesystem::path &path, std::string &text) {
        try { load_string_file(path, text); return true; } catch (const std::exception &) { return false; }
    };
    auto write = [](const boost::filesystem::path &path, const std::string &text) {
        try { save_string_file(path, text); return true; } catch (const std::exception &) { return false; }
    };
    std::string markdown;
    if (!read(source, markdown)) {
        show_error(nullptr, format_wxstr(_L("The multi-nozzle printing guide could not be opened from %1%.\n\n"
                                            "Why this can happen: the app's files are incomplete, for example after an "
                                            "interrupted copy to Applications.\n"
                                            "How to fix it: download the app again and copy it to Applications."),
                                         source.string()));
        return;
    }
    std::string html = "<!doctype html><html><head><meta charset=\"utf-8\"><title>Multi-nozzle printing</title><style>"
                       ":root{color-scheme:light dark}body{font:15px/1.55 -apple-system,BlinkMacSystemFont,sans-serif;"
                       "max-width:760px;margin:32px auto;padding:0 20px}table{border-collapse:collapse;margin:12px 0}"
                       "td,th{border:1px solid #8884;padding:6px 10px;vertical-align:top;text-align:left}"
                       "code{background:#8882;padding:1px 4px;border-radius:4px}</style></head><body>";
    md_html(markdown.data(), MD_SIZE(markdown.size()),
            [](const MD_CHAR *text, MD_SIZE size, void *out) { static_cast<std::string *>(out)->append(text, size); }, &html,
            MD_DIALECT_GITHUB, 0);
    html += "</body></html>";
    const boost::filesystem::path page = boost::filesystem::path(data_dir()) / "cache" / "multi_nozzle_guide.html";
    boost::system::error_code ec;
    boost::filesystem::create_directories(page.parent_path(), ec);
    if (!write(page, html)) {
        show_error(nullptr, format_wxstr(_L("The multi-nozzle printing guide could not be shown because %1% could not be "
                                            "written. Make room on the disk and try again; the guide is also in the app at %2%."),
                                         page.string(), source.string()));
        return;
    }
    wxLaunchDefaultBrowser(wxFileName::FileNameToURL(wxFileName(from_u8(page.string()))));
}

namespace {

// The features the dialog assigns, as tool_head_uses() reads them, and the support pair.
const char *const SUPPORT_KEYS[] = {"support_filament", "support_interface_filament"};

// The first filament (1-based) a tool head prints, by the filament map; 0 when it prints none.
int filament_of_head(const DynamicPrintConfig &config, size_t head, size_t filaments)
{
    for (size_t f = 1; f <= filaments; ++f)
        if (tool_head_of_filament(config, int(f)) == head)
            return int(f);
    return 0;
}

class MultiNozzleSetupDialog : public DPIDialog
{
public:
    MultiNozzleSetupDialog(wxWindow *parent);

    // The edits, applied by apply().
    std::vector<double>        nozzles;   // per tool head
    std::vector<double>        heights;   // per tool head, entered preferred layer height (0 = Default)
    std::map<std::string, int> features; // feature filament key -> filament (0 = Default)

    void apply() const;

protected:
    void on_dpi_changed(const wxRect &) override {}

private:
    DynamicPrintConfig         m_config;     // the plate's config as it is now, unplanned
    std::vector<std::string>   m_diameters;  // the nozzle sizes the printer has presets for
    std::vector<int>           m_head_filament;
    size_t                     m_filaments = 1; // the filaments in the sidebar
    Model                      m_model;      // the objects on the plate, for the preview
    double                     m_base_height = 0.2;

    wxScrolledWindow          *m_body = nullptr; // everything but the buttons, scrolled on a small screen
    ComboBox                  *m_recipe = nullptr;
    std::vector<ComboBox *>    m_size_fields;
    std::vector<ComboBox *>    m_height_fields;
    std::vector<std::vector<double>> m_height_choices;
    std::map<std::string, ComboBox *> m_feature_fields;
    std::vector<int>           m_feature_choices; // filament per feature field item (0 = Default)
    ogStaticText              *m_result = nullptr;

    void     fill_height_field(size_t head);
    wxString feature_choice_label(int filament, bool support) const;
    int      feature_choice_index(int filament);
    void     fit_to_screen();
    void     show_values();
    void     update_result();
    void     apply_recipe(int recipe);
    double   coarse_height(size_t head) const;
    double   max_height(size_t head) const;
    DynamicPrintConfig edited_config() const;
};

MultiNozzleSetupDialog::MultiNozzleSetupDialog(wxWindow *parent)
    : DPIDialog(parent, wxID_ANY, _L("Set up multi-nozzle printing"), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxCLOSE_BOX)
{
    PresetBundle &bundle = *wxGetApp().preset_bundle;
    m_config             = bundle.full_config(false);
    m_diameters          = bundle.printers.diameters_of_selected_printer();
    m_model              = plate_model();
    m_base_height        = std::max(0.01, m_config.opt_float("layer_height"));

    const auto  *nozzle_opt = m_config.option<ConfigOptionFloats>("nozzle_diameter");
    const size_t heads      = nozzle_opt != nullptr ? nozzle_opt->values.size() : 1;
    nozzles = nozzle_opt != nullptr ? nozzle_opt->values : std::vector<double>{0.4};
    if (const auto *stored = bundle.printers.get_edited_preset().config.option<ConfigOptionFloats>("extruder_layer_height"); stored != nullptr)
        heights = stored->values;
    heights.resize(heads, 0.);
    m_filaments = std::max<size_t>(1, bundle.filament_presets.size());
    for (size_t head = 0; head < heads; ++head)
        m_head_filament.push_back(filament_of_head(m_config, head, m_filaments));
    for (const ToolHeadFeature &feature : tool_head_features())
        features[feature.key] = m_config.has(feature.key) ? m_config.opt_int(feature.key) : 0;
    for (const char *key : SUPPORT_KEYS)
        features[key] = m_config.has(key) ? m_config.opt_int(key) : 0;

    SetBackgroundColour(*wxWHITE);
    SetFont(Label::Body_14);
    m_body = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
    m_body->SetBackgroundColour(*wxWHITE);
    m_body->SetScrollRate(0, FromDIP(12));
    wxWindow *body = m_body;
    auto *sizer = new wxBoxSizer(wxVERTICAL);
    const int pad = FromDIP(16);
    auto add_text = [this, body, sizer, pad](const wxString &text, const wxFont &font, int top) {
        auto *label = new wxStaticText(body, wxID_ANY, text);
        label->SetFont(font);
        label->Wrap(FromDIP(520));
        sizer->Add(label, 0, wxLEFT | wxRIGHT | wxTOP, top < 0 ? pad : FromDIP(top));
        return label;
    };
    auto make_combo = [this, body](int width, bool editable) {
        return new ComboBox(body, wxID_ANY, wxEmptyString, wxDefaultPosition, {FromDIP(width), -1}, 0, nullptr,
                            editable ? 0 : wxCB_READONLY);
    };

    add_text(_L("Print one object with several nozzles: for example a fine nozzle for the outside, so the surface stays "
                "smooth, and a large nozzle with thicker layers for the inside, which then prints much faster.\n\n"
                "Each nozzle prints its layers at its own height. The object is sliced at a fine \"object layer height\"; "
                "a nozzle with a thicker layer height prints only on every 2nd, 3rd... of those layers."),
             Label::Body_13, -1);

    // Recipes.
    add_text(_L("Start from a recipe"), Label::Head_14, 16);
    m_recipe = make_combo(520, false);
    for (const wxString &recipe : {_L("Choose a recipe..."),
                                   _L("Fine outside, fast inside: outer walls and surfaces fine; inner walls and infill coarse"),
                                   _L("Fine walls, fast infill: all walls and surfaces fine; solid and sparse infill coarse"),
                                   _L("Fast sparse infill: only the sparse infill coarse (the safest start)"),
                                   _L("Off: every feature with the object's filament, no layer heights per nozzle")})
        m_recipe->AppendString(recipe);
    m_recipe->SetSelection(0);
    m_recipe->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent &) {
        apply_recipe(m_recipe->GetSelection());
        m_recipe->SetSelection(0);
    });
    sizer->Add(m_recipe, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(6));
    if (heads > 1)
        add_text(_L("Recipes use the first nozzle as the fine one and the largest other nozzle as the coarse one, and give both "
                    "their own layer height. They never change the nozzle sizes: those must match the nozzles installed on "
                    "the printer."),
                 Label::Body_12, 4)->SetForegroundColour(StateColor::darkModeColorFor(wxColour("#6B6B6B")));

    // Nozzles.
    add_text(_L("Nozzles"), Label::Head_14, 16);
    auto *grid = new wxFlexGridSizer(3, FromDIP(6), FromDIP(12));
    grid->Add(new wxStaticText(body, wxID_ANY, wxEmptyString));
    grid->Add(new wxStaticText(body, wxID_ANY, _L("Installed nozzle size")));
    grid->Add(new wxStaticText(body, wxID_ANY, _L("Layer height it prints")));
    for (size_t head = 0; head < heads; ++head) {
        grid->Add(new wxStaticText(body, wxID_ANY, nozzle_display_name(head, heads)), 0, wxALIGN_CENTER_VERTICAL);
        ComboBox *size = make_combo(140, false);
        for (const std::string &d : m_diameters)
            size->AppendString(from_u8(d) + " mm");
        size->Enable(m_diameters.size() > 1);
        size->SetToolTip(_L("The diameter of the nozzle installed on this tool head. It must match the printer: a wrong size "
                            "makes every line of that nozzle too thick or too thin."));
        size->Bind(wxEVT_COMBOBOX, [this, head, size](wxCommandEvent &) {
            const int sel = size->GetSelection();
            if (sel >= 0 && size_t(sel) < m_diameters.size())
                nozzles[head] = parse_mm(m_diameters[sel]);
            fill_height_field(head);
            update_result();
        });
        m_size_fields.push_back(size);
        grid->Add(size, 0, wxALIGN_CENTER_VERTICAL);
        ComboBox *height = make_combo(260, false);
        height->SetToolTip(_L("How thick the layers of this nozzle are. The object layer height prints every layer; a thicker "
                              "height prints on every 2nd, 3rd... layer only, so it must be a whole multiple of the object layer "
                              "height - the list offers those. \"Not set\" lets the nozzle follow the nozzle it shares a part "
                              "with: a fine nozzle sharing a part with a coarse one then prints coarse layers too."));
        height->Bind(wxEVT_COMBOBOX, [this, head, height](wxCommandEvent &) {
            const int sel = height->GetSelection();
            if (sel >= 0 && size_t(sel) < m_height_choices[head].size())
                heights[head] = m_height_choices[head][sel];
            update_result();
        });
        m_height_fields.push_back(height);
        m_height_choices.emplace_back();
        grid->Add(height, 0, wxALIGN_CENTER_VERTICAL);
    }
    sizer->Add(grid, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(6));

    // Features.
    add_text(_L("What each nozzle prints"), Label::Head_14, 16);
    auto *features_grid = new wxFlexGridSizer(2, FromDIP(6), FromDIP(12));
    m_feature_choices.push_back(0);
    for (size_t head = 0; head < heads; ++head)
        if (m_head_filament[head] > 0)
            m_feature_choices.push_back(m_head_filament[head]);
    std::vector<std::pair<std::string, wxString>> rows;
    for (const ToolHeadFeature &feature : tool_head_features())
        rows.emplace_back(feature.key, _(wxString::FromUTF8(feature.name)));
    rows.emplace_back("support", _L("Supports"));
    for (const auto &[key, label] : rows) {
        features_grid->Add(new wxStaticText(body, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
        ComboBox *field = make_combo(260, false);
        for (int filament : m_feature_choices)
            field->AppendString(feature_choice_label(filament, key == "support"));
        field->Bind(wxEVT_COMBOBOX, [this, key = key, field](wxCommandEvent &) {
            const int sel = field->GetSelection();
            const int filament = sel >= 0 && size_t(sel) < m_feature_choices.size() ? m_feature_choices[sel] : 0;
            if (key == "support")
                for (const char *support_key : SUPPORT_KEYS)
                    features[support_key] = filament;
            else
                features[key] = filament;
            update_result();
        });
        m_feature_fields[key] = field;
        features_grid->Add(field, 0, wxALIGN_CENTER_VERTICAL);
    }
    sizer->Add(features_grid, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(6));
    add_text(_L("\"The object's own filament\" follows the filament each object or part is set to in the object list. Supports "
                "are printed only where support is turned on; supports left at \"Any nozzle\" print with whichever nozzle "
                "is already printing that layer. Recipes leave the supports as they are."),
             Label::Body_12, 4)->SetForegroundColour(StateColor::darkModeColorFor(wxColour("#6B6B6B")));

    // Result.
    add_text(_L("What will happen"), Label::Head_14, 16);
    m_result = new ogStaticText(body, wxEmptyString, wxST_NO_AUTORESIZE);
    m_result->SetFont(Label::Body_13);
    m_result->SetMinSize({FromDIP(520), -1});
    sizer->Add(m_result, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(6));

    sizer->AddSpacer(FromDIP(4));
    m_body->SetSizer(sizer);

    // The buttons stay below the scrolled body, so they are on screen however long the result grows.
    auto *buttons = new DialogButtons(this, {"OK", "Cancel"});
    buttons->GetOK()->SetLabel(_L("Apply"));
    buttons->GetOK()->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_OK); });
    buttons->GetCANCEL()->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { EndModal(wxID_CANCEL); });
    auto *outer = new wxBoxSizer(wxVERTICAL);
    outer->Add(m_body, 1, wxEXPAND);
    outer->Add(buttons, 0, wxEXPAND | wxTOP, FromDIP(8));
    SetSizer(outer);

    for (size_t head = 0; head < heads; ++head)
        fill_height_field(head);
    show_values();
    update_result();
    wxGetApp().UpdateDlgDarkUI(this);
    CenterOnParent();
}

// As tall as the content, at most three quarters of the screen: a 13-inch Mac shows the whole dialog
// with its buttons, the body scrolls.
void MultiNozzleSetupDialog::fit_to_screen()
{
    const int     display_index = wxDisplay::GetFromWindow(this);
    const wxRect  area          = wxDisplay(display_index == wxNOT_FOUND ? 0u : unsigned(display_index)).GetClientArea();
    const wxSize  content       = m_body->GetSizer()->CalcMin();
    const int     scrollbar     = wxSystemSettings::GetMetric(wxSYS_VSCROLL_X, this);
    m_body->SetMinSize({content.x + scrollbar, std::min(content.y, area.GetHeight() * 3 / 4)});
    m_body->FitInside();
    Layout();
    Fit();
}

// The label of a filament in the feature lists: the nozzle that prints it.
wxString MultiNozzleSetupDialog::feature_choice_label(int filament, bool support) const
{
    if (filament == 0)
        // Supports at Default print with the filament active on the layer, not the object's.
        return support ? _L("Any nozzle (the one already printing the layer)") : _L("The object's own filament");
    if (size_t(filament) > m_filaments)
        // TRN A filament number that is not one of the sidebar's filaments, for example a mixed filament.
        return format_wxstr(_L("Filament %1%"), filament);
    return format_wxstr(_L("%1% (filament %2%)"), nozzle_display_name(tool_head_of_filament(m_config, filament), nozzles.size()), filament);
}

// The item of a filament in the feature lists, added to every list when it is not there yet (a value
// set in the process settings that no nozzle prints first), so the lists show the real setting.
int MultiNozzleSetupDialog::feature_choice_index(int filament)
{
    const auto it = std::find(m_feature_choices.begin(), m_feature_choices.end(), filament);
    if (it != m_feature_choices.end())
        return int(it - m_feature_choices.begin());
    m_feature_choices.push_back(filament);
    for (const auto &[key, field] : m_feature_fields)
        field->AppendString(feature_choice_label(filament, key == "support"));
    return int(m_feature_choices.size() - 1);
}

// The heights offered for a tool head: the object layer height and its whole multiples the nozzle
// prints well; an entered height that is none of them is kept as an entry too.
void MultiNozzleSetupDialog::fill_height_field(size_t head)
{
    ComboBox *field = m_height_fields[head];
    std::vector<double> &choices = m_height_choices[head];
    choices.assign(1, 0.);
    const double max = max_height(head);
    for (int k = 1; k * m_base_height <= max + 1e-6; ++k)
        choices.push_back(std::round(k * m_base_height * 1000.) / 1000.);
    if (heights[head] > EPSILON &&
        std::none_of(choices.begin(), choices.end(), [h = heights[head]](double c) { return std::abs(c - h) < 1e-6; }))
        choices.push_back(heights[head]);
    field->Clear();
    for (double h : choices) {
        if (h <= EPSILON) {
            field->AppendString(_L("Not set (follows the nozzle it shares parts with)"));
            continue;
        }
        const long every = std::lround(h / m_base_height);
        field->AppendString(std::abs(every * m_base_height - h) >= 1e-6 ?
                                format_wxstr(_L("%1% mm (not a multiple of the object layer height)"), mm_number(h)) :
                            every <= 1 ? format_wxstr(_L("%1% mm (every layer)"), mm_number(h)) :
                                         format_wxstr(_L("%1% mm (every %2% layers)"), mm_number(h), every));
    }
    const auto it = std::find_if(choices.begin(), choices.end(), [h = heights[head]](double c) { return std::abs(c - h) < 1e-6; });
    field->SetSelection(it == choices.end() ? 0 : int(it - choices.begin()));
}

void MultiNozzleSetupDialog::show_values()
{
    for (size_t head = 0; head < m_size_fields.size(); ++head) {
        const auto it = std::find_if(m_diameters.begin(), m_diameters.end(),
                                     [d = nozzles[head]](const std::string &s) { return std::abs(parse_mm(s) - d) < 1e-6; });
        if (it != m_diameters.end())
            m_size_fields[head]->SetSelection(int(it - m_diameters.begin()));
        else
            m_size_fields[head]->SetValue(mm_number(nozzles[head]) + " mm");
        fill_height_field(head);
    }
    for (const auto &[key, field] : m_feature_fields) {
        const int filament = key == "support" ? features[SUPPORT_KEYS[0]] : features[key];
        field->SetSelection(feature_choice_index(filament));
    }
}

DynamicPrintConfig MultiNozzleSetupDialog::edited_config() const
{
    DynamicPrintConfig config = m_config;
    // A nozzle size brings the layer height limits of its size preset along, as the sidebar does.
    const auto *current = m_config.option<ConfigOptionFloats>("nozzle_diameter");
    auto       *mins    = config.option<ConfigOptionFloats>("min_layer_height");
    auto       *maxs    = config.option<ConfigOptionFloats>("max_layer_height");
    for (size_t head = 0; head < nozzles.size() && current != nullptr && mins != nullptr && maxs != nullptr; ++head) {
        if (head < current->values.size() && std::abs(current->values[head] - nozzles[head]) < 1e-6)
            continue;
        const Preset *preset = wxGetApp().preset_bundle->printers.find_system_preset_by_model_and_variant(
            m_config.opt_string("printer_model"), into_u8(mm_number(nozzles[head])));
        if (preset == nullptr)
            continue;
        const auto *v_min = preset->config.option<ConfigOptionFloats>("min_layer_height");
        const auto *v_max = preset->config.option<ConfigOptionFloats>("max_layer_height");
        if (v_min == nullptr || v_max == nullptr || v_min->values.empty() || v_max->values.empty())
            continue;
        mins->values.resize(nozzles.size(), mins->values.empty() ? 0. : mins->values.back());
        maxs->values.resize(nozzles.size(), maxs->values.empty() ? 0. : maxs->values.back());
        mins->values[head] = v_min->get_at(head);
        maxs->values[head] = v_max->get_at(head);
    }
    config.set_key_value("nozzle_diameter", new ConfigOptionFloats(nozzles));
    config.set_key_value("extruder_layer_height", new ConfigOptionFloats(heights));
    for (const auto &[key, filament] : features)
        config.set_key_value(key, new ConfigOptionInt(filament));
    return config;
}

void MultiNozzleSetupDialog::update_result()
{
    const DynamicPrintConfig config = edited_config();
    // Decided by the slicing engine's own rules on the plate's objects (ToolHeadUse.hpp).
    const ToolHeadUses uses = tool_head_uses(m_model, config);
    wxString text;
    if (uses.grid <= EPSILON) {
        text = _L("Add an object to the plate to see what each nozzle will print.");
    } else {
        text = format_wxstr(_L("The objects are sliced in %1% mm layers."), mm_number(uses.grid));
        // The planned grid, not the first object's own layer height, which an object setting may change.
        const double planned = effective_extruder_layer_heights(config).grid;
        if (planned > EPSILON && planned < m_base_height - 1e-6)
            text += " " + format_wxstr(_L("That is finer than the %1% mm of the process preset, so that every nozzle's layer height is "
                                          "a whole multiple of it; slicing takes longer."),
                                       mm_number(m_base_height));
        size_t used = 0;
        for (size_t head = 0; head < uses.heads.size(); ++head) {
            const ToolHeadUse &use = uses.heads[head];
            text += "\n" + wxString::FromUTF8(u8"\u2022 ") + nozzle_display_name(head, uses.heads.size()) + " (" + mm_number(use.nozzle) + " mm): ";
            if (use.features.empty()) {
                text += _L("prints nothing");
                continue;
            }
            ++used;
            const std::vector<wxString> lines = tool_head_print_lines(use, uses.grid);
            for (size_t i = 0; i < lines.size(); ++i)
                text += (i == 0 ? "" : "; ") + lines[i];
        }
        if (used <= 1)
            text += "\n\n" + _L("Only one nozzle prints, so the object prints as usual. Assign a feature to another nozzle to use two.");
        else
            text += "\n\n" + _L("Check the result in the preview after slicing: the \"Layer Height\" and \"Tool\" views of the "
                                "legend show which nozzle prints each line, and at which height.");
        const auto *min_heights = config.option<ConfigOptionFloats>("min_layer_height");
        for (const wxString &warning : tool_head_warnings(uses, min_heights != nullptr ? min_heights->values : std::vector<double>{}))
            text += "\n\n" + wxString::FromUTF8(u8"\u26A0 ") + warning;
    }
    m_result->SetText(text, false);
    m_result->Wrap(FromDIP(520));
    fit_to_screen();
}

// The thickest layer a tool head's nozzle prints well: the maximum layer height of the printer
// profile for its nozzle size (the sidebar adopts it with the size), at most 3/4 of the nozzle.
double MultiNozzleSetupDialog::max_height(size_t head) const
{
    const double nozzle  = nozzles[head];
    double       max     = 0.75 * nozzle;
    const auto  *current = m_config.option<ConfigOptionFloats>("nozzle_diameter");
    const DynamicPrintConfig *source = nullptr;
    if (current != nullptr && head < current->values.size() && std::abs(current->values[head] - nozzle) < 1e-6)
        source = &m_config;
    else if (const Preset *preset = wxGetApp().preset_bundle->printers.find_system_preset_by_model_and_variant(
                 m_config.opt_string("printer_model"), into_u8(mm_number(nozzle)));
             preset != nullptr)
        source = &preset->config;
    if (source != nullptr)
        if (const auto *limits = source->option<ConfigOptionFloats>("max_layer_height"); limits != nullptr && !limits->values.empty())
            if (const double limit = limits->get_at(head); limit > EPSILON)
                max = std::min(max, limit);
    return max;
}

// The coarse layer height a recipe gives a tool head: the thickest whole multiple of the object
// layer height its nozzle prints well (the object layer height itself when no multiple fits).
double MultiNozzleSetupDialog::coarse_height(size_t head) const
{
    const double max = max_height(head);
    const int    k   = std::max(1, int(std::floor(max / m_base_height + 1e-6)));
    return std::round(k * m_base_height * 1000.) / 1000.;
}

void MultiNozzleSetupDialog::apply_recipe(int recipe)
{
    if (recipe <= 0)
        return;
    const size_t heads = nozzles.size();
    // Fine: the first nozzle. Coarse: the largest other nozzle that prints a filament.
    size_t coarse = heads;
    for (size_t head = 1; head < heads; ++head)
        if (m_head_filament[head] > 0 && (coarse == heads || nozzles[head] > nozzles[coarse] + 1e-6))
            coarse = head;
    const int fine_filament   = m_head_filament.empty() ? 0 : m_head_filament[0];
    const int coarse_filament = coarse < heads ? m_head_filament[coarse] : 0;
    if (recipe != 4 && (coarse_filament == 0 || fine_filament == 0)) {
        // Nothing is changed: a recipe that cannot be set up must not reset what is set.
        MessageDialog(this,
                      heads <= 1 ? _L("The selected printer has one nozzle, so there is nothing to share between nozzles. Select a "
                                      "printer preset with several tool heads, such as the Snapmaker U1, to print with several nozzles.") :
                                   _L("This recipe needs a filament on the first nozzle and on a second one. Add filaments in the "
                                      "sidebar's filament list until each nozzle you want to use has one (on the U1, filament 2 is "
                                      "on nozzle 2), then choose the recipe again."),
                      _L("Set up multi-nozzle printing"), wxOK | wxICON_INFORMATION)
            .ShowModal();
        return;
    }
    // Only what the recipes describe: the six features and the layer heights. Supports stay as set
    // (a soluble support filament, for example).
    std::fill(heights.begin(), heights.end(), 0.);
    for (const ToolHeadFeature &feature : tool_head_features())
        features[feature.key] = 0;
    if (recipe == 4) {
        show_values();
        update_result();
        return;
    }
    // Both nozzles get a height of their own: a nozzle without one follows the walls of the parts
    // it shares, so the fine nozzle would print the coarse layers wherever a coarse wall drives them.
    heights[0]      = std::round(m_base_height * 1000.) / 1000.;
    heights[coarse] = coarse_height(coarse);
    const int F = fine_filament, C = coarse_filament;
    std::map<std::string, int> assignment;
    switch (recipe) {
    case 1: // fine outside, fast inside
        assignment = {{"outer_wall_filament_id", F},     {"top_surface_filament_id", F},    {"bottom_surface_filament_id", F},
                      {"inner_wall_filament_id", C},     {"internal_solid_filament_id", C}, {"sparse_infill_filament_id", C}};
        break;
    case 2: // fine walls, fast infill
        assignment = {{"outer_wall_filament_id", F},     {"top_surface_filament_id", F},    {"bottom_surface_filament_id", F},
                      {"inner_wall_filament_id", F},     {"internal_solid_filament_id", C}, {"sparse_infill_filament_id", C}};
        break;
    case 3: // fast sparse infill
        assignment = {{"outer_wall_filament_id", F},     {"top_surface_filament_id", F},    {"bottom_surface_filament_id", F},
                      {"inner_wall_filament_id", F},     {"internal_solid_filament_id", F}, {"sparse_infill_filament_id", C}};
        break;
    default: break;
    }
    for (const auto &[key, filament] : assignment)
        features[key] = filament;
    show_values();
    update_result();
}

void MultiNozzleSetupDialog::apply() const
{
    Plater *plater = wxGetApp().plater();
    if (plater == nullptr)
        return;
    Sidebar &sidebar = plater->sidebar();
    const DynamicPrintConfig &printer = wxGetApp().preset_bundle->printers.get_edited_preset().config;
    const auto *current = printer.option<ConfigOptionFloats>("nozzle_diameter");
    for (size_t head = 0; head < nozzles.size(); ++head)
        if (current == nullptr || head >= current->values.size() || std::abs(current->values[head] - nozzles[head]) > 1e-6)
            sidebar.apply_nozzle_diameter(head, mm_number(nozzles[head]));
    for (size_t head = 0; head < heights.size(); ++head)
        sidebar.set_preferred_layer_height(head, heights[head]);
    if (Tab *tab = wxGetApp().get_tab(Preset::TYPE_PRINT); tab != nullptr) {
        DynamicPrintConfig process = wxGetApp().preset_bundle->prints.get_edited_preset().config;
        bool               changed = false;
        for (const auto &[key, filament] : features)
            if (!process.has(key) || process.opt_int(key) != filament) {
                process.set_key_value(key, new ConfigOptionInt(filament));
                changed = true;
            }
        if (changed)
            tab->load_config(process);
    }
    plater->schedule_background_process();
}

} // namespace

void open_multi_nozzle_setup()
{
    MultiNozzleSetupDialog dialog(wxGetApp().mainframe);
    if (dialog.ShowModal() == wxID_OK)
        dialog.apply();
}

}} // namespace Slic3r::GUI
