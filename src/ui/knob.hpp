#ifndef JS80P__UI__KNOB_HPP
#define JS80P__UI__KNOB_HPP

/* Backwards-compatible alias: the rotary knob is now one render style of the
 * unified Control component (src/ui/control.hpp). Existing call sites that spell
 * the type "Knob" keep working. */
#include "ui/control.hpp"

namespace JS80P
{
    using Knob = Control;
}

#endif
