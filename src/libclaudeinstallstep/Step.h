/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef STEP_H
#define STEP_H

#include "DllMacro.h"
#include "Job.h"

#include "modulesystem/InstanceKey.h"
#include "modulesystem/Requirement.h"

#include <QList>
#include <QObject>
#include <QString>
#include <QVariantMap>

namespace ClaudeInstall
{

/**
 * @brief The Step class is the frontend-neutral part of a view module.
 *
 * A step is one stop of the wizard: it knows how to navigate (next / back,
 * multiple pages), whether it is ready, what it will do during the install
 * (prettyStatus() / summaryText()), and which jobs it creates. It knows nothing
 * about how it is shown. Frontends add that: the Widgets / QML frontend uses
 * ViewStep (which adds widget()), a text-mode frontend binds its own pages to
 * the step's config().
 *
 * Steps are driven by a Wizard.
 */
class STEPDLLEXPORT Step : public QObject
{
    Q_OBJECT
public:
    explicit Step( QObject* parent = nullptr );
    ~Step() override;

    /** @brief Human-readable name of the step
     *
     * This (translated) string is shown in the sidebar (progress)
     * and during installation. There is no default.
     */
    virtual QString prettyName() const = 0;

    /** @brief Describe what this step will do during install
     *
     * Optional. May return a non-empty string describing what this
     * step is going to do (should be translated). This is also used
     * in the summary page to describe what is going to be done.
     * Return an empty string to provide no description.
     *
     * The default implementation returns an empty string, so nothing
     * will be displayed for this step when a summary is shown.
     */
    virtual QString prettyStatus() const;

    /** @brief A longer, plain-text description of what this step will do
     *
     * This is the frontend-neutral replacement for a summary widget, for
     * steps where prettyStatus() is not sufficient. Return an empty string
     * to provide no description. The default implementation returns prettyStatus().
     */
    virtual QString summaryText() const;

    /** @brief A long description of what this step will do, as an object to show
     *
     * Optional. Returns an object that frontends know how to show in the
     * summary (the Widgets frontend returns a QWidget, through ViewStep,
     * which overrides this with a more specific return type). The caller
     * takes ownership. Return @c nullptr to provide none. This is only
     * used for complicated steps where prettyStatus() / summaryText()
     * are not sufficient.
     *
     * The default implementation returns @c nullptr.
     */
    virtual QObject* createSummaryWidget() const;

    /** @brief The configuration object of this step, if it has one
     *
     * The Config is a QObject with properties and signals that a frontend
     * can bind to. The step keeps ownership. The default implementation
     * returns @c nullptr.
     */
    virtual QObject* config();

    /**
     * @brief Multi-page support, go next
     *
     * Multi-page steps need to manage the content shown themselves.
     * This method is called when the user chooses *next*, and should switch
     * to the next of the multiple pages. It needs to be consistent with
     * both isNextEnabled() and isAtEnd().
     *
     * In particular: when isAtEnd() returns false, next() is called when
     * the user chooses next and a new page should be shown by this step.
     * When isAtEnd() returns true, choosing next will switch to the next
     * step in sequence, rather than a next page in the current step.
     */
    virtual void next();
    /// @brief Multi-page support, go back
    virtual void back();

    /// @brief Can the user go *next* with currently-filled-in data?
    virtual bool isNextEnabled() const = 0;
    /// @brief Can the user go *back* with currently-filled-in data?
    virtual bool isBackEnabled() const = 0;

    /**
     * @brief Multi-page support, switch to previous step?
     *
     * For a multi-page step, this indicates that the first (beginning)
     * page is showing. Going *back* when at the beginning of a
     * step, switches to the previous step, not the previous page of the
     * current step.
     */
    virtual bool isAtBeginning() const = 0;
    /// @brief Multi-page support, switch to next step?
    virtual bool isAtEnd() const = 0;

    /**
     * @brief onActivate called every time a Step is shown, either by going forward
     * or backward.
     * The default implementation does nothing.
     */
    virtual void onActivate();

    /**
     * @brief onLeave called every time a Step is hidden and control passes to
     * another Step, either by going forward or backward.
     * The default implementation does nothing.
     */
    virtual void onLeave();

    /**
     * @brief Jobs needed to run this step
     *
     * When a Step is listed in the exec section, its jobs are executed instead.
     * This function returns that list of jobs; an empty list is ok.
     */
    virtual JobList jobs() const = 0;

    void setModuleInstanceKey( const ClaudeInstall::ModuleSystem::InstanceKey& instanceKey );
    ClaudeInstall::ModuleSystem::InstanceKey moduleInstanceKey() const { return m_instanceKey; }

    virtual void setConfigurationMap( const QVariantMap& configurationMap );

    /**
     * @brief Can this module proceed, on this machine?
     *
     * This is called asynchronously at startup, and returns a list of
     * the requirements that the module has checked, and their status.
     * See ClaudeInstall::RequirementEntry for details.
     */
    virtual RequirementsList checkRequirements();

    /**
     * @brief Called when the user cancels the installation
     *
     * Steps are expected to leave the system unchanged when
     * the installation is cancelled. The default implementation
     * does nothing.
     */
    virtual void onCancel();

    /**
     * @brief Is this the step that runs the jobs (the *exec* phase)?
     *
     * The wizard handles the step specially (confirmation before it,
     * buttons during it). The default implementation returns @c false.
     */
    virtual bool isExecutionStep() const;

signals:
    /// @brief Tells the wizard to enable the *next* button according to @p status
    void nextStatusChanged( bool status );

protected:
    ClaudeInstall::ModuleSystem::InstanceKey m_instanceKey;
};

using StepList = QList< Step* >;

}  // namespace ClaudeInstall

#endif  // STEP_H
