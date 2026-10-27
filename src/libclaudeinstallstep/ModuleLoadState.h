/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2017-2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef MODULELOADSTATE_H
#define MODULELOADSTATE_H

#include "DllMacro.h"

#include "modulesystem/RequirementsModel.h"

#include <QObject>
#include <QPointer>

namespace ClaudeInstall
{

/**
 * @brief What the module loader tells everyone else about start-up
 *
 * Config objects need two things from the module system: the global
 * requirements model (to show what the computer lacks) and a notice when
 * all modules have been loaded (to start things that need every module's
 * configuration, such as GeoIP lookups). Whatever loads the modules -- the
 * ModuleManager of the Widgets frontend, or the loader of another
 * frontend -- publishes both here, so Config objects do not depend
 * on a particular loader.
 */
class STEPDLLEXPORT ModuleLoadState : public QObject
{
    Q_OBJECT
public:
    static ModuleLoadState* instance();

    /// @brief The global requirements model, or @c nullptr if no loader has published one
    RequirementsModel* requirementsModel() const { return m_requirementsModel; }
    void setRequirementsModel( RequirementsModel* model ) { m_requirementsModel = model; }

    /// @brief Have all modules been loaded?
    bool isLoaded() const { return m_loaded; }

public slots:
    /// @brief The loader calls this when it has loaded all the modules
    void notifyModulesLoaded();

signals:
    void modulesLoaded();
    /// @brief The loader finished checking requirements; @p satisfied says if the mandatory ones are
    void requirementsComplete( bool satisfied );

private:
    explicit ModuleLoadState( QObject* parent = nullptr );

    QPointer< RequirementsModel > m_requirementsModel;
    bool m_loaded = false;
};

}  // namespace ClaudeInstall

#endif
