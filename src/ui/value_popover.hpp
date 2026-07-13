#ifndef JS80P__UI__VALUE_POPOVER_HPP
#define JS80P__UI__VALUE_POPOVER_HPP

#include <memory>

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/theme.hpp"


namespace JS80P
{

/**
 * \brief The free-floating value popover, a sibling kept in front of the other
 *        controls so it can sit above a control's title / dial / badge without
 *        being clipped by the control's own bounds. Shows a value, optionally
 *        with a title line above it (for controls / widgets with no visible
 *        caption). Purely decorative: it never intercepts the mouse.
 */
class ValuePopover : public juce::Component
{
    public:
        static constexpr float ARROW_H = 4.0f;
        static constexpr float LINE_H = 14.0f;

        ValuePopover()
        {
            setWantsKeyboardFocus(false);
            /* Click-through, but let a hosted child (the text editor) receive the
             * mouse so the caret can be placed. */
            setInterceptsMouseClicks(false, true);
        }

        void set_content(juce::String title, juce::String value, juce::Colour accent)
        {
            /* The title is styled like a knob caption (dim, uppercased). */
            title_ = title.toUpperCase();
            value_ = std::move(value);
            accent_ = accent;
        }

        /** In edit mode the value line is left blank for a hosted text editor. */
        void set_editing(bool const on) { editing_ = on; }

        void set_arrow_x(float const x) { arrow_x_ = x; }

        /** Point the arrow up (bubble sitting *below* its anchor) instead of the
         *  default down (bubble above). Used when there is no room above — e.g.
         *  the header OUT knob, whose bubble would otherwise clip off the top. */
        void set_arrow_up(bool const on) { arrow_up_ = on; }

        static juce::Font value_font() { return juce::Font(juce::FontOptions().withHeight(11.0f).withStyle("Bold")); }
        static juce::Font title_font() { return juce::Font(juce::FontOptions().withHeight(11.0f)); }

        bool has_title() const { return title_.isNotEmpty(); }

        /** Preferred size (including the pointer arrow) for the current content. */
        juce::Rectangle<int> measure() const
        {
            float w = juce::jmax(30.0f, juce::GlyphArrangement::getStringWidth(value_font(), value_));
            float h = LINE_H;

            if (has_title()) {
                w = juce::jmax(w, juce::GlyphArrangement::getStringWidth(title_font(), title_));
                h += LINE_H;
            }

            return juce::Rectangle<int>(
                0, 0,
                juce::roundToInt(juce::jmax(30.0f, w + 14.0f)),
                juce::roundToInt(h + 4.0f + ARROW_H)
            );
        }

        /** The value line's rectangle in local coordinates (where the text editor
         *  is placed while editing). */
        juce::Rectangle<int> value_area() const
        {
            juce::Rectangle<float> body = getLocalBounds().toFloat().reduced(0.5f);
            if (arrow_up_) {
                body.removeFromTop(ARROW_H);
            } else {
                body.removeFromBottom(ARROW_H);
            }
            body = body.reduced(3.0f, 2.0f);
            if (has_title()) {
                body.removeFromTop(LINE_H);
            }
            return body.getSmallestIntegerContainer();
        }

        /**
         * \brief The component a popover for `owner` should be parented to: the
         *        (transformed) new-GUI root, so the popover inherits the same
         *        scale AffineTransform as the controls and renders at the right
         *        size when the UI is zoomed. The GUI root spans the whole surface,
         *        so the popover is never clipped by a thin strip or header row
         *        (the reason it isn't just a child of the control).
         *
         * The new GUI is laid out at a fixed base resolution and drawn through a
         * uniform scale transform on its root (see PluginEditor::resized()). That
         * root is the *only* component in the chain that carries a transform, so
         * it is the outermost ancestor of `owner` with a non-identity transform.
         * Keying on the transform (rather than "the outermost child of the
         * top-level") is what makes this correct in a plain host window too: when
         * the plugin runs standalone the editor sits *between* the GUI root and
         * the top-level window, so the naive walk would stop at the untransformed
         * editor and the popover would be positioned right but drawn at base size.
         *
         * At 1.0x the transform is the identity (nothing to inherit); there we
         * fall back to the outermost child of the top-level, a full-surface,
         * base-coordinate root that keeps the popover unclipped and correctly
         * sized.
         */
        static juce::Component* host_for(juce::Component& owner)
        {
            juce::Component* const top = owner.getTopLevelComponent();
            if (top == nullptr) {
                return nullptr;
            }

            juce::Component* scaled = nullptr;
            for (juce::Component* c = &owner; c != nullptr && c != top;
                    c = c->getParentComponent()) {
                if (!c->getTransform().isIdentity()) {
                    scaled = c;
                }
            }
            if (scaled != nullptr) {
                return scaled;
            }

            juce::Component* host = &owner;
            while (host->getParentComponent() != nullptr
                    && host->getParentComponent() != top) {
                host = host->getParentComponent();
            }
            return host;
        }

        /**
         * \brief Place `slot`'s popover above `anchor` (given in `owner`'s
         *        coordinates), hosted on the GUI root (see host_for) so it is
         *        never clipped and scales with the UI. Creates the popover on
         *        first use.
         */
        static void show(
                std::unique_ptr<ValuePopover>& slot,
                juce::Component& owner,
                juce::Rectangle<int> const anchor,
                juce::String title,
                juce::String value,
                juce::Colour accent
        ) {
            juce::Component* const top = host_for(owner);
            if (top == nullptr || top == &owner) {
                return;
            }

            if (slot == nullptr) {
                slot = std::make_unique<ValuePopover>();
            }
            slot->set_content(std::move(title), std::move(value), accent);
            slot->set_editing(false);

            juce::Point<int> const a = top->getLocalPoint(
                &owner, juce::Point<int>(anchor.getCentreX(), anchor.getY())
            );
            juce::Point<int> const a_bottom = top->getLocalPoint(
                &owner, juce::Point<int>(anchor.getCentreX(), anchor.getBottom())
            );
            juce::Rectangle<int> const box = slot->measure();
            int const w = box.getWidth();
            int const h = box.getHeight();
            int const px = juce::jlimit(0, juce::jmax(0, top->getWidth() - w), a.x - w / 2);

            /* Sit above the anchor by default; flip below (arrow pointing up) when
             * the bubble would run off the top of the host. */
            bool const flip = (a.y - 2 - h) < 0;
            int const py = flip ? (a_bottom.y + 2) : (a.y - 2 - h);

            if (slot->getParentComponent() != top) {
                top->addChildComponent(slot.get());
            }
            slot->set_arrow_up(flip);
            slot->set_arrow_x((float)(a.x - px));
            slot->setBounds(px, py, w, h);
            slot->toFront(false);
            slot->setVisible(true);
            slot->repaint();
        }

        static void hide(std::unique_ptr<ValuePopover>& slot)
        {
            if (slot != nullptr) {
                slot->setVisible(false);
            }
        }

        void paint(juce::Graphics& g) override
        {
            juce::Rectangle<float> bubble = getLocalBounds().toFloat().reduced(0.5f);
            if (arrow_up_) {
                bubble.removeFromTop(ARROW_H);
            } else {
                bubble.removeFromBottom(ARROW_H);
            }

            g.setColour(Theme::PANEL_2.withAlpha(0.96f));
            g.fillRoundedRectangle(bubble, 3.0f);
            g.setColour(accent_);
            g.drawRoundedRectangle(bubble, 3.0f, 1.0f);

            /* Pointer toward the anchor: down (bubble above) or up (bubble below). */
            juce::Path arrow;
            float const ax = juce::jlimit(bubble.getX() + 4.0f, bubble.getRight() - 4.0f, arrow_x_);
            if (arrow_up_) {
                arrow.addTriangle(ax - 3.0f, bubble.getY() + 0.5f,
                                  ax + 3.0f, bubble.getY() + 0.5f,
                                  ax, bubble.getY() - ARROW_H);
            } else {
                arrow.addTriangle(ax - 3.0f, bubble.getBottom() - 0.5f,
                                  ax + 3.0f, bubble.getBottom() - 0.5f,
                                  ax, bubble.getBottom() + ARROW_H);
            }
            g.fillPath(arrow);

            juce::Rectangle<float> text = bubble.reduced(4.0f, 2.0f);

            if (has_title()) {
                g.setColour(Theme::TEXT_DIM);
                g.setFont(title_font());
                g.drawText(title_, text.removeFromTop(LINE_H), juce::Justification::centred, false);
            }

            if (!editing_) {
                g.setColour(Theme::TEXT);
                g.setFont(value_font());
                g.drawText(value_, text, juce::Justification::centred, false);
            }
        }

    private:
        juce::String title_;
        juce::String value_;
        juce::Colour accent_ { juce::Colours::white };
        float arrow_x_ { 0.0f };
        bool arrow_up_ { false };
        bool editing_ { false };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ValuePopover)
};

}

#endif
