/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef TUIWIDGETS_H
#define TUIWIDGETS_H

#include "DllMacro.h"

#include <ftxui/component/component.hpp>
#include <ftxui/dom/elements.hpp>

#include <QAbstractItemModel>
#include <QString>

#include <functional>
#include <string>
#include <vector>

/**
 * @file
 * @brief Small building blocks for the pages of text-mode steps
 *
 * The pages of different modules look alike: a title, labelled text fields,
 * check boxes, some paragraphs of explanation, and messages about what is
 * wrong. These helpers give them the same look. They only know about
 * FTXUI, not about any module.
 */

namespace ClaudeInstall
{
namespace Tui
{

/// @brief A QString for FTXUI
inline std::string
str( const QString& s )
{
    return s.toStdString();
}

/** @brief Is this a plain Linux console (TERM=linux) or a terminal that is not UTF-8?
 *
 * There the font has few glyphs: use ASCII symbols and Latin text only
 * (see symbol() and drawableOnConsole()). In a terminal emulator it is @c false.
 */
TUIDLLEXPORT bool isConsole();

/// @brief A status symbol with spaces around it: a check mark, a cross or an exclamation mark (ASCII on the console)
TUIDLLEXPORT std::string symbol( bool ok, bool warning = false );

/// @brief Can the Linux console font show @p s? (Latin scripts only)
TUIDLLEXPORT bool drawableOnConsole( const QString& s );

/** @brief Turns the little HTML that messages contain into plain text
 *
 * Line breaks (`<br/>`) and the ends of list items and paragraphs become
 * newlines, other tags are dropped and the common entities are decoded.
 */
TUIDLLEXPORT QString stripMarkup( const QString& s );

/// @brief A paragraph (wrapped text) from a QString; each line (separated by newlines) is a paragraph of its own
TUIDLLEXPORT ftxui::Element paragraphOf( const QString& s );

/// @brief A bold title above a separator
TUIDLLEXPORT ftxui::Element titled( const QString& title, ftxui::Element body );

/// @brief A message that something is wrong (red) or is only a warning (yellow); empty for @p message empty
TUIDLLEXPORT ftxui::Element problem( const QString& message, bool warning = false );

/** @brief A text field with a label and a message about its content
 *
 * The component takes keyboard focus like any input. @p value is the text being
 * edited and must outlive the component. @p onChange is called after each change
 * of the text. @p status, if given, is asked on each redraw for the message to show
 * under the field (empty for none); it is shown as a problem.
 *
 * With @p password the text is hidden.
 *
 * The field is *not* updated when the underlying setting changes: if the setting can
 * change by itself (e.g. a login name guessed from the full name), the page
 * should copy the setting to @p value when the field is not focused
 * (ftxui::ComponentBase::Focused()).
 */
TUIDLLEXPORT ftxui::Component field( const QString& label,
                                     std::string* value,
                                     bool password,
                                     const std::function< void() >& onChange,
                                     const std::function< QString() >& status = {} );

/// @brief Makes the space bar act like Enter on @p component (menus only know Enter)
TUIDLLEXPORT ftxui::Component acceptsSpace( ftxui::Component component );

/// @brief A check box; @p onChange is called after the box is toggled
TUIDLLEXPORT ftxui::Component check( const QString& label, bool* value, const std::function< void() >& onChange );

/** @brief A titled, scrolling list to pick one entry from
 *
 * @p entries and @p selected must outlive the component; the list may be
 * changed (replaced) at any time. @p onChange is called when the user moves
 * to another entry. The title is highlighted while the list has the focus.
 * Enter or space chooses the entry: @p onEnter is called, or, if there is none,
 * the focus moves to the next field.
 */
TUIDLLEXPORT ftxui::Component picker( const QString& title,
                                      std::vector< std::string >* entries,
                                      int* selected,
                                      const std::function< void() >& onChange,
                                      const std::function< void() >& onEnter = {} );

/** @brief Fills @p entries with the text (@p role) of the rows of a list @p model
 *
 * @return @c true if the entries are different afterwards
 */
TUIDLLEXPORT bool fillEntries( std::vector< std::string >* entries, const QAbstractItemModel* model, int role = Qt::DisplayRole );

/// @brief A page of content that is taller than the screen: scrolls with the arrow and page keys
TUIDLLEXPORT ftxui::Component scrolled( const std::function< ftxui::Element() >& content );

}  // namespace Tui
}  // namespace ClaudeInstall

#endif
