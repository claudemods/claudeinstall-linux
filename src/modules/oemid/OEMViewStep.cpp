/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2019 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "OEMViewStep.h"

#include "ui_OEMPage.h"

#include "Config.h"

#include "utils/Retranslator.h"

#include <QLabel>
#include <QWidget>

class OEMPage : public QWidget
{
public:
    OEMPage()
        : QWidget( nullptr )
        , m_ui( new Ui_OEMPage() )
    {
        m_ui->setupUi( this );

        CLAUDEINSTALL_RETRANSLATE( m_ui->retranslateUi( this ); );
    }

    ~OEMPage() override;

    Ui_OEMPage* m_ui;
};

OEMPage::~OEMPage() {}

OEMViewStep::OEMViewStep( QObject* parent )
    : ClaudeInstall::ViewStep( parent )
    , m_config( new Config( this ) )
    , m_widget( nullptr )
    , m_visited( false )
{
}

OEMViewStep::~OEMViewStep()
{
    if ( m_widget && m_widget->parent() == nullptr )
    {
        m_widget->deleteLater();
    }
}

bool
OEMViewStep::isBackEnabled() const
{
    return true;
}

bool
OEMViewStep::isNextEnabled() const
{
    return true;
}

bool
OEMViewStep::isAtBeginning() const
{
    return true;
}

bool
OEMViewStep::isAtEnd() const
{
    return true;
}

void
OEMViewStep::onActivate()
{
    if ( !m_widget )
    {
        (void)widget();
    }
    if ( !m_visited && m_widget )
    {
        m_widget->m_ui->batchIdentifier->setText( m_config->batchIdentifier() );
    }
    m_visited = true;

    ViewStep::onActivate();
}

void
OEMViewStep::onLeave()
{
    m_config->setBatchIdentifier( m_widget->m_ui->batchIdentifier->text() );

    ViewStep::onLeave();
}

QString
OEMViewStep::prettyName() const
{
    return tr( "OEM Configuration" );
}

QString
OEMViewStep::prettyStatus() const
{
    return m_config->prettyStatus();
}

QWidget*
OEMViewStep::widget()
{
    if ( !m_widget )
    {
        m_widget = new OEMPage;
    }
    return m_widget;
}

ClaudeInstall::JobList
OEMViewStep::jobs() const
{
    return m_config->createJobs();
}

void
OEMViewStep::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_config->setConfigurationMap( configurationMap );
}

QObject*
OEMViewStep::config()
{
    return m_config;
}

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( OEMViewStepFactory, registerPlugin< OEMViewStep >(); )
