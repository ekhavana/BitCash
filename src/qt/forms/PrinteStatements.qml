import QtQuick 2.7
import QtQuick.Controls 2.0
import QtQuick.Controls.Material 2.0
import QtQuick.Layouts 1.3

Item {
    id: item1
    width: 1100
    height: 1000

    Image {
        id: receiveicon
        y: 66
        anchors.verticalCenter: receivecap.verticalCenter
        anchors.left: layoutrect.left
        fillMode: Image.PreserveAspectFit
        source: "../res/assets/Miscellaneous/print-icon.png"
    }

    Text {
        id: receivecap
        y: 74
        width: 97
        color: "#202124"
        anchors.left: receiveicon.right
        anchors.leftMargin: 15
        text: qsTr("Print eStatement")
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
        anchors.bottom: whitebox.bottom
        anchors.bottomMargin: -6
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
        anchors.top: receivecap.bottom
        anchors.topMargin: 26
        border.width: 0
        width: 632
        height: 339
    }

    Label {
        id: label
        color: "#202124"
        text: qsTr("Select the month")
        font.pixelSize: 14
        font.weight: Font.DemiBold
        font.family: "Montserrat SemiBold"
        anchors.top: whitebox.top
        anchors.topMargin: 30
        anchors.left: whitebox.left
        anchors.leftMargin: 30
    }

    property alias spinBox: spinBox
    property alias spinBoxyear: spinBoxyear

    SpinBox {
        id: spinBox
        anchors.left: label.left
        anchors.leftMargin: 0
        anchors.top: label.bottom
        from: 1
        anchors.topMargin: 20
        to: 12
        value: 1
        editable: true
    }

    Label {
        id: labelyear
        color: "#202124"
        text: qsTr("Select the year")
        font.pixelSize: 14
        font.weight: Font.DemiBold
        font.family: "Montserrat SemiBold"
        anchors.top: whitebox.top
        anchors.topMargin: 30
        anchors.left: whitebox.horizontalCenter
        anchors.leftMargin: 0
    }

    SpinBox {
        id: spinBoxyear
        anchors.left: labelyear.left
        anchors.leftMargin: 0
        anchors.top: labelyear.bottom
        from: 2018
        anchors.topMargin: 20
        to: 2099
        value: 2019
        editable: true
    }
    property alias printbtn: printbtn

    Mybutton {
        id: printbtn
        height: 44
        text: qsTr("Print")
        font.weight: Font.DemiBold
        font.pixelSize: 14
        leftPadding: 44
        rightPadding: 20
        anchors.left: whitebox.left
        anchors.leftMargin: 30
        anchors.top: radioButtonDollar.bottom
        anchors.topMargin: 30
        iconname: "../res/assets/Miscellaneous/print-icon.png"
        font.capitalization: Font.MixedCase
        font.family: "Montserrat SemiBold"
        btncolor: "white"
        txtcolor: "#3e45ac"
        btncolordown: "#eaeaea"
        txtcolordown: "#494ea7"
        btnbordercolor: "#3e45ac"
        borderwidth: 1
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

    property alias radioButtonBitCash: radioButtonBitCash
    property alias radioButtonDollar: radioButtonDollar

    RadioButton {
        id: radioButtonBitCash
        text: qsTr("Print BitCash statement")
        visible: false
        checked: true
        anchors.top: spinBox.bottom
        anchors.topMargin: 30
        anchors.left: whitebox.left
        anchors.leftMargin: 30
    }

    RadioButton {
        id: radioButtonDollar
        x: 5
        y: 6
        text: qsTr("Print BitCash Dollar statement")
        visible: false
        checked: false
        anchors.top: radioButtonBitCash.bottom
        anchors.topMargin: 0
        anchors.left: whitebox.left
        anchors.leftMargin: 30
    }


}
