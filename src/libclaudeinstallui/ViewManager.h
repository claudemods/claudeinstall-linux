/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017-2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef VIEWMANAGER_H
#define VIEWMANAGER_H

#include "DllMacro.h"
#include "Wizard.h"
#include "viewpages/ViewStep.h"

#include <QAbstractListModel>
#include <QList>
#include <QPushButton>
#include <QStackedWidget>

namespace ClaudeInstall
{
/**
 * @brief The ViewManager class handles progression through view pages.
 * @note Singleton object, only use through ViewManager::instance().
 *
 * The ViewManager is the Widgets frontend of the Wizard: it inherits the
 * navigation logic, the button state and the model of steps from the
 * Wizard, and adds the stack of widgets and the dialogs.
 */
class UIDLLEXPORT ViewManager : public Wizard
{
    Q_OBJECT

    ///@brief Sides on which the ViewManager has side-panels
    Q_PROPERTY( Qt::Orientations panelSides READ panelSides WRITE setPanelSides MEMBER m_panelSides )

public:
    /**
     * @brief instance access to the ViewManager singleton.
     * @return pointer to the singleton instance.
     */
    static ViewManager* instance();
    static ViewManager* instance( QObject* parent );

    /**
     * @brief centralWidget always returns the central widget in the ClaudeInstall main
     * window.
     * @return a pointer to the active QWidget (usually a wizard page provided by a
     * view module).
     */
    QWidget* centralWidget();

    /**
     * @brief addViewStep appends a view step to the roster.
     * @param step a pointer to the ViewStep object to add.
     * @note a ViewStep is the active instance of a view module, it aggregates one
     * or more view pages, plus zero or more jobs which may be created at runtime.
     */
    void addViewStep( ViewStep* step );

    /**
     * @brief viewSteps returns the list of currently present view steps.
     * @return the ViewStepList.
     * This should only return an empty list before startup is complete.
     */
    ViewStepList viewSteps() const;

    /**
     * @brief currentStep returns the currently active ViewStep, i.e. the ViewStep
     * which owns the currently visible view page.
     * @return the active ViewStep. Do not confuse this with centralWidget().
     * @see ViewStep::centralWidget
     */
    ViewStep* currentStep() const;

    Qt::Orientations panelSides() const { return m_panelSides; }
    void setPanelSides( Qt::Orientations panelSides ) { m_panelSides = panelSides; }

public Q_SLOTS:
    /** @brief Replaces the stack with a view step stating that initialization failed.
     *
     * @param modules a list of failed modules.
     */
    void onInitFailed( const QStringList& modules );

signals:
    void ensureSize( QSize size ) const;  // See ViewStep::ensureSize()

protected:
    void stepInserted( int index, Step* step ) override;
    void stepShown( int index ) override;
    bool confirmExecution() override;
    bool confirmCancel() override;
    void exitApplication( int exitCode ) override;
    void showInstallationFailure( const QString& message, const QString& details ) override;

private:
    explicit ViewManager( QObject* parent = nullptr );
    ~ViewManager() override;

    static ViewManager* s_instance;

    QWidget* m_widget;
    QStackedWidget* m_stack;

    Qt::Orientations m_panelSides;
};

}  // namespace ClaudeInstall

#endif  // VIEWMANAGER_H
