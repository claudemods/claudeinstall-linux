/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017-2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef WIZARD_H
#define WIZARD_H

#include "DllMacro.h"
#include "Step.h"

#include <QAbstractListModel>
#include <QList>

namespace ClaudeInstall
{
/**
 * @brief The Wizard class handles progression through the steps.
 *
 * This is the frontend-neutral part of the ViewManager: the list of steps
 * (as a model), the current step, the state and labels of the next / back /
 * quit buttons, and the rules for moving between steps and for cancelling.
 * A frontend derives from it and overrides the hooks (asking the user
 * questions, showing the current step, quitting) to present it.
 */
class STEPDLLEXPORT Wizard : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY( int currentStepIndex READ currentStepIndex NOTIFY currentStepChanged FINAL )

    Q_PROPERTY( bool nextEnabled READ nextEnabled NOTIFY nextEnabledChanged FINAL )
    Q_PROPERTY( QString nextLabel READ nextLabel NOTIFY nextLabelChanged FINAL )
    Q_PROPERTY( QString nextIcon READ nextIcon NOTIFY nextIconChanged FINAL )

    Q_PROPERTY( bool backEnabled READ backEnabled NOTIFY backEnabledChanged FINAL )
    Q_PROPERTY( QString backLabel READ backLabel NOTIFY backLabelChanged FINAL )
    Q_PROPERTY( QString backIcon READ backIcon NOTIFY backIconChanged FINAL )

    Q_PROPERTY( bool quitEnabled READ quitEnabled NOTIFY quitEnabledChanged FINAL )
    Q_PROPERTY( QString quitLabel READ quitLabel NOTIFY quitLabelChanged FINAL )
    Q_PROPERTY( QString quitIcon READ quitIcon NOTIFY quitIconChanged FINAL )
    Q_PROPERTY( QString quitTooltip READ quitTooltip NOTIFY quitTooltipChanged FINAL )

    Q_PROPERTY( bool quitVisible READ quitVisible NOTIFY quitVisibleChanged FINAL )

    Q_PROPERTY( bool backAndNextVisible READ backAndNextVisible NOTIFY backAndNextVisibleChanged FINAL )

    // Global properties, where Wizard proxies to Settings
    Q_PROPERTY( bool isDebugMode READ isDebugMode CONSTANT FINAL )
    Q_PROPERTY( bool isChrootMode READ isChrootMode CONSTANT FINAL )
    Q_PROPERTY( bool isSetupMode READ isSetupMode CONSTANT FINAL )
    Q_PROPERTY( QString logFilePath READ logFilePath CONSTANT FINAL )

public:
    explicit Wizard( QObject* parent = nullptr );
    ~Wizard() override;

    /** @brief The wizard that is running the application
     *
     * This is the most recently created Wizard that still exists (an
     * application has one), or @c nullptr. Frontend-neutral code such as
     * the summary uses it to look at all the steps.
     */
    static Wizard* active() { return s_active; }

    /**
     * @brief addStep appends a step to the roster.
     * @param step a pointer to the Step object to add.
     * @note a Step is the active instance of a view module, it aggregates one
     * or more pages, plus zero or more jobs which may be created at runtime.
     */
    void addStep( Step* step );

    /**
     * @brief steps returns the list of currently present steps.
     * This should only return an empty list before startup is complete.
     */
    StepList steps() const { return m_steps; }

    /** @brief The currently active step, or @c nullptr */
    Step* currentStepBase() const;

    /** @brief currentStepIndex returns the index of the currently active Step. */
    int currentStepIndex() const { return m_currentStep; }

    enum class Confirmation
    {
        Continue,  // User rejects cancel / close question
        CancelInstallation,  // User accepts cancel / close question
        EndOfInstallation,  // There was no question because the installation was already done
    };

    /**
     * @brief Called when "Cancel" is clicked; asks for confirmation.
     * Other means of closing ClaudeInstall also call this method, e.g. alt-F4.
     * At the end of installation, no confirmation is asked.
     */
    Confirmation confirmCancelInstallation();

public Q_SLOTS:
    /**
     * @brief next moves forward to the next page of the current Step (if any),
     * or to the first page of the next Step if the current Step doesn't
     * have any more pages.
     */
    void next();
    bool nextEnabled() const
    {
        return m_nextEnabled;  ///< Is the next-button to be enabled
    }
    QString nextLabel() const
    {
        return m_nextLabel;  ///< What should be displayed on the next-button
    }
    QString nextIcon() const
    {
        return m_nextIcon;  ///< Name of the icon to show
    }

    /**
     * @brief back moves backward to the previous page of the current Step (if any),
     * or to the last page of the previous Step if the current Step doesn't
     * have any pages before the current one.
     */
    void back();
    bool backEnabled() const
    {
        return m_backEnabled;  ///< Is the back-button to be enabled
    }
    QString backLabel() const
    {
        return m_backLabel;  ///< What should be displayed on the back-button
    }
    QString backIcon() const
    {
        return m_backIcon;  ///< Name of the icon to show
    }

    bool backAndNextVisible() const
    {
        return m_backAndNextVisible;  ///< Are the back and next buttons to be shown
    }
    /**
     * @brief Probably quit
     *
     * Asks for confirmation if necessary. Terminates the application.
     */
    void quit();
    bool quitEnabled() const
    {
        return m_quitEnabled;  ///< Is the quit-button to be enabled
    }
    QString quitLabel() const
    {
        return m_quitLabel;  ///< What should be displayed on the quit-button
    }
    QString quitIcon() const
    {
        return m_quitIcon;  ///< Name of the icon to show
    }
    bool quitVisible() const
    {
        return m_quitVisible;  ///< Should the quit-button be visible
    }
    QString quitTooltip() const { return m_quitTooltip; }

    /**
     * @brief onInstallationFailed handles a fatal failure of the installation.
     * @param message the error string.
     * @param details the details string.
     *
     * Logs the failure, emits installationFailed() and calls showInstallationFailure().
     */
    void onInstallationFailed( const QString& message, const QString& details );

    /** @brief Tell the wizard that initialization / loading is complete.
     *
     * Call this at least once, to tell the wizard to activate the first step.
     */
    void onInitComplete();

    /// @brief Connected to Step::nextStatusChanged for all steps
    void updateNextStatus( bool enabled );

    /// @brief Proxy to Settings::debugMode() default @c false
    bool isDebugMode() const;
    /// @brief Proxy to Settings::doChroot() default @c true
    bool isChrootMode() const;
    /// @brief Proxy to Settings::isSetupMode() default @c false
    bool isSetupMode() const;
    /// @brief Proxy to Logger::logFile(), default empty
    QString logFilePath() const;

signals:
    void currentStepChanged();
    void cancelEnabled( bool enabled ) const;

    /// @brief The installation failed; a frontend can show the error
    void installationFailed( const QString& message, const QString& details );

    void nextEnabledChanged( bool ) const;
    void nextLabelChanged( QString ) const;
    void nextIconChanged( QString ) const;

    void backEnabledChanged( bool ) const;
    void backLabelChanged( QString ) const;
    void backIconChanged( QString ) const;
    void backAndNextVisibleChanged( bool ) const;

    void quitEnabledChanged( bool ) const;
    void quitLabelChanged( QString ) const;
    void quitIconChanged( QString ) const;
    void quitVisibleChanged( bool ) const;
    void quitTooltipChanged( QString ) const;

protected:
    /** @section Frontend hooks
     *
     * The default implementations suit a frontend that asks nothing:
     * they accept everything, and quit the application.
     */

    /// @brief A step was inserted at @p index (while the model is updating)
    virtual void stepInserted( int index, Step* step );
    /// @brief The step at @p index is now the one to show
    virtual void stepShown( int index );
    /** @brief Asks the user if the *exec* phase should start (if the settings want that)
     *  @return @c true to continue
     */
    virtual bool confirmExecution();
    /** @brief Asks the user if the installation should be cancelled
     *  @return @c true to cancel
     */
    virtual bool confirmCancel();
    /// @brief Ends the application
    virtual void exitApplication( int exitCode );
    /// @brief Tells the user about the failed installation; the default exits with a failure code
    virtual void showInstallationFailure( const QString& message, const QString& details );

    void insertStep( int before, Step* step );
    void updateButtonLabels();
    void updateCancelEnabled( bool enabled );
    void updateBackAndNextVisibility( bool visible );

    inline bool currentStepValid() const { return ( 0 <= m_currentStep ) && ( m_currentStep < m_steps.length() ); }

    StepList m_steps;
    int m_currentStep;

private:
    static Wizard* s_active;

    bool m_nextEnabled = false;
    QString m_nextLabel;
    QString m_nextIcon;  ///< Name of icon to show on button

    bool m_backEnabled = false;
    QString m_backLabel;
    QString m_backIcon;

    bool m_backAndNextVisible = true;

    bool m_quitEnabled = false;
    QString m_quitLabel;
    QString m_quitIcon;
    QString m_quitTooltip;
    bool m_quitVisible = true;

public:
    /** @section Model
     *
     * These are the methods and enums used for the as-a-model part
     * of the Wizard.
     */
    enum Role
    {
        ProgressTreeItemCurrentIndex = Qt::UserRole + 13  ///< Index (row) of the current step
    };

    QVariant data( const QModelIndex& index, int role = Qt::DisplayRole ) const override;
    int rowCount( const QModelIndex& parent = QModelIndex() ) const override;
};

}  // namespace ClaudeInstall

#endif  // WIZARD_H
