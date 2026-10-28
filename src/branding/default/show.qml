/* === This file is part of ClaudeInstall - <https://claudeinstall.io> ===
 *
 *   SPDX-License-Identifier: GPL-3.0-or-later
 *
 *   The slideshow shown while the system is installed: three slides that change every
 *   eight seconds. What the installer is doing right now is written under the progress bar.
 */

import QtQuick 2.0;
import claudeinstall.slideshow 1.0;

Presentation
{
    id: presentation

    function nextSlide() {
        presentation.goToNextSlide();
    }

    Timer {
        id: advanceTimer
        interval: 8000
        running: presentation.activatedInClaudeInstall
        repeat: true
        onTriggered: nextSlide()
    }

    Slide {
        Image {
            source: "slide1.png"
            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
        }
    }

    Slide {
        Image {
            source: "slide2.png"
            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
        }
    }

    Slide {
        Image {
            source: "slide3.png"
            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
        }
    }

    function onActivate() {
        presentation.currentSlide = 0;
    }

    function onLeave() {
    }
}
