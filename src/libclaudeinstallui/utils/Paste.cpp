/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2019 Bill Auger
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

#include "Paste.h"

#include "Branding.h"
#include "DllMacro.h"
#include "utils/Logger.h"
#include "utils/Units.h"
#include "widgets/TranslationFix.h"

#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QTcpSocket>
#include <QUrl>
#include <QWidget>

using namespace ClaudeInstall::Units;

/** @brief Reads the logfile, returns its contents.
 *
 * Returns an empty QByteArray() on any kind of error.
 */
STATICTEST QByteArray
logFileContents( const qint64 sizeLimitBytes )
{
    if ( sizeLimitBytes > 0 )
    {
        cDebug() << "Log upload size limit was limited to" << sizeLimitBytes << "bytes";
    }
    if ( sizeLimitBytes == 0 )
    {
        cDebug() << "Log upload size is 0, upload disabled.";
        return QByteArray();
    }

    const QString name = Logger::logFile();
    QFile pasteSourceFile( name );
    if ( !pasteSourceFile.open( QIODevice::ReadOnly | QIODevice::Text ) )
    {
        cWarning() << "Could not open log file" << name;
        return QByteArray();
    }
    if ( sizeLimitBytes < 0 )
    {
        return pasteSourceFile.readAll();
    }
    QFileInfo fi( pasteSourceFile );
    if ( fi.size() > sizeLimitBytes )
    {
        cDebug() << "Only last" << sizeLimitBytes << "bytes of log file (sized" << fi.size() << "bytes) uploaded";
        fi.refresh();  // Because we just wrote to the file with that cDebug() ^^
        pasteSourceFile.seek( fi.size() - sizeLimitBytes );
    }
    return pasteSourceFile.read( sizeLimitBytes );
}

STATICTEST QString
ficheLogUpload( const QByteArray& pasteData, const QUrl& serverUrl, QObject* parent )
{
    QTcpSocket* socket = new QTcpSocket( parent );
    // 16 bits of port-number
    socket->connectToHost( serverUrl.host(), quint16( serverUrl.port() ) );

    if ( !socket->waitForConnected() )
    {
        cError() << "Could not connect to paste server";
        socket->close();
        return QString();
    }

    cDebug() << "Connected to paste server" << serverUrl.host();

    socket->write( pasteData );

    if ( !socket->waitForBytesWritten() )
    {
        cError() << "Could not write to paste server";
        socket->close();
        return QString();
    }

    cDebug() << Logger::SubEntry << "Paste data written to paste server";

    if ( !socket->waitForReadyRead() )
    {
        cError() << "No data from paste server";
        socket->close();
        return QString();
    }

    cDebug() << Logger::SubEntry << "Reading response from paste server";
    QByteArray responseText = socket->readLine( 1024 );
    socket->close();

    QUrl pasteUrl = QUrl( QString( responseText ).trimmed(), QUrl::StrictMode );
    if ( pasteUrl.isValid() && pasteUrl.host() == serverUrl.host() )
    {
        cDebug() << Logger::SubEntry << "Paste server results:" << pasteUrl;
        return pasteUrl.toString();
    }
    else
    {
        cError() << "No data from paste server";
        return QString();
    }
}

QString
ClaudeInstall::Paste::doLogUpload( QObject* parent )
{
    auto [ type, serverUrl, sizeLimitBytes ] = ClaudeInstall::Branding::instance()->uploadServer();
    if ( !serverUrl.isValid() )
    {
        cWarning() << "Upload configured with invalid URL";
        return QString();
    }
    if ( type == ClaudeInstall::Branding::UploadServerType::None )
    {
        // Early return to avoid reading the log file
        return QString();
    }
    if ( sizeLimitBytes == 0 )
    {
        // Suggests that it is un-set in the config file
        cWarning() << "Upload configured to send 0 bytes";
        return QString();
    }

    QByteArray pasteData = logFileContents( sizeLimitBytes );
    if ( pasteData.isEmpty() )
    {
        // An error has already been logged
        return QString();
    }

    switch ( type )
    {
    case ClaudeInstall::Branding::UploadServerType::None:
        cWarning() << "No upload configured.";
        return QString();
    case ClaudeInstall::Branding::UploadServerType::Fiche:
        return ficheLogUpload( pasteData, serverUrl, parent );
    }
    return QString();
}

QString
ClaudeInstall::Paste::doLogUploadUI( QWidget* parent )
{
    // These strings originated in the ViewManager class
    QString pasteUrl = ClaudeInstall::Paste::doLogUpload( parent );
    QString pasteUrlMessage;
    if ( pasteUrl.isEmpty() )
    {
        pasteUrlMessage = QCoreApplication::translate( "ClaudeInstall::ViewManager",
                                                       "The upload was unsuccessful. No web-paste was done." );
    }
    else
    {
        QClipboard* clipboard = QApplication::clipboard();
        clipboard->setText( pasteUrl, QClipboard::Clipboard );

        if ( clipboard->supportsSelection() )
        {
            clipboard->setText( pasteUrl, QClipboard::Selection );
        }
        QString pasteUrlFmt = QCoreApplication::translate( "ClaudeInstall::ViewManager",
                                                           "Install log posted to\n\n%1\n\nLink copied to clipboard" );
        pasteUrlMessage = pasteUrlFmt.arg( pasteUrl );
    }

    QMessageBox mb( QMessageBox::Critical,
                    QCoreApplication::translate( "ClaudeInstall::ViewManager", "Install Log Paste URL" ),
                    pasteUrlMessage,
                    QMessageBox::Ok );
    ClaudeInstall::fixButtonLabels( &mb );
    mb.exec();
    return pasteUrl;
}

bool
ClaudeInstall::Paste::isEnabled()
{
    auto [ type, serverUrl, sizeLimitBytes ] = ClaudeInstall::Branding::instance()->uploadServer();
    return type != ClaudeInstall::Branding::UploadServerType::None && sizeLimitBytes != 0;
}
