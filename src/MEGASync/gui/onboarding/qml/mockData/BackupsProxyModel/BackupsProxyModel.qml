import QtQuick 2.0

import BackupsController 1.0

ListModel {
    id: proxyModel

    function createBackups() {
        console.log("mockup BackupsProxyModel::createBackups()");
        BackupsController.createBackups();
    }

    signal rowSelectedChanged(bool selectedRow, bool selectedAll)

    property bool selectedFilterEnabled: false

    onSelectedFilterEnabledChanged: {
        console.log("mockup BackupsProxyModel::selectedFilterEnabled: " + selectedFilterEnabled);
    }

    ListElement {
        mName: "Desktop"
        mFolder: "C:\\Users\\mega\\Desktop"
        mSize: "30 MB"
        mSelected: false
        mSelectable: true
        mDone: false
        mError: 0
        mErrorVisible: false
    }
    ListElement {
        mName: "Documents12345678910111213141516171819202122232425262728293031323334353637383940"
        mFolder: "C:\\Users\\mega\\Documents12345678910111213141516171819202122232425262728293031323334353637383940"
        mSize: "2.3 GB"
        mSelected: false
        mSelectable: true
        mDone: false
        mError: 0
        mErrorVisible: false
    }
}
