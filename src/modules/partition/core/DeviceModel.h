/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2014 Aurélien Gâteau <agateau@kde.org>
 *   SPDX-FileCopyrightText: 2017 2019, Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */
#ifndef DEVICEMODEL_H
#define DEVICEMODEL_H

#include <QAbstractListModel>
#include <QList>
#include <QScopedPointer>
#include <QVariant>

#include <functional>

class Device;
class PartitionModel;

/**
 * A Qt model which exposes a list of Devices.
 */
class DeviceModel : public QAbstractListModel
{
    Q_OBJECT
public:
    DeviceModel( QObject* parent = nullptr );
    ~DeviceModel() override;

    using DeviceList = QList< Device* >;

    /**
     * Init the model with the list of devices. Does *not* take ownership of the
     * devices.
     */
    void init( const DeviceList& devices );

    int rowCount( const QModelIndex& parent = QModelIndex() ) const override;
    QVariant data( const QModelIndex& index, int role = Qt::DisplayRole ) const override;

    Device* deviceForIndex( const QModelIndex& index ) const;

    /** @brief How a frontend that can show pictures provides the DecorationRole
     *
     * The model has no pictures of its own, so that it can be used without
     * a GUI (the text-mode frontend does). Without a provider, the
     * DecorationRole is empty.
     */
    using DecorationProvider = std::function< QVariant() >;
    static void setDecorationProvider( const DecorationProvider& provider );

    void swapDevice( Device* oldDevice, Device* newDevice );

    void addDevice( Device* device );

    void removeDevice( Device* device );

private:
    DeviceList m_devices;
};

#endif /* DEVICEMODEL_H */
