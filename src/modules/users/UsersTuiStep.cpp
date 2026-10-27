/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "UsersTuiStep.h"

#include "TuiWidgets.h"

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( UsersTuiStepFactory, registerPlugin< UsersTuiStep >(); )

using ClaudeInstall::Tui::check;
using ClaudeInstall::Tui::field;

static QString
q( const std::string& s )
{
    return QString::fromStdString( s );
}

UsersTuiStep::UsersTuiStep( QObject* parent )
    : ClaudeInstall::TuiStep( parent )
    , m_config( new Config( this ) )
{
    connect( m_config, &Config::readyChanged, this, &UsersTuiStep::nextStatusChanged );
}

UsersTuiStep::~UsersTuiStep() {}

QString
UsersTuiStep::prettyName() const
{
    return tr( "Users" );
}

QObject*
UsersTuiStep::config()
{
    return m_config;
}

bool
UsersTuiStep::isNextEnabled() const
{
    return m_config->isReady();
}

bool
UsersTuiStep::isBackEnabled() const
{
    return true;
}

bool
UsersTuiStep::isAtBeginning() const
{
    return true;
}

bool
UsersTuiStep::isAtEnd() const
{
    return true;
}

ClaudeInstall::JobList
UsersTuiStep::jobs() const
{
    return m_config->createJobs();
}

void
UsersTuiStep::onLeave()
{
    m_config->finalizeGlobalStorage();
}

void
UsersTuiStep::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_config->setConfigurationMap( configurationMap );
    ClaudeInstall::TuiStep::setConfigurationMap( configurationMap );
}

ftxui::Component
UsersTuiStep::createComponent()
{
    using namespace ftxui;

    // What the configuration starts with (e.g. from the configuration file)
    m_fullName = m_config->fullName().toStdString();
    m_loginName = m_config->loginName().toStdString();
    m_hostName = m_config->hostname().toStdString();
    m_password = m_config->userPassword().toStdString();
    m_passwordAgain = m_config->userPasswordSecondary().toStdString();
    m_autoLogin = m_config->doAutoLogin();
    m_reuseForRoot = m_config->reuseUserPasswordForRoot();
    m_requireStrong = m_config->requireStrongPasswords();

    const bool hostnameShown = m_config->hostnameAction() == HostNameAction::EtcHostname
        || m_config->hostnameAction() == HostNameAction::SystemdHostname;
    const bool rootWritten = m_config->writeRootPassword();
    m_showRoot = rootWritten && !m_reuseForRoot;

    Components fields;
    fields.push_back( field( tr( "Your name", "@label" ),
                             &m_fullName,
                             false,
                             [ this ] { m_config->setFullName( q( m_fullName ) ); } ) );
    Component login = field( tr( "Login name", "@label" ),
                             &m_loginName,
                             false,
                             [ this ] { m_config->setLoginName( q( m_loginName ) ); },
                             [ this ] { return m_config->loginNameStatus(); } );
    fields.push_back( login );

    Component host;
    if ( hostnameShown )
    {
        host = field( tr( "Computer name", "@label" ),
                      &m_hostName,
                      false,
                      [ this ] { m_config->setHostName( q( m_hostName ) ); },
                      [ this ] { return m_config->hostnameStatus(); } );
        fields.push_back( host );
    }

    fields.push_back( field( tr( "Password", "@label" ),
                             &m_password,
                             true,
                             [ this ] { m_config->setUserPassword( q( m_password ) ); } ) );
    fields.push_back( field( tr( "Repeat password", "@label" ),
                             &m_passwordAgain,
                             true,
                             [ this ] { m_config->setUserPasswordSecondary( q( m_passwordAgain ) ); },
                             [ this ] { return m_config->userPasswordMessage(); } ) );

    if ( m_config->displayAutoLogin() )
    {
        fields.push_back(
            check( tr( "Log in automatically without asking for the password", "@label" ),
                   &m_autoLogin,
                   [ this ] { m_config->setAutoLogin( m_autoLogin ); } ) );
    }

    if ( rootWritten )
    {
        fields.push_back( check( tr( "Use the same password for the administrator account", "@label" ),
                                 &m_reuseForRoot,
                                 [ this ]
                                 {
                                     m_config->setReuseUserPasswordForRoot( m_reuseForRoot );
                                     m_showRoot = !m_reuseForRoot;
                                 } ) );
        Component rootFields = Container::Vertical(
            { field( tr( "Administrator password", "@label" ),
                     &m_rootPassword,
                     true,
                     [ this ] { m_config->setRootPassword( q( m_rootPassword ) ); } ),
              field( tr( "Repeat administrator password", "@label" ),
                     &m_rootPasswordAgain,
                     true,
                     [ this ] { m_config->setRootPasswordSecondary( q( m_rootPasswordAgain ) ); },
                     [ this ] { return m_config->rootPasswordMessage(); } ) } );
        fields.push_back( Maybe( rootFields, &m_showRoot ) );
    }

    if ( m_config->permitWeakPasswords() )
    {
        fields.push_back( check( tr( "Require strong passwords", "@label" ),
                                 &m_requireStrong,
                                 [ this ] { m_config->setRequireStrongPasswords( m_requireStrong ); } ) );
    }

    Component form = Container::Vertical( fields );
    return Renderer( form,
                     [ this, form, login, host ]
                     {
                         // The login and host names follow what is typed in the name field
                         if ( !login->Focused() )
                         {
                             m_loginName = m_config->loginName().toStdString();
                         }
                         if ( host && !host->Focused() )
                         {
                             m_hostName = m_config->hostname().toStdString();
                         }
                         return ClaudeInstall::Tui::titled( tr( "Create your account", "@title" ),
                                                            vbox( { form->Render(),
                                                                    filler(),
                                                                    text( tr( "Up and down move between the fields.", "@label" )
                                                                              .toStdString() )
                                                                        | dim } ) );
                     } );
}
