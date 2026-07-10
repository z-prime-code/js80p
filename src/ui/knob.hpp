/*
 * This file is part of JS80P, a synthesizer plugin.
 * Copyright (C) 2023, 2024, 2025, 2026  Attila M. Magyar
 *
 * JS80P is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * JS80P is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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
