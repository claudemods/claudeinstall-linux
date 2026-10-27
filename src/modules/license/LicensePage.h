/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2015 Anke Boersma <demm@kaosx.us>
 *   SPDX-FileCopyrightText: 2015 Alexandre Arnt <qtgzmanager@gmail.com>
 *   SPDX-FileCopyrightText: 2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef LICENSEPAGE_H
#define LICENSEPAGE_H

#include "Config.h"  // For LicenseEntry as well

#include <QWidget>

namespace Ui
{
class LicensePage;
}  // namespace Ui

class LicenseWidget;

class LicensePage : public QWidget
{
    Q_OBJECT
public:
    explicit LicensePage( Config* config, QWidget* parent = nullptr );

    /// @brief Shows the licenses from the Config (replacing those shown before)
    void loadEntries();

    bool isNextEnabled() const;

public slots:
    /** @brief Check if the user can continue
     *
     * The user can continue if
     *  - none of the licenses are required, or
     *  - the user has ticked the "OK" box.
     * This function tells the Config (which updates the global storage
     * as needed), and updates the appearance of the page as needed.
     * @p checked indicates whether the checkbox has been ticked or not.
     * (e.g. when @p checked is true, you can continue regardless)
     */
    void checkAcceptance( bool checked );

    void retranslate();

private:
    Config* m_config;

    Ui::LicensePage* ui;
    QList< LicenseWidget* > m_entries;
};

#endif  //LICENSEPAGE_H
