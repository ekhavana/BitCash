import QtQuick 2.7
import QtQuick.Controls 2.0
import QtQuick.Controls.Material 2.0
import QtQuick.Layouts 1.3

Item {
    id: item1
    width: 1100
    height: 1000

    Rectangle {
        id: toolBar
        y: 0
        height: 44
        anchors.right: parent.right
        anchors.rightMargin: 0
        anchors.left: parent.left
        anchors.leftMargin: 0
        color: "#e2e3f2"

        Image {
            x: 30
            y: 13
            width: 16
            height: 16
            anchors.verticalCenter: parent.verticalCenter
            fillMode: Image.PreserveAspectFit
            source: "../res/assets/Notification Alerts/info-icon.png"
        }

        Button {
            onClicked: toolBar.visible = false
            anchors.right: parent.right
            anchors.rightMargin: 30
            y: 13
            width: 16
            height: 16
            anchors.verticalCenter: parent.verticalCenter
            background: Image {
                fillMode: Image.PreserveAspectFit
                source: "../res/assets/Notification Alerts/info-close.png"
            }
        }

        Label {
            id: text10
            x: 58
            y: 13
            text: qsTr("Use your BitCash address or nickname you created to receive BitCash from other users")
            font.weight: Font.DemiBold
            anchors.verticalCenter: parent.verticalCenter
            textFormat: Text.AutoText
            font.pixelSize: 13
            font.family: "Montserrat SemiBold"
            color: "#3f49aa"
        }
    }

    property alias toolBar2: toolBar2
    property alias text11: text11

    Rectangle {
        id: toolBar2
        y: 0
        visible: false
        height: 44
        anchors.right: parent.right
        anchors.rightMargin: 0
        anchors.left: parent.left
        anchors.leftMargin: 0
        color: "#ddf5e3"

        Image {
            x: 30
            y: 13
            width: 16
            height: 16
            fillMode: Image.PreserveAspectFit
            source: "../res/assets/Notification Alerts/success-icon.png"
        }

        Button {
            onClicked: toolBar2.visible = false
            anchors.right: parent.right
            anchors.rightMargin: 30
            y: 13
            width: 16
            height: 16
            background: Image {
                fillMode: Image.PreserveAspectFit
                source: "../res/assets/Notification Alerts/success-close.png"
            }
        }

        Label {
            id: text11
            x: 58
            y: 13
            text: ""
            font.pixelSize: 13
            font.weight: Font.DemiBold
            font.family: "Montserrat SemiBold"
            color: "#1bb846"
        }
    }

    Image {
        id: receiveicon
        y: 66
        anchors.verticalCenter: receivecap.verticalCenter
        anchors.left: layoutrect.left
        fillMode: Image.PreserveAspectFit
        source: "../res/assets/Headlines/receive.png"
    }

    Text {
        id: receivecap
        y: 74
        width: 97
        color: "#202124"
        anchors.left: receiveicon.right
        anchors.leftMargin: 15
        text: qsTr("Receive")
        font.pixelSize: 20
        font.weight: Font.DemiBold
        font.family: "Montserrat SemiBold"
    }

    Rectangle {
        id: layoutrect
        anchors.top: toolBar.bottom
        anchors.topMargin: 10
        visible: false
        width: 682
        height: 210
        anchors.horizontalCenter: parent.horizontalCenter
    }

    ShadowBox {
        id: shadowwhitebox
        anchors.left: layoutrect.left
        anchors.right: layoutrect.right
        color: "white"
        radius: 3
        anchors.bottom: whitebox3.bottom
        anchors.bottomMargin: -31
        anchors.rightMargin: 0
        anchors.leftMargin: 0
        anchors.top: whitebox.top
        anchors.topMargin: -6
        border.width: 0
        width: 632
    }

    Rectangle {
        id: whitebox
        anchors.left: layoutrect.left
        anchors.right: layoutrect.right
        color: "white"
        radius: 1
        anchors.rightMargin: 6
        anchors.leftMargin: 6
        anchors.top: whitebox4.bottom
        anchors.topMargin: 26
        border.width: 0
        width: 632
        height: 171
    }

    Rectangle {
        id: whitebox2
        width: 620
        anchors.left: layoutrect.left
        anchors.right: layoutrect.right
        anchors.top: whitebox.bottom
        anchors.topMargin: 0
        color: "white"
        radius: 3
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        visible: true
        border.width: 0
        height: 410
    }

    Label {
        id: label
        color: "#202124"
        text: qsTr("Your BitCash address")
        font.pixelSize: 14
        font.weight: Font.DemiBold
        font.family: "Montserrat SemiBold"
        anchors.top: whitebox.top
        anchors.topMargin: 30
        anchors.left: whitebox.left
        anchors.leftMargin: 30
    }

    property alias receivingaddress: receivingaddress
    property alias receivingaddressEdit: receivingaddressEdit

    Label {
        id: receivingaddress
        color: "#3e45ac"
        text: "TESThyhwTyjkPPKkQ27T4B3vAuU6iMgwmHWuvNHjGTB8cmdwxExm"
        font.pixelSize: 16
        font.family: "Montserrat"
        anchors.top: label.bottom
        anchors.topMargin: 5
        anchors.left: label.left
        anchors.leftMargin: 0
        anchors.right: whitebox.right
        anchors.rightMargin: 30
    }

    MenuTextField {
        id: receivingaddressEdit
        visible: false
        height: 52
        text: "TESThyhwTyjkPPKkQ27T4B3vAuU6iMgwmHWuvNHjGTB8cmdwxExm"
        bottomPadding: 16
        font.pixelSize: 16
        font.family: "Montserrat"
        padding: 16
        rightPadding: 16
        leftPadding: 16
        topPadding: 16
        anchors.top: label.bottom
        anchors.topMargin: 10
        anchors.left: label.left
        anchors.leftMargin: 0
        anchors.right: whitebox.right
        anchors.rightMargin: 30
        selectByMouse: true
        readOnly: true
    }

    Label {
        id: label1
        color: "#202124"
        text: qsTr("Your nickname to receive BitCash")
        font.pixelSize: 14
        font.weight: Font.DemiBold
        font.family: "Montserrat SemiBold"
        anchors.top: whitebox2.top
        anchors.topMargin: 30
        anchors.left: whitebox.left
        anchors.leftMargin: 36
    }

    property alias receivingnickname: receivingnickname
    property alias receivingnicknameEdit: receivingnicknameEdit

    Label {
        id: receivingnickname
        color: "#3e45ac"
        text: "@test"
        font.letterSpacing: 0
        font.family: "Montserrat"
        font.pixelSize: 18
        anchors.top: label1.bottom
        anchors.topMargin: 5
        anchors.right: whitebox2.right
        anchors.rightMargin: 30
        anchors.left: whitebox2.left
        anchors.leftMargin: 30
    }

    MenuTextField {
        id: receivingnicknameEdit
        visible: false
        height: 52
        text: ""
        font.letterSpacing: 0
        font.family: "Montserrat"
        horizontalAlignment: Text.AlignLeft
        curPos: 0
        renderType: Text.QtRendering
        font.pixelSize: 16
        padding: 16
        rightPadding: 16
        leftPadding: 16
        topPadding: 16
        anchors.top: label1.bottom
        anchors.topMargin: 10
        anchors.right: whitebox2.right
        anchors.rightMargin: 36
        anchors.left: whitebox2.left
        anchors.leftMargin: 36
        selectByMouse: true
        readOnly: true
    }

    property alias copyaddress: copyaddress
    Mybutton {
        id: copyaddress
        height: 44
        text: qsTr("Copy ₡ address to clipboard")
        font.weight: Font.DemiBold
        font.pixelSize: 14
        font.family: "Montserrat SemiBold"
        rightPadding: 20
        leftPadding: 20
        anchors.top: receivingaddress.bottom
        anchors.topMargin: 20
        anchors.left: whitebox.left
        anchors.leftMargin: 30
        font.capitalization: Font.MixedCase
    }

    property alias copynickname: copynickname
    Mybutton {
        id: copynickname
        height: 44
        text: qsTr("Copy ₡ nickname to clipboard")
        font.weight: Font.DemiBold
        font.pixelSize: 14
        font.family: "Montserrat SemiBold"
        rightPadding: 20
        leftPadding: 20
        anchors.top: receivingnickname2.bottom
        anchors.topMargin: 20
        anchors.left: whitebox2.left
        anchors.leftMargin: 30
        font.capitalization: Font.MixedCase
    }

    property alias whitebox3: whitebox3
    Rectangle {
        id: whitebox3
        x: -3
        y: -1
        width: 632
        height: 410
        color: "#ffffff"
        radius: 1
        anchors.rightMargin: 6
        anchors.leftMargin: 6
        visible: false
        anchors.topMargin: 0
        anchors.top: whitebox.bottom
        anchors.left: layoutrect.left
        border.width: 0
        anchors.right: layoutrect.right
    }
    Rectangle {
        id: dividerline
        x: -3
        y: -1
        width: 632
        height: 1
        color: "#e3e3e3"
        radius: 3
        anchors.rightMargin: 36
        anchors.leftMargin: 36
        visible: true
        anchors.top: whitebox.bottom
        anchors.left: layoutrect.left
        border.width: 0
        anchors.right: layoutrect.right
    }
    property alias label22: label22
    Label {
        id: label22
        color: "#202124"
        visible: false
        text: qsTr("Your nickname")
        font.pixelSize: 14
        font.weight: Font.DemiBold
        font.family: "Montserrat SemiBold"
        anchors.top: whitebox3.top
        anchors.topMargin: 30
        anchors.left: whitebox3.left
        anchors.leftMargin: 30
    }
    property alias receivingnickname2: receivingnickname2
    Label {
        id: receivingnickname2
        color: "#3e45ac"
        text: qsTr("You haven't yet created a Nickname.")
        visible: false
        font.letterSpacing: 0
        font.family: "Montserrat"
        font.pixelSize: 18
        anchors.top: label1.bottom
        anchors.topMargin: 5
        anchors.right: whitebox3.right
        anchors.rightMargin: 30
        anchors.left: whitebox3.left
        anchors.leftMargin: 30
    }

    property alias createnickname: createnickname
    Mybutton {
        id: createnickname
        height: 44
        visible: false
        text: qsTr("Create your nickname")
        font.weight: Font.DemiBold
        font.pixelSize: 14
        font.family: "Montserrat SemiBold"
        rightPadding: 20
        leftPadding: 20
        anchors.top: receivingnickname.bottom
        anchors.topMargin: 20
        anchors.left: whitebox3.left
        anchors.leftMargin: 36
        font.capitalization: Font.MixedCase
    }

    ShadowBox {
        id: whitebox4
        x: -3
        y: -1
        width: 632
        height: 120
        color: "#ffffff"
        radius: 1
        visible: true
        anchors.topMargin: 24
        anchors.top: receivecap.bottom
        anchors.left: layoutrect.left
        border.width: 0
        anchors.right: layoutrect.right
    }
    property alias linkedit: linkedit
    MenuTextField {
        id: linkedit
        height: 48
        anchors.top: whitebox4.top

        color: "#202124"
        font.pixelSize: 14
        font.family: "Montserrat"
        anchors.right: claimcoinsbtn.left
        anchors.rightMargin: 20
        anchors.left: whitebox4.left
        anchors.leftMargin: 36
        padding: 16
        rightPadding: 16
        leftPadding: 16
        topPadding: 16
        placeholderText: qsTr("Paste your link here")
        anchors.topMargin: 36
        selectByMouse: true
    }

    property alias claimcoinsbtn: claimcoinsbtn

    Mybutton {
        id: claimcoinsbtn
        height: 44
        anchors.top: whitebox4.top
        text: qsTr("Receive money")
        anchors.right: whitebox4.right
        anchors.rightMargin: 36
        font.weight: Font.DemiBold
        font.pixelSize: 14
        rightPadding: 20
        anchors.topMargin: 36
        leftPadding: 20
        font.capitalization: Font.MixedCase
        font.family: "Montserrat SemiBold"
    }

    property alias infoboxerror: infoboxerror
    property alias errorlabel: errorlabel

    Rectangle {
        id: infoboxerror
        visible: false
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: 30
        anchors.leftMargin: 30
        anchors.bottomMargin: 30
        height: 44
        width: 240
        radius: 3
        border.width: 0
        color: "#4d505e"
        Label {
            id: errorlabel
            text: ""
            font.family: "Montserrat"
            font.pixelSize: 11
            color: "#ffffff"
            anchors.left: parent.left
            anchors.leftMargin: 20
            anchors.verticalCenter: parent.verticalCenter
        }
        Button {
            id: closebuttonerror
            onClicked: infoboxerror.visible = false
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            width: 16
            height: 16
            background: Image {
                x: 0
                y: 0
                fillMode: Image.PreserveAspectFit
                source: "../res/icons/closebtngrey.png"
            }
        }
    }
}
