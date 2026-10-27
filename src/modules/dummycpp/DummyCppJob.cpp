/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Teo Mrnjavac <teo@kde.org> (original dummypython code)
 *   SPDX-FileCopyrightText: 2016 Kevin Kofler <kevin.kofler@chello.at>
 *   SPDX-FileCopyrightText: 2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "DummyCppJob.h"

#include <QDateTime>
#include <QThread>

#include "ClaudeInstallVersion.h"
#include "GlobalStorage.h"
#include "JobQueue.h"

#include "compat/Variant.h"
#include "utils/Logger.h"
#include "utils/System.h"

DummyCppJob::DummyCppJob( QObject* parent )
    : ClaudeInstall::CppJob( parent )
{
}

DummyCppJob::~DummyCppJob() {}

QString
DummyCppJob::prettyName() const
{
    return tr( "Performing dummy C++ job…", "@status" );
}

static QString variantListToString( const QVariantList& variantList );
static QString variantMapToString( const QVariantMap& variantMap );
static QString variantHashToString( const QVariantHash& variantHash );

static QString
variantToString( const QVariant& variant )
{
    if ( ClaudeInstall::typeOf( variant ) == ClaudeInstall::MapVariantType )
    {
        return variantMapToString( variant.toMap() );
    }
    else if ( ClaudeInstall::typeOf( variant ) == ClaudeInstall::HashVariantType )
    {
        return variantHashToString( variant.toHash() );
    }
    else if ( ( ClaudeInstall::typeOf( variant ) == ClaudeInstall::ListVariantType )
              || ( ClaudeInstall::typeOf( variant ) == ClaudeInstall::StringListVariantType ) )
    {
        return variantListToString( variant.toList() );
    }
    else
    {
        return variant.toString();
    }
}

static QString
variantListToString( const QVariantList& variantList )
{
    QStringList result;
    for ( const QVariant& variant : variantList )
    {
        result.append( variantToString( variant ) );
    }
    return '{' + result.join( ',' ) + '}';
}

static QString
variantMapToString( const QVariantMap& variantMap )
{
    QStringList result;
    for ( auto it = variantMap.constBegin(); it != variantMap.constEnd(); ++it )
    {
        result.append( it.key() + '=' + variantToString( it.value() ) );
    }
    return '[' + result.join( ',' ) + ']';
}

static QString
variantHashToString( const QVariantHash& variantHash )
{
    QStringList result;
    for ( auto it = variantHash.constBegin(); it != variantHash.constEnd(); ++it )
    {
        result.append( it.key() + '=' + variantToString( it.value() ) );
    }
    return '<' + result.join( ',' ) + '>';
}

ClaudeInstall::JobResult
DummyCppJob::exec()
{
    // Ported from dummypython
    ClaudeInstall::System::runCommand( ClaudeInstall::System::RunLocation::RunInHost,
                                   QStringList() << "/bin/sh"
                                                 << "-c"
                                                 << "touch ~/claudeinstall-dummycpp" );
    QString accumulator = QDateTime::currentDateTimeUtc().toString( Qt::ISODate ) + '\n';
    accumulator += QStringLiteral( "ClaudeInstall version: " ) + CLAUDEINSTALL_VERSION_SHORT + '\n';
    accumulator += QStringLiteral( "This job's name: " ) + prettyName() + '\n';
    accumulator += QStringLiteral( "Configuration map: %1\n" ).arg( variantMapToString( m_configurationMap ) );
    accumulator += QStringLiteral( "   *** globalstorage test ***\n" );
    ClaudeInstall::GlobalStorage* globalStorage = ClaudeInstall::JobQueue::instance()->globalStorage();
    accumulator += QStringLiteral( "lala: " )
        + ( globalStorage->contains( "lala" ) ? QStringLiteral( "true" ) : QStringLiteral( "false" ) ) + '\n';
    accumulator += QStringLiteral( "foo: " )
        + ( globalStorage->contains( "foo" ) ? QStringLiteral( "true" ) : QStringLiteral( "false" ) ) + '\n';
    accumulator += QStringLiteral( "count: " ) + QString::number( globalStorage->count() ) + '\n';
    globalStorage->insert( "item2", "value2" );
    globalStorage->insert( "item3", 3 );
    accumulator += QStringLiteral( "keys: %1\n" ).arg( globalStorage->keys().join( ',' ) );
    accumulator += QStringLiteral( "remove: %1\n" ).arg( QString::number( globalStorage->remove( "item2" ) ) );
    accumulator += QStringLiteral( "values: %1 %2 %3\n" )
                       .arg( globalStorage->value( "foo" ).toString(),
                             globalStorage->value( "item2" ).toString(),
                             globalStorage->value( "item3" ).toString() );

    Q_EMIT progress( 0.1 );
    cDebug() << "[DUMMYCPP]: " << accumulator;

    globalStorage->debugDump();
    Q_EMIT progress( 0.5 );

    QThread::sleep( 1 );
    ClaudeInstall::System::instance()->targetEnvCall(
        QStringList { "ls" },
        QString(),
        QString(),
        std::chrono::seconds( 1 ) );  // Expect an error because of missing rootMountPoint

    for ( int i = 0; i < 1000000; ++i )
    {
        Q_EMIT progress( qreal( i / 1000000.f ) );
    }

    QThread::sleep( 1 );

    return ClaudeInstall::JobResult::ok();
}

void
DummyCppJob::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_configurationMap = configurationMap;
}

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( DummyCppJobFactory, registerPlugin< DummyCppJob >(); )
