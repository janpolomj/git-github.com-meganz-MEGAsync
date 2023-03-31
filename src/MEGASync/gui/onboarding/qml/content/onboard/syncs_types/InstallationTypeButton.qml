import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12

import Common 1.0
import Components 1.0 as Custom

Button {
    id: btn

    enum Type {
        Sync = 0,
        Backup = 1,
        Fuse = 2
    }

    property string title: ""
    property string description: ""
    property string imageSource: ""
    property int type: InstallationTypeButton.Type.Sync

    Layout.preferredWidth: parent.width
    Layout.fillWidth: true
    Layout.preferredHeight: 96
    checkable: true
    checked: false
    onClicked: console.info("Button clicked")

    background:
        Rectangle {
            id: box

            /*
             * Object properties
             */

            border.width: 2
            radius: 8
            color: Styles.surface1
            border.color: btn.checked ? Styles.borderStrongSelected : Styles.borderStrong

            /*
             * Child objects
             */

            RowLayout {
                anchors.fill: parent
                spacing: 16

                Custom.SvgImage {
                    id: icon
                    color: btn.checked ? Styles.iconAccent : Styles.iconSecondary
                    Layout.leftMargin: 24
                    source: imageSource
                }

                ColumnLayout {

                    Text {
                        text: title
                        color: Styles.buttonPrimaryHover
                        Layout.preferredHeight: 24
                        font.pixelSize: 16
                        font.weight: Font.Bold
                        font.family: "Inter"
                        font.styleName: "normal"
                        lineHeight: 24
                    }

                    Text {
                        text: description
                        wrapMode: Text.WordWrap
                        lineHeightMode: Text.FixedHeight
                        Layout.preferredWidth: 324
                        color: Styles.toastBackground
                        Layout.preferredHeight: 32
                        font.pixelSize: 10
                        font.weight: Font.Light
                        font.family: "Inter"
                        font.styleName: "normal"
                        lineHeight: 16
                    }
                }
            }
        } // Rectangle

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onPressed: {
             mouse.accepted = false;
       }
    }

} // Button


