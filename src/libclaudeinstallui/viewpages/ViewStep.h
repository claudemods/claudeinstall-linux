/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014-2015 Teo Mrnjavac <teo@kde.org>
 *   SPDX-FileCopyrightText: 2017 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#ifndef VIEWSTEP_H
#define VIEWSTEP_H

#include "DllMacro.h"
#include "Job.h"
#include "Step.h"

#include "modulesystem/InstanceKey.h"
#include "modulesystem/Requirement.h"

#include <QList>
#include <QObject>
#include <QSize>
#include <QWidget>  // The return type of createSummaryWidget() is covariant with Step's QObject*

namespace ClaudeInstall
{

/**
 * @brief The ViewStep class is the base class for all view modules.
 * A view module is a ClaudeInstall module which has at least one UI page (exposed as
 * ViewStep::widget), and can optionally create ClaudeInstall jobs at runtime.
 * As of early 2020, a view module can be implemented by deriving from ViewStep
 * in C++ (as a Qt Plugin or a Qml ViewStep).
 *
 * A ViewStep is a Step (the frontend-neutral part: navigation, readiness,
 * jobs, requirements, descriptions) that is shown with Widgets. The Step
 * interface -- prettyName(), prettyStatus(), next(), back(), isNextEnabled(),
 * isBackEnabled(), isAtBeginning(), isAtEnd(), onActivate(), onLeave(),
 * jobs(), setConfigurationMap(), checkRequirements(), onCancel() and the
 * nextStatusChanged() signal -- is documented in Step.h.
 *
 * A ViewStep can describe itself in human-readable format for the SummaryPage
 * (which shows all of the things which have been collected to be done in the
 * next exec-step) through prettyStatus() and createSummaryWidget().
 */
class UIDLLEXPORT ViewStep : public Step
{
    Q_OBJECT
public:
    explicit ViewStep( QObject* parent = nullptr );
    ~ViewStep() override;

    /** @brief Return a long description what this step will do during install
     *
     * Optional. May return a widget which will be inserted in the summary
     * page. The caller takes ownership of the widget. Return nullptr to
     * provide no widget. In general, this is only used for complicated
     * steps where prettyStatus() is not sufficient.
     *
     * The default implementation returns nullptr, so nothing
     * will be displayed for this step when a summary is shown.
     */
    QWidget* createSummaryWidget() const override;

    /** @brief Get (or create) the widget for this view step
     *
     * While a view step **may** create the widget when it is loaded,
     * it is recommended to wait with widget creation until the
     * widget is actually asked for: a view step **may** be used
     * without a UI.
     */
    virtual QWidget* widget() = 0;

    /** @brief Get margins for this widget
     *
     * This is called by the layout manager to find the desired
     * margins (width is used for left and right margin, height is
     * used for top and bottom margins) for the widget. The
     * @p panelSides indicates where there are panels in the overall
     * layout: horizontally and / or vertically adjacent (or none!)
     * to the view step's widget.
     *
     * Should return a size based also on QStyle metrics for layout.
     * The default implementation just returns the default layout metrics
     * (often 11 pixels on a side).
     */
    virtual QSize widgetMargins( Qt::Orientations panelSides );

signals:
    /* Emitted when the viewstep thinks it needs more space than is currently
     * available for display. @p size is the requested space, that is needed
     * to display the entire page.
     *
     * This request may be silently ignored.
     */
    void ensureSize( QSize enlarge ) const;
};

using ViewStepList = QList< ViewStep* >;
}  // namespace ClaudeInstall

#endif  // VIEWSTEP_H
