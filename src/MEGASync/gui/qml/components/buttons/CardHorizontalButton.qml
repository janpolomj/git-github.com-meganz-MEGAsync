// System
import QtQuick 2.12
import QtQuick.Layouts 1.12

// QML common
import Common 1.0
import Components.Texts 1.0 as MegaTexts
import Components.Images 1.0 as MegaImages

CardButton {
    id: button

    contentComponent: Component {

        Row {
            id: main

            readonly property int horizontalMargin: 24
            readonly property int verticalMargin: 16
            readonly property int textSpacing: 8
            readonly property int textTopHeight: 24
            readonly property int textBottomHeight: 32

            anchors.fill: parent
            anchors.leftMargin: horizontalMargin
            anchors.rightMargin: horizontalMargin
            anchors.topMargin: verticalMargin
            anchors.bottomMargin: verticalMargin
            spacing: 16

            MegaImages.SvgImage {
                color: button.checked || button.hovered
                       ? Styles.iconAccent
                       : Styles.iconSecondary
                source: imageSource
                sourceSize: imageSourceSize
            }

            Column {
                width: parent.width - x
                spacing: main.textSpacing

                MegaTexts.Text {
                    text: title
                    height: main.textTopHeight
                    anchors.left: parent.left
                    anchors.right: parent.right
                    font.pixelSize: MegaTexts.Text.Size.MediumLarge
                    font.weight: Font.Bold
                }

                MegaTexts.Text {
                    text: description
                    height: main.textBottomHeight
                    anchors.left: parent.left
                    anchors.right: parent.right
                    font.pixelSize: MegaTexts.Text.Size.Small
                    font.weight: Font.Light
                }
            }
        }
    }
}
