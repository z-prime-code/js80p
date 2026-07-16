/*
 * This file is part of JS80P, a synthesizer plugin.
 * Copyright (C) 2026  Attila M. Magyar
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

#ifndef JS80P__PLUGIN__VST3__JUCE_GUI_HPP
#define JS80P__PLUGIN__VST3__JUCE_GUI_HPP


namespace JS80P
{

/**
 * \brief The JUCE-free seam between the hand-rolled VST3 wrapper and the JUCE
 *        editor, so that plugin.cpp keeps compiling against the VST3 SDK alone.
 *        Everything that needs a JUCE header lives in plugin-juce.cpp, behind
 *        these functions and the \c void* the wrapper carries around.
 *
 *        Only compiled into the JS80P_JUCE_GUI build; the stock build renders
 *        the original GUI through plugin-xcb.cpp / -win32.cpp / -macos.cpp.
 */
namespace JuceGui
{
    /**
     * \brief Start up JUCE's library runtime and pin the caches that are shared
     *        between editors (see GlyphSheet) for as long as the returned handle
     *        is held. Refcounted, so holding several handles is cheap; the last
     *        one released shuts JUCE down. Call on the host's UI thread.
     */
    void* create_runtime();

    /** \brief Release a handle obtained from \c create_runtime(). */
    void destroy_runtime(void* const runtime);

    /** \brief The size a freshly opened editor wants to be. */
    void preferred_size(int& width, int& height);
}

}

#endif
