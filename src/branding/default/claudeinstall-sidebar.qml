/* Horizontal progress bar along the bottom of the window.

   SPDX-FileCopyrightText: 2020 Adriaan de Groot <groot@kde.org>
   SPDX-FileCopyrightText: 2021 Anke Boersma <demm@kaosx.us>
   SPDX-License-Identifier: GPL-3.0-or-later

   Used with "sidebar: qml,bottom" in branding.desc. The colours come from the
   *style* section of branding.desc (see the claudeinstall-colors command).
*/

import io.claudeinstall.ui 1.0
import io.claudeinstall.core 1.0

import QtQuick 2.3
import QtQuick.Layouts 1.3
import QtQuick.Controls 2.15

Rectangle {
    id: sideBar;
    color: Branding.styleString( Branding.SidebarBackground );
    height: 38;
    width: parent.width;

    RowLayout {
        anchors.fill: parent;
        spacing: 2;

        Image {
            Layout.leftMargin: 12;
            Layout.rightMargin: 12;
            Layout.alignment: Qt.AlignCenter;
            id: logo;
            width: 30;
            height: width;  // square
            source: "file:/" + Branding.imagePath(Branding.ProductLogo);
            sourceSize.width: width;
            sourceSize.height: height;
        }

        Repeater {
            model: ViewManager
            Rectangle {
                Layout.leftMargin: 6;
                Layout.rightMargin: 6;
                Layout.fillWidth: true;
                Layout.alignment: Qt.AlignCenter;
                height: 32;
                radius: 6;
                color: Branding.styleString( index == ViewManager.currentStepIndex ? Branding.SidebarBackgroundCurrent : Branding.SidebarBackground );

                Text {
                    horizontalAlignment: Text.AlignHCenter;
                    verticalAlignment: Text.AlignVCenter;
                    anchors.verticalCenter: parent.verticalCenter;
                    anchors.horizontalCenter: parent.horizontalCenter;
                    x: parent.x + 12;
                    color: Branding.styleString( index == ViewManager.currentStepIndex ? Branding.SidebarTextCurrent : Branding.SidebarText );

                    text: display;
                    width: parent.width;
                    wrapMode: Text.WordWrap;
                    font.weight: (index == ViewManager.currentStepIndex ? Font.Bold : Font.Normal);
                    font.pointSize : (index == ViewManager.currentStepIndex ? 10 : 9);
                }
            }
        }

        Item {
            Layout.fillHeight: true;
        }
    }
}
