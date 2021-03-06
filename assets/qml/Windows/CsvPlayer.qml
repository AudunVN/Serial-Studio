/*
 * Copyright (c) 2020-2021 Alex Spataru <https://github.com/alex-spataru>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12

Window {
    id: root

    //
    // Window options
    //
    title: qsTr("CSV Player") + CppTranslator.dummy
    minimumWidth: column.implicitWidth + 4 * app.spacing
    maximumWidth: column.implicitWidth + 4 * app.spacing
    minimumHeight: column.implicitHeight + 4 * app.spacing
    maximumHeight: column.implicitHeight + 4 * app.spacing
	flags: Qt.Dialog | Qt.WindowStaysOnTopHint | Qt.WindowCloseButtonHint | Qt.WindowTitleHint

    //
    // Close CSV file when window is closed
    //
    onVisibleChanged: {
        if (!visible && CppCsvPlayer.isOpen)
            CppCsvPlayer.closeFile()
    }

    //
    // Use page item to set application palette
    //
    Page {
        anchors.margins: 0
        anchors.fill: parent
        palette.text: "#fff"
        palette.buttonText: "#fff"
        palette.windowText: "#fff"
        palette.window: app.windowBackgroundColor

        //
        // Automatically display the window when the CSV file is opened
        //
        Connections {
            target: CppCsvPlayer
            function onOpenChanged() {
                if (CppCsvPlayer.isOpen)
                    root.visible = true
                else
                    root.visible = false
            }
        }

        //
        // Window controls
        //
        ColumnLayout {
            id: column
            anchors.centerIn: parent
            spacing: app.spacing * 2

            //
            // Timestamp display
            //
            Label {
                font.family: app.monoFont
                text: CppCsvPlayer.timestamp
                Layout.alignment: Qt.AlignLeft
            }

            //
            // Progress display
            //
            Slider {
                Layout.fillWidth: true
                value: CppCsvPlayer.progress
                onValueChanged: {
                    if (value != CppCsvPlayer.progress)
                        CppCsvPlayer.setProgress(value)
                }
            }

            //
            // Play/pause buttons
            //
            RowLayout {
                spacing: app.spacing
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignHCenter

                Button {
                    icon.color: "#fff"
                    opacity: enabled ? 1 : 0.5
                    Layout.alignment: Qt.AlignVCenter
                    onClicked: CppCsvPlayer.previousFrame()
                    icon.source: "qrc:/icons/media-prev.svg"
                    enabled: CppCsvPlayer.progress > 0 && !CppCsvPlayer.isPlaying

                    Behavior on opacity {NumberAnimation{}}
                }

                Button {
                    icon.width: 32
                    icon.height: 32
                    icon.color: "#fff"
                    onClicked: CppCsvPlayer.toggle()
                    Layout.alignment: Qt.AlignVCenter
                    icon.source: CppCsvPlayer.isPlaying ? "qrc:/icons/media-pause.svg" :
                                                          "qrc:/icons/media-play.svg"
                }

                Button {
                    icon.color: "#fff"
                    opacity: enabled ? 1 : 0.5
                    Layout.alignment: Qt.AlignVCenter
                    onClicked: CppCsvPlayer.nextFrame()
                    icon.source: "qrc:/icons/media-next.svg"
                    enabled: CppCsvPlayer.progress < 1 && !CppCsvPlayer.isPlaying

                    Behavior on opacity {NumberAnimation{}}
                }
            }
        }
    }
}
