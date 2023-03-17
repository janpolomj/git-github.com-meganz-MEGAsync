import QtQuick 2.12
import QtQuick.Controls 2.12

ResumePageForm {

    buttonGroup.onCheckedButtonChanged: {
        console.debug("TODO: Button group clicked");
    }

    preferencesButton.onClicked: {
        console.debug("TODO: Open in preferences button clicked");
    }

    doneButton.onClicked: {
        console.debug("TODO: Done button clicked");
    }
}
