/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore BVBA and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
import QtQuick 2.15
import Muse.Ui 1.0
import Muse.UiComponents 1.0
import Muse.Diagnostics 1.0

Rectangle {

    id: root

    objectName: "DiagnosticNavigationPanel"

    color: ui.theme.backgroundPrimaryColor

    Component.onCompleted: {
        keynavModel.reload()
    }

    DiagnosticNavigationModel {
        id: keynavModel

        onBeforeReload: {
            view.model = 0
        }

        onAfterReload: {
            view.model = keynavModel.sections
        }
    }

    Row {
        id: tools
        anchors.left: parent.left
        anchors.right: parent.right
        height: 48

        FlatButton {
            anchors.verticalCenter: parent.verticalCenter
            text: "Refresh"
            onClicked: keynavModel.reload()
        }
    }

    function formatIndex(idx) {
        if (idx.row === -1) {
            return "order: " + idx.column
        }
        return "index: [" + idx.row + "," + idx.column + "]"
    }

    ListView {
        id: view
        objectName: "DiagnosticNavigationView"
        anchors.top: tools.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        clip: true
        spacing: 8

        //model: keynavModel.sections
        delegate: Rectangle {

            id: item

            property var section: modelData
            property bool active: section.active
            onActiveChanged: {
                if (active) {
                    //view.positionViewAtIndex(model.index, ListView.Beginning)
                }
            }

            width: parent ? parent.width : 0
            height: 48 + subView.height

            color: ui.theme.buttonColor
            opacity: item.section.enabled ? 1.0 : ui.theme.itemOpacityDisabled
            border.width: item.section.active ? 3 : 0
            border.color: ui.theme.fontPrimaryColor

            StyledTextLabel {
                id: secLabel
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: 8
                text: "section: " + item.section.name + ", "
                      + root.formatIndex(item.section.index)
                      + ", enabled: " + item.section.enabled
                      + ", panels: " + item.section.panelsCount
                      + ", controls: " + item.section.controlsCount
            }

            ListView {
                id: subView
                objectName: "DiagnosticNavigationSubView"
                anchors.top: secLabel.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 16
                height: contentHeight
                clip: true
                interactive: false
                spacing: 8

                model: item.section.subsections
                delegate: Rectangle {

                    id: subitem

                    property var panel: modelData

                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 48 + ctrlView.height

                    color: ui.theme.buttonColor
                    opacity: subitem.panel.enabled ? 1.0 : ui.theme.itemOpacityDisabled
                    border.width: subitem.panel.active ? 3 : 0
                    border.color: ui.theme.fontPrimaryColor

                    StyledTextLabel {
                        id: subLabel
                        anchors.top: parent.top
                        anchors.left: parent.left
                        anchors.margins: 8
                        text: "panel: " + subitem.panel.name
                              + ", " + root.formatIndex(subitem.panel.index)
                              + ", enabled: " + subitem.panel.enabled
                              + ", controls: " + subitem.panel.controlsCount
                    }

                    GridView {
                        id: ctrlView
                        objectName: "DiagnosticNavigationGridView"
                        anchors.top: subLabel.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: 16
                        height: contentHeight
                        clip: true
                        interactive: false

                        cellHeight: 32
                        cellWidth: 68

                        model: subitem.panel.controls
                        delegate: Rectangle {
                            id: gridItem

                            property var control: modelData

                            // need for tooltips
                            property var navigation: control

                            height: 28
                            width: 64
                            radius: 4

                            enabled: control.enabled

                            color: ui.theme.buttonColor
                            opacity: gridItem.control.enabled ? 1.0 : ui.theme.itemOpacityDisabled
                            border.width: gridItem.control.active ? 3 : 1
                            border.color: ui.theme.fontPrimaryColor

                            Column {
                                anchors.fill: parent
                                anchors.margins: 2

                                StyledTextLabel {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    horizontalAlignment: Text.AlignLeft
                                    verticalAlignment: Text.AlignTop
                                    font.pixelSize: nameLabel.font.pixelSize / 1.5
                                    text: "[" + control.index.row + "," + control.index.column + "]"
                                }

                                StyledTextLabel {
                                    id: nameLabel
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.margins: 8
                                    text: control.name
                                }
                            }

                            MouseArea {
                                anchors.fill: parent

                                enabled: gridItem.enabled
                                acceptedButtons: Qt.LeftButton | Qt.RightButton
                                hoverEnabled: true
                                onContainsMouseChanged: {
                                    if (containsMouse) {
                                        var info = control.name + "\n"
                                                + root.formatIndex(control.index) + "\n"
                                                + "enabled: " + control.enabled

                                        ui.tooltip.show(gridItem, info)
                                    } else {
                                        ui.tooltip.hide(gridItem)
                                    }
                                }

                                onClicked: function(e) {
                                    switch (e.button) {
                                    case Qt.LeftButton: {
                                        control.requestActive()
                                        control.trigger()
                                    } break;
                                    case Qt.RightButton: {
                                        keynavModel.copyToClipboard(item.section, subitem.panel, gridItem.control)
                                    } break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
