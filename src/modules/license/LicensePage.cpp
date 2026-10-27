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

#include "LicensePage.h"

#include "LicenseWidget.h"
#include "ui_LicensePage.h"

#include "GlobalStorage.h"
#include "JobQueue.h"
#include "ViewManager.h"

#include "utils/Gui.h"
#include "utils/Logger.h"
#include "utils/NamedEnum.h"
#include "utils/Retranslator.h"
#include "utils/Variant.h"

#include <QApplication>
#include <QBoxLayout>
#include <QComboBox>
#include <QDesktopServices>
#include <QFocusEvent>
#include <QLabel>

#include <algorithm>

static const char mustAccept[] = "#acceptFrame { border: 1px solid red;"
                                 "background-color: #fff6f6;"
                                 "border-radius: 4px;"
                                 "padding: 2px; }";
static const char okAccept[] = "#acceptFrame { padding: 3px }";

LicensePage::LicensePage( Config* config, QWidget* parent )
    : QWidget( parent )
    , m_config( config )
    , ui( new Ui::LicensePage )
{
    ui->setupUi( this );

    // ui->verticalLayout->insertSpacing( 1, ClaudeInstall::defaultFontHeight() );
    ClaudeInstall::unmarginLayout( ui->verticalLayout );

    ui->acceptFrame->setStyleSheet( mustAccept );
    {
        // The inner frame was unmargined (above), reinstate margins so all are
        // the same *x* (an x-height, approximately).
        const auto x = ClaudeInstall::defaultFontHeight() / 2;
        ui->acceptFrame->layout()->setContentsMargins( x, x, x, x );
    }

    connect( ui->acceptCheckBox, &QCheckBox::toggled, this, &LicensePage::checkAcceptance );

    CLAUDEINSTALL_RETRANSLATE_SLOT( &LicensePage::retranslate );
}

void
LicensePage::loadEntries()
{
    for ( QWidget* w : m_entries )
    {
        ui->licenseEntriesLayout->removeWidget( w );
        w->deleteLater();
    }

    const auto entriesList = m_config->entries();

    m_entries.clear();
    m_entries.reserve( entriesList.count() );
    for ( const LicenseEntry& entry : entriesList )
    {
        LicenseWidget* w = new LicenseWidget( entry );
        ui->licenseEntriesLayout->addWidget( w );
        m_entries.append( w );
    }

    ui->acceptCheckBox->setChecked( false );
    checkAcceptance( false );
}

void
LicensePage::retranslate()
{
    ui->acceptCheckBox->setText( tr( "I accept the terms and conditions above.", "@info" ) );

    QString review = tr( "Please review the End User License Agreements (EULAs).", "@info" );
    const auto br = QStringLiteral( "<br/>" );

    if ( !m_config->allLicensesOptional() )
    {
        ui->mainText->setText( tr( "This setup procedure will install proprietary "
                                   "software that is subject to licensing terms.",
                                   "@info" )
                               + br + review );
        QString mustAcceptText(
            tr( "If you do not agree with the terms, the setup procedure cannot continue.", "@info" ) );
        ui->acceptCheckBox->setToolTip( mustAcceptText );
    }
    else
    {
        ui->mainText->setText( tr( "This setup procedure can install proprietary "
                                   "software that is subject to licensing terms "
                                   "in order to provide additional features and enhance the user "
                                   "experience.",
                                   "@info" )
                               + br + review );
        QString okAcceptText( tr( "If you do not agree with the terms, proprietary software will not "
                                  "be installed, and open source alternatives will be used instead.",
                                  "@info" ) );
        ui->acceptCheckBox->setToolTip( okAcceptText );
    }
    ui->retranslateUi( this );

    for ( const auto& w : m_entries )
    {
        w->retranslateUi();
    }
}

bool
LicensePage::isNextEnabled() const
{
    return m_config->isNextEnabled();
}

void
LicensePage::checkAcceptance( bool checked )
{
    m_config->setAccepted( checked );  // Also updates the global storage

    if ( !m_config->isNextEnabled() )
    {
        ui->acceptFrame->setStyleSheet( mustAccept );
    }
    else
    {
        ui->acceptFrame->setStyleSheet( okAccept );
    }
}
