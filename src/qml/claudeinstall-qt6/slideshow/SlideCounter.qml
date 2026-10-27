/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-FileCopyrightText: 2018 Adriaan de Groot <groot@kde.org>
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   ClaudeInstall is Free Software: see the License-Identifier above.
 *
 */

/* This control just shows a (non-translated) count of the slides
 * in the slideshow in the format "n / total".
 */

import QtQuick 2.5;

Rectangle {
    id: slideCounter
    anchors.right: parent.right
    anchors.bottom: parent.bottom
    width: 100
    height: 50

    Text {
        id: slideCounterText
        anchors.centerIn: parent
        //: slide counter, %1 of %2 (numeric)
        text: qsTr("%L1 / %L2").arg(parent.parent.currentSlide + 1).arg(parent.parent.slides.length)
    }
}
