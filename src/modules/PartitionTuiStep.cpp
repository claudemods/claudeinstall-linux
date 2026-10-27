/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2024 ClaudeInstall contributors
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "PartitionTuiStep.h"

#include "core/DeviceModel.h"
#include "core/PartUtils.h"
#include "core/PartitionActions.h"
#include "core/PartitionCoreModule.h"
#include "core/PartitionModel.h"

#include "TuiWidgets.h"

#include "BrandingInfo.h"
#include "GlobalStorage.h"
#include "JobQueue.h"
#include "partition/PartitionIterator.h"
#include "utils/Logger.h"
#include "utils/String.h"
#include "utils/Units.h"
#include "utils/Variant.h"

#include <kpmcore/core/device.h>

#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>

CLAUDEINSTALL_PLUGIN_FACTORY_DEFINITION( PartitionTuiStepFactory, registerPlugin< PartitionTuiStep >(); )

namespace Tui = ClaudeInstall::Tui;

static QString
q( const std::string& s )
{
    return QString::fromStdString( s );
}

PartitionTuiStep::PartitionTuiStep( QObject* parent )
    : ClaudeInstall::TuiStep( parent )
    , m_config( new Config( this ) )
    , m_core( new PartitionCoreModule( this ) )  // Unusable before init is complete!
{
}

PartitionTuiStep::~PartitionTuiStep()
{
    if ( m_future )
    {
        m_future->waitForFinished();
    }
}

QString
PartitionTuiStep::prettyName() const
{
    return tr( "Partitions", "@label" );
}

QObject*
PartitionTuiStep::config()
{
    return m_config;
}

bool
PartitionTuiStep::isBackEnabled() const
{
    return true;
}

bool
PartitionTuiStep::isAtBeginning() const
{
    return true;
}

bool
PartitionTuiStep::isAtEnd() const
{
    return true;
}

ClaudeInstall::JobList
PartitionTuiStep::jobs() const
{
    return m_core->jobs( m_config );
}

void
PartitionTuiStep::setConfigurationMap( const QVariantMap& configurationMap )
{
    m_config->setConfigurationMap( configurationMap );

    // The same global storage settings as the GUI step makes; other modules (and the jobs) read them
    ClaudeInstall::GlobalStorage* gs = ClaudeInstall::JobQueue::instance()->globalStorage();
    if ( configurationMap.contains( "swapPartitionName" ) )
    {
        gs->insert( "swapPartitionName", ClaudeInstall::getString( configurationMap, "swapPartitionName" ) );
    }
    gs->insert( "drawNestedPartitions", ClaudeInstall::getBool( configurationMap, "drawNestedPartitions", false ) );
    gs->insert( "alwaysShowPartitionLabels", ClaudeInstall::getBool( configurationMap, "alwaysShowPartitionLabels", true ) );
    gs->insert( "enableLuksAutomatedPartitioning",
                ClaudeInstall::getBool( configurationMap, "enableLuksAutomatedPartitioning", true ) );
    const QString partitionTableName = ClaudeInstall::getString( configurationMap, "defaultPartitionTableType" );
    if ( partitionTableName.isEmpty() )
    {
        cWarning() << "Partition-module setting *defaultPartitionTableType* is unset, "
                      "will use gpt for efi or msdos for bios";
    }
    gs->insert( "defaultPartitionTableType", partitionTableName );
    gs->insert( "createHybridBootloaderLayout",
                ClaudeInstall::getBool( configurationMap, "createHybridBootloaderLayout", false ) );

    m_core->partitionLayout().init( m_config->defaultFsType(), configurationMap.value( "partitionLayout" ).toList() );
    m_core->dirFSRestrictLayout().init( configurationMap.value( "directoryFilesystemRestrictions" ).toList() );

    // Scanning the disks takes a while, so it happens in the background
    m_future = new QFutureWatcher< void >( this );
    connect( m_future,
             &QFutureWatcher< void >::finished,
             this,
             [ this ]
             {
                 continueLoading();
                 m_future->deleteLater();
                 m_future = nullptr;
             } );
    auto* core = m_core;
    m_future->setFuture( QtConcurrent::run( [ core ] { core->init(); } ) );

    ClaudeInstall::TuiStep::setConfigurationMap( configurationMap );
}

void
PartitionTuiStep::continueLoading()
{
    // What the page offers: the swap choices that the configuration enables ..
    const auto enabled = m_config->swapChoices();
    for ( Config::SwapChoice c : { Config::NoSwap, Config::SmallSwap, Config::FullSwap, Config::ReuseSwap, Config::SwapFile } )
    {
        if ( !enabled.contains( c ) )
        {
            continue;
        }
        m_swapChoices.append( c );
        switch ( c )
        {
        case Config::NoSwap:
            m_swapNames.push_back( Tui::str( tr( "No swap", "@label" ) ) );
            break;
        case Config::ReuseSwap:
            m_swapNames.push_back( Tui::str( tr( "Reuse swap", "@label" ) ) );
            break;
        case Config::SmallSwap:
            m_swapNames.push_back( Tui::str( tr( "Swap (no hibernate)", "@label" ) ) );
            break;
        case Config::FullSwap:
            m_swapNames.push_back( Tui::str( tr( "Swap (with hibernate)", "@label" ) ) );
            break;
        case Config::SwapFile:
            m_swapNames.push_back( Tui::str( tr( "Swap to file", "@label" ) ) );
            break;
        }
        if ( c == m_config->swapChoice() )
        {
            m_swap = m_swapChoices.size() - 1;
        }
    }

    // .. and the filesystems
    const QStringList filesystems = m_config->eraseFsTypes();
    for ( const QString& fs : filesystems )
    {
        m_filesystems.push_back( Tui::str( fs ) );
    }
    m_filesystem = qMax( 0, int( filesystems.indexOf( m_config->eraseFsType() ) ) );

    m_encrypt = m_config->preCheckEncryption();

    Tui::fillEntries( &m_devices, m_core->deviceModel(), Qt::DisplayRole );
    m_config->setInstallChoice( Config::Erase );
    m_ready = true;
    updateNext();
}

Device*
PartitionTuiStep::selectedDevice() const
{
    auto* model = m_core->deviceModel();
    return model ? model->deviceForIndex( model->index( m_device ) ) : nullptr;
}

QString
PartitionTuiStep::eraseProblem() const
{
    Device* device = selectedDevice();
    if ( !device )
    {
        return tr( "There is no disk to install on.", "@info" );
    }
    for ( auto it = ClaudeInstall::Partition::PartitionIterator::begin( device );
          it != ClaudeInstall::Partition::PartitionIterator::end( device );
          ++it )
    {
        if ( ( *it )->isMounted() )
        {
            return tr( "This storage device has one of its partitions mounted.", "@info" );
        }
    }
    return QString();
}

bool
PartitionTuiStep::encryptionWanted() const
{
    const auto* gs = ClaudeInstall::JobQueue::instance()->globalStorage();
    return m_encrypt && gs->value( "enableLuksAutomatedPartitioning" ).toBool();
}

bool
PartitionTuiStep::encryptionConfirmed() const
{
    return !m_passphrase.empty() && m_passphrase == m_passphraseAgain;
}

bool
PartitionTuiStep::calculateNextEnabled() const
{
    if ( !m_ready || m_devices.empty() || !eraseProblem().isEmpty() )
    {
        return false;
    }
    return !encryptionWanted() || encryptionConfirmed();
}

bool
PartitionTuiStep::isNextEnabled() const
{
    return m_nextEnabled;
}

void
PartitionTuiStep::updateNext()
{
    const bool enabled = calculateNextEnabled();
    if ( enabled != m_nextEnabled )
    {
        m_nextEnabled = enabled;
        emit nextStatusChanged( enabled );
    }
}

void
PartitionTuiStep::onActivate()
{
    m_config->fillGSSecondaryConfiguration();
}

void
PartitionTuiStep::apply()
{
    auto* gs = ClaudeInstall::JobQueue::instance()->globalStorage();

    m_config->setInstallChoice( Config::Erase );
    if ( m_swap >= 0 && m_swap < m_swapChoices.size() )
    {
        m_config->setSwapChoice( m_swapChoices.at( m_swap ) );
    }
    if ( m_filesystem >= 0 && size_t( m_filesystem ) < m_filesystems.size() )
    {
        m_config->setEraseFsTypeChoice( q( m_filesystems[ size_t( m_filesystem ) ] ) );
    }
    const QString passphrase = encryptionWanted() ? q( m_passphrase ) : QString();

    Device* device = selectedDevice();
    if ( !device )
    {
        return;
    }
    if ( m_core->isDirty() )
    {
        // Starts from the disk as it is; this replaces the devices, so look again
        m_core->revertDevice( device );
        device = selectedDevice();
        if ( !device )
        {
            return;
        }
    }

    PartitionActions::Choices::AutoPartitionOptions options { gs->value( "defaultPartitionTableType" ).toString(),
                                                              m_config->eraseFsType(),
                                                              m_config->luksFileSystemType(),
                                                              passphrase,
                                                              gs->value( "efiSystemPartition" ).toString(),
                                                              ClaudeInstall::GiBtoBytes(
                                                                  gs->value( "requiredStorageGiB" ).toDouble() ),
                                                              m_config->swapChoice() };
    PartitionActions::doAutopartition( m_core, device, options );

    gs->insert( "luksPassphrase", passphrase.isEmpty() ? QString() : ClaudeInstall::String::obscure( passphrase ) );

    // The bootloader of a BIOS system goes on the disk (for EFI, the partitioning set up the ESP)
    if ( !PartUtils::isEfiSystem() )
    {
        m_core->setBootLoaderInstallPath( device->deviceNode() );
    }
}

void
PartitionTuiStep::onLeave()
{
    // Also called when going back; there is nothing to apply if the choices are not complete
    if ( m_ready && calculateNextEnabled() )
    {
        apply();
    }
}

/// @brief One line per partition of @p model, as it will be (or is)
static QStringList
describeModel( const PartitionModel* model )
{
    QStringList lines;
    for ( int i = 0; i < model->rowCount(); ++i )
    {
        auto text = [ & ]( int column ) { return model->data( model->index( i, column ) ).toString(); };
        lines.append( QStringLiteral( "  %1  %2  %3  %4" )
                          .arg( text( PartitionModel::NameColumn ),
                                text( PartitionModel::FileSystemColumn ),
                                text( PartitionModel::MountPointColumn ),
                                text( PartitionModel::SizeColumn ) ) );
    }
    return lines;
}

QString
PartitionTuiStep::prettyStatus() const
{
    if ( !m_ready )
    {
        return QString();
    }

    const QString product = ClaudeInstall::BrandingInfo::instance().shortVersionedName();
    QStringList lines;

    const auto list = m_core->createSummaryInfo();
    for ( const auto& info : list )
    {
        lines.append( tr( "Erase disk %1 (%2) and install %3", "@info" ).arg( info.deviceNode, info.deviceName, product ) );
        lines.append( tr( "Before:", "@label" ) );
        lines.append( describeModel( info.partitionModelBefore ) );
        lines.append( tr( "After:", "@label" ) );
        lines.append( describeModel( info.partitionModelAfter ) );

        // Created for us to look at; nothing else owns them
        delete info.partitionModelBefore;
        delete info.partitionModelAfter;
    }
    lines.append( QString() );
    for ( const auto& job : m_core->jobs( m_config ) )
    {
        const QString description = Tui::stripMarkup( job->prettyDescription() );
        if ( !description.isEmpty() )
        {
            lines.append( description );
        }
    }
    if ( m_config->swapChoice() == Config::SwapChoice::SwapFile )
    {
        lines.append( tr( "Create a swap file." ) );
    }
    return lines.join( QLatin1Char( '\n' ) );
}

ClaudeInstall::RequirementsList
PartitionTuiStep::checkRequirements()
{
    if ( m_future )
    {
        m_future->waitForFinished();
    }

    ClaudeInstall::RequirementsList l;
    l.append( {
        QLatin1String( "partitions" ),
        [] { return tr( "has at least one disk device available." ); },
        [] { return tr( "There are no partitions to install on." ); },
        m_core->deviceModel()->rowCount() > 0,  // satisfied
#ifdef DEBUG_PARTITION_UNSAFE
        false  // optional
#else
        true  // required
#endif
    } );
    return l;
}

ftxui::Component
PartitionTuiStep::createComponent()
{
    using namespace ftxui;

    auto devices = Tui::picker( tr( "Disk", "@label" ),
                                &m_devices,
                                &m_device,
                                [ this ] { updateNext(); } );
    auto swaps = Tui::picker( tr( "Swap", "@label" ), &m_swapNames, &m_swap, [ this ] { updateNext(); } );
    auto filesystems = Tui::picker( tr( "Filesystem", "@label" ), &m_filesystems, &m_filesystem, [ this ] { updateNext(); } );

    Component choices = Container::Horizontal(
        { devices, Container::Vertical( { swaps | size( WIDTH, EQUAL, 28 ), filesystems | size( WIDTH, EQUAL, 28 ) } ) } );

    Component encryption = Container::Vertical(
        { Tui::check( tr( "Encrypt system", "@label" ), &m_encrypt, [ this ] { updateNext(); } ),
          Maybe( Container::Vertical(
                     { Tui::field( tr( "Passphrase", "@label" ), &m_passphrase, true, [ this ] { updateNext(); } ),
                       Tui::field( tr( "Repeat passphrase", "@label" ),
                                   &m_passphraseAgain,
                                   true,
                                   [ this ] { updateNext(); },
                                   [ this ]
                                   {
                                       return !m_passphraseAgain.empty() && !encryptionConfirmed()
                                           ? tr( "The passphrases do not match.", "@info" )
                                           : QString();
                                   } ) } ),
                 &m_encrypt ) } );

    const bool encryptionOffered = ClaudeInstall::JobQueue::instance()->globalStorage()->value( "enableLuksAutomatedPartitioning" ).toBool();
    Component form = encryptionOffered ? Container::Vertical( { choices, encryption } ) : choices;

    return Renderer( form,
                     [ this, form, choices, encryption, encryptionOffered ]
                     {
                         if ( !m_ready )
                         {
                             return Tui::titled( prettyName(),
                                                 Tui::paragraphOf( tr( "Gathering system information…", "@status" ) ) );
                         }

                         Elements notes;
                         const QString problem = eraseProblem();
                         notes.push_back( Tui::problem( problem ) );
                         notes.push_back( Tui::paragraphOf( tr( "Erase the disk and install. "
                                                                "All the data on the chosen disk will be lost.",
                                                                "@label" ) ) );

                         return Tui::titled(
                             prettyName(),
                             vbox( { vbox( notes ),
                                     text( "" ),
                                     choices->Render() | flex,
                                     encryptionOffered ? encryption->Render() : emptyElement(),
                                     text( tr( "Left and right change the list; Tab goes on to the next part. "
                                               "Other ways to partition are not available in text mode.",
                                               "@label" )
                                               .toStdString() )
                                         | dim } ) );
                     } );
}
