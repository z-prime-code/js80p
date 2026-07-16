#include <cmath>

#include "ui/editor_root.hpp"


namespace JS80P
{

/* Session-only user scale, shared across instances. See header. */
int EditorRoot::shared_width = 0;
int EditorRoot::shared_height = 0;
juce::Array<EditorRoot*, juce::CriticalSection> EditorRoot::instances;
bool EditorRoot::syncing = false;


int EditorRoot::base_width()
{
    return (int)(0.527 * (double)JS80P::GUI::WIDTH);
}


int EditorRoot::base_height()
{
    return (int)(0.527 * (double)(JS80P::GUI::HEIGHT - 53));
}


void EditorRoot::preferred_size(int& width, int& height)
{
    if (shared_width > 0 && shared_height > 0) {
        width = shared_width;
        height = shared_height;

        return;
    }

    width = base_width();
    height = base_height();
}


void EditorRoot::apply_size_constraints(int& width, int& height) const
{
    double const bw = (double)base_width();
    double const bh = (double)base_height();

    double const scale_from_width = (double)width / bw;
    double const scale_from_height = (double)height / bh;

    /* Which edge did the user drag? The host doesn't say, so treat the axis that
     * moved further from the scale we're currently at as the intended one. */
    double const current = (
        getWidth() > 0 ? (double)getWidth() / bw : scale_from_width
    );

    double scale = (
        std::fabs(scale_from_width - current)
            >= std::fabs(scale_from_height - current)
            ? scale_from_width
            : scale_from_height
    );

    scale = juce::jlimit(MIN_SCALE, MAX_SCALE, scale);

    width = juce::roundToInt(bw * scale);
    height = juce::roundToInt(bh * scale);
}


EditorRoot::EditorRoot(Synth& synth, char const* const version)
    : gui(nullptr),
    matrix_active(false),
    in_resize(false)
{
    int width = 0;
    int height = 0;

    preferred_size(width, height);
    setSize(width, height);

    instances.add(this);

    /* Original GUI (JUCE Widget backend). Its root is reparented into matrix_host
     * so the new GUI can embed it, contained, under its MATRIX tab. */
    gui = new JS80P::GUI(
        version,
        nullptr,
        (JS80P::GUI::PlatformWidget)(juce::Component*)this,
        synth,
        true,
        this
    );
    gui->show();

    /* Host for the embedded legacy GUI (hidden until the MATRIX tab is active). */
    addChildComponent(matrix_host);

    if (juce::Component* const legacy_root =
            (juce::Component*)gui->get_root_platform_widget()) {
        matrix_host.addChildComponent(legacy_root);
    }

    /* New simplified GUI — opaque, on top, shown by default. */
    new_gui = std::make_unique<NewGui>(synth);
    new_gui->on_matrix = [this](bool const active) { set_matrix(active); };
    addAndMakeVisible(*new_gui);

    startTimerHz((int)JS80P::GUI::REFRESH_RATE);

    resized();
}


EditorRoot::~EditorRoot()
{
    stopTimer();

    instances.removeAllInstancesOf(this);

    new_gui = nullptr;

    delete gui;
    gui = nullptr;
}


void EditorRoot::request_resize(int const width, int const height)
{
    if (on_resize_request) {
        on_resize_request(width, height);

        return;
    }

    setSize(width, height);
}


void EditorRoot::sync_all_to_shared(EditorRoot const* const source)
{
    if (syncing || shared_width <= 0 || shared_height <= 0) {
        return;
    }

    syncing = true;

    /* Hold the list's lock for the whole sweep so registration/removal from
     * another thread can't invalidate iteration mid-loop. */
    juce::Array<EditorRoot*, juce::CriticalSection>::ScopedLockType const lock(
        instances.getLock()
    );

    for (EditorRoot* const editor : instances) {
        if (editor == source) {
            continue;
        }

        bool const needs_resize =
            editor->getWidth() != shared_width || editor->getHeight() != shared_height;

        if (needs_resize) {
            editor->request_resize(shared_width, shared_height);
        }
    }

    syncing = false;
}


void EditorRoot::set_matrix(bool const active)
{
    matrix_active = active;
    matrix_host.setVisible(active);

    if (active) {
        gui->show();
        layout_matrix();
        matrix_host.toFront(false);   /* over the new GUI's body, below its header */
    }
}


void EditorRoot::layout_matrix()
{
    if (gui == nullptr || in_resize) {
        return;
    }

    /* The body sits below the header, which is 46 px at base resolution but drawn
     * scaled (see resized()); matrix_host is an untransformed sibling, so clear
     * the scaled header height. Fit the legacy GUI within the body, aspect-locked,
     * then centre it. */
    double const scale = (double)getWidth() / (double)base_width();
    int const header_h = juce::roundToInt(46.0 * scale);
    juce::Rectangle<int> const body(
        0, header_h, getWidth(), juce::jmax(0, getHeight() - header_h)
    );

    in_resize = true;
    gui->resize(body.getWidth(), body.getHeight());
    in_resize = false;

    juce::Rectangle<int> scaled(0, 0, gui->get_width(), gui->get_height());
    scaled.setCentre(body.getCentre());
    matrix_host.setBounds(scaled);   /* the legacy root fills this from (0, 0) */
}


void EditorRoot::resized()
{
    if (new_gui == nullptr || !new_gui->has_painted) {
        /* Before first paint, a size differing from the shared scale is the host
         * restoring its stale saved size — override it back, don't record it. */
        if (shared_width > 0
                && (getWidth() != shared_width || getHeight() != shared_height)) {
            request_resize(shared_width, shared_height);
            return;   /* the resize re-enters resized(); layout runs there. */
        }
    }
    else if (!syncing && getWidth() > 0 && getHeight() > 0) {
        /* After first paint, any resize is a user rescale (aspect-locked): record
         * it and push it onto the other live editors, hidden ones included, so
         * they show at this size when revealed. Skipped while we drive them. */
        shared_width = getWidth();
        shared_height = getHeight();
        sync_all_to_shared(this);
    }

    if (new_gui != nullptr) {
        /* Lay the new GUI out at the base resolution and scale it as a unit. */
        double const scale = (double)getWidth() / (double)base_width();

        new_gui->setBounds(0, 0, base_width(), base_height());
        new_gui->setTransform(juce::AffineTransform::scale((float)scale));
    }

    if (matrix_active) {
        layout_matrix();
        matrix_host.toFront(false);
    }
}


void EditorRoot::timerCallback()
{
    if (gui != nullptr) {
        gui->idle();
    }
}


void EditorRoot::handle_resize_request(
        int const new_width, int const new_height
) {
    request_resize(new_width, new_height);
}

}
