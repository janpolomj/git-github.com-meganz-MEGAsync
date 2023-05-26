// System
import QtQuick 2.12
import QtQuick.Layouts 1.12
import QtQuick.Controls 2.12

// QML common
import Common 1.0
import Components.Texts 1.0 as MegaTexts
import Components.Images 1.0 as MegaImages
import Components.Buttons 1.0 as MegaButtons

// C++
import Onboard 1.0

Rectangle {

    property alias buttonGroup: buttonGroup
    property alias preferencesButton: preferencesButton
    property alias doneButton: doneButton

    property string title: "Your Sync is set up!"
    property string description: "Lorem ipsum dolor a text that congratulates the user and suggests other options to choose below. Use two lines at most. In this case we offer syncs as an option again."

    color: Styles.surface1

    ColumnLayout {
        anchors.fill: parent

        MegaTexts.Text {
            text: title
            Layout.topMargin: 32
            font.pixelSize: MegaTexts.Text.Size.Large
            Layout.preferredWidth: parent.width
            font.weight: Font.Bold
            horizontalAlignment: Text.AlignHCenter
        }

        MegaImages.SvgImage {
            source: Images.resume
            Layout.topMargin: 40
            Layout.alignment: Qt.AlignHCenter
        }

        MegaTexts.Text {
            text: description
            Layout.topMargin: 40
            Layout.preferredHeight: 40
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 712
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: MegaTexts.Text.Size.Medium
            font.weight: Font.Light
        }

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredHeight: 208
            Layout.preferredWidth: 712
            color: "transparent"

            ButtonGroup {
                id: buttonGroup
            }

            RowLayout {
                spacing: 11
                anchors.fill: parent

                SyncsVerticalButton {
                    id: syncButton

                    title: OnboardingStrings.sync
                    description: OnboardingStrings.syncButtonDescription
                    imageSource: Images.sync
                    ButtonGroup.group: buttonGroup
                    type: SyncsType.Sync
                    checkable: false
                }

                SyncsVerticalButton {
                    id: backupsButton

                    title: OnboardingStrings.backup
                    description: OnboardingStrings.backupButtonDescription
                    imageSource: Images.cloud
                    ButtonGroup.group: buttonGroup
                    type: SyncsType.Backup
                    checkable: false
                }
            }
        }

        RowLayout {
            spacing: 8
            Layout.rightMargin: 32
            Layout.bottomMargin: 24
            Layout.alignment: Qt.AlignBottom | Qt.AlignRight

            MegaButtons.OutlineButton {
                id: preferencesButton

                text: OnboardingStrings.openInPreferences
            }

            MegaButtons.PrimaryButton {
                id: doneButton

                text: OnboardingStrings.done
            }
        }
    }
}
