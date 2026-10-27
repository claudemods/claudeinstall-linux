/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2015-2016 Teo Mrnjavac <teo@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */


#ifndef DEVICEINFOWIDGET_H
#define DEVICEINFOWIDGET_H

#include <kpmcore/core/partitiontable.h>

#include <QWidget>

class QLabel;

class DeviceInfoWidget : public QWidget
{
    Q_OBJECT
public:
    explicit DeviceInfoWidget( QWidget* parent = nullptr );

    void setPartitionTableType( PartitionTable::TableType type );

public slots:
    void retranslateUi();

private:
    QLabel* m_ptIcon;
    QLabel* m_ptLabel;
    PartitionTable::TableType m_tableType;
};

#endif  // DEVICEINFOWIDGET_H
