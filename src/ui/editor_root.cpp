#include <cmath>
#include <optional>

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


juce::ComponentPeer* EditorRoot::own_peer() const
{
    juce::ComponentPeer* const peer = getPeer();

    if (peer == nullptr || &peer->getComponent() != this) {
        return nullptr;
    }

    return peer;
}


double EditorRoot::content_scale() const
{
    juce::ComponentPeer* const peer = own_peer();

    /* Not our window: whoever owns it has already scaled us, so there is no
     * remainder. Also the state before addToDesktop(), where 1.0 is the only
     * honest answer -- nothing has been sized or painted at that point either. */
    if (peer == nullptr) {
        return 1.0;
    }

    /* Whatever set_host_scale() pushed, if anything; otherwise what the peer
     * worked out from the OS when the window was created. */
    double const scale = peer->getPlatformScaleFactor();

    return scale > 0.0 ? scale : 1.0;
}


int EditorRoot::to_physical(int const logical) const
{
    return juce::roundToInt((double)logical * content_scale());
}


int EditorRoot::to_logical(int const physical) const
{
    return juce::roundToInt((double)physical / content_scale());
}


void EditorRoot::preferred_physical_size(int& width, int& height) const
{
    preferred_size(width, height);

    double const scale = content_scale();

    width = juce::roundToInt((double)width * scale);
    height = juce::roundToInt((double)height * scale);
}


bool EditorRoot::set_host_scale(double const scale)
{
    if (scale <= 0.0) {
        return false;
    }

    bool const changed = host_scale != scale;

    host_scale = scale;

    juce::ComponentPeer* const peer = own_peer();

    if (peer == nullptr) {
        /* No window yet; nothing to push onto. The value is kept so that
         * whoever builds the window can re-apply it. */
        return changed;
    }

    if (!changed && peer->getCustomPlatformScaleFactor().has_value()) {
        return false;
    }

    /* Override the peer's OS-derived scaling with the host's: from here
     * content_scale() reads this value back out. */
    peer->setCustomPlatformScaleFactor(std::optional<double>(scale));

    return changed;
}


void EditorRoot::apply_size_constraints(int& width, int& height) const
{
    double const bw = (double)base_width();
    double const bh = (double)base_height();

    /* The diagonal of the proposed rectangle, projected onto the locked aspect
     * ratio: both axes contribute, so the result moves continuously with the
     * rectangle the host hands us.
     *
     * The previous rule instead guessed which edge the user had dragged (the
     * axis whose implied scale departed further from the current one) and used
     * that axis alone. Because it compared against the *current* size, which
     * this very function had just changed, the guess could flip from one drag
     * event to the next: successive rectangles a few pixels apart would resolve
     * alternately to the width- and height-derived scale, and the window jumped
     * between the two. Projection has no branch to flip.
     *
     * The scale is quantised to whole pixels of width so that a drag that does
     * not move far enough to change the snapped size reports the size it already
     * has -- see handle_resize_request(), which drops no-op resizes on that
     * basis, and would otherwise ping-pong the host with sub-pixel corrections. */
    double scale = (
        ((double)width * bw + (double)height * bh) / (bw * bw + bh * bh)
    );

    scale = juce::jlimit(MIN_SCALE, MAX_SCALE, scale);

    width = juce::roundToInt(bw * scale);
    height = juce::roundToInt((double)width * bh / bw);
}


EditorRoot::EditorRoot(Synth& synth, char const* const version)
    : host_scale(0.0),
    gui(nullptr),
    synth(synth),
    version(version),
    matrix_active(false),
    in_resize(false)
{
    int width = 0;
    int height = 0;

    preferred_size(width, height);
    setSize(width, height);

    instances.add(this);

    /* NB the legacy GUI is deliberately NOT built here; see ensure_legacy_gui(). */

    /* Host for the embedded legacy GUI (hidden until the MATRIX tab is active). */
    addChildComponent(matrix_host);

    /* New simplified GUI — opaque, on top, shown by default. */
    new_gui = std::make_unique<NewGui>(synth);
    new_gui->on_matrix = [this](bool const active) { set_matrix(active); };
    addAndMakeVisible(*new_gui);

    /* One rule for both builds: zoom, never reflow. */
    constrainer.setFixedAspectRatio((double)base_width() / (double)base_height());
    constrainer.setSizeLimits(
        (int)((double)base_width() * MIN_SCALE),
        (int)((double)base_height() * MIN_SCALE),
        (int)((double)base_width() * MAX_SCALE),
        (int)((double)base_height() * MAX_SCALE)
    );

    resizer = std::make_unique<juce::ResizableCornerComponent>(this, &constrainer);
    addAndMakeVisible(*resizer);

    startTimerHz((int)JS80P::GUI::REFRESH_RATE);

    resized();
}


void EditorRoot::ensure_legacy_gui()
{
    if (gui != nullptr) {
        return;
    }

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

    if (juce::Component* const legacy_root =
            (juce::Component*)gui->get_root_platform_widget()) {
        matrix_host.addChildComponent(legacy_root);
    }
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
    /* First entry into the tab is what pays for the legacy GUI, rather than
     * every editor open. */
    if (active) {
        ensure_legacy_gui();
    }

    matrix_active = active;
    matrix_host.setVisible(active);

    if (active) {
        gui->show();
        layout_matrix();
        matrix_host.toFront(false);   /* over the new GUI's body, below its header */
        resizer->toFront(false);      /* ...but never over the grip */
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

        /* The grip resizes THIS component; the host still believes the window is
         * whatever it last set. Tell it. Resizes that came from the host reach
         * here too, and would bounce straight back -- but by then the host's own
         * rect already matches, and request_resize drops sizes equal to the
         * current one, so the echo stops there rather than looping. */
        request_resize(getWidth(), getHeight());
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

    if (resizer != nullptr) {
        /* Untransformed, unlike new_gui, so it stays a fixed grab target at any
         * zoom; kept above whatever else was just raised. */
        int const grip = 16;

        resizer->setBounds(getWidth() - grip, getHeight() - grip, grip, grip);
        resizer->toFront(false);
    }
}


void EditorRoot::timerCallback()
{
    /* NULL until the MATRIX tab is first opened; nothing to idle before that. */
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
