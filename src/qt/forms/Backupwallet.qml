import QtQuick 2.7
import QtQuick.Controls 2.0
import QtQuick.Controls.Material 2.0
import QtQuick.Layouts 1.3

Item {
    id: item1
    visible: false
    width: parent.width
    height: parent.height

    property alias privatekeyEdit: privatekeyEdit
    property alias publicaddressEdit: publicaddressEdit
    property alias printBtn: printBtn
    property real maxavailbalance: maxavailbalance
    property int selectStart
    property int selectEnd
    property int curPos
    property alias swipeView: swipeView
    property alias tabBar: tabBar

    signal printpaperwalletSignalintern()
    signal backupwalletfileSignalintern()
    signal importkeySignalintern(string key, bool fromwebwallet)

    function setpaperwalletaddressesintern(address,privatekey) {
        publicaddressEdit.text=address
        privatekeyEdit.text=privatekey
    }

    function setwalletfolderintern(folder) {
        folderedit.text=folder
    }

    function setimportprogressintern(percent) {
        progressBar.value=percent
    }

    function displayerrormessageintern(msg) {
        errorlabel.text=msg
        infoboxerror.visible=true
        timer.setTimeout(function(){
            infoboxerror.visible=false
        }, 5000);
    }

    Flickable {
        clip: true
        width: parent.width
        height: parent.height
        contentWidth: width
        contentHeight: 1024
        ScrollBar.vertical: ScrollBar {
            active: true;
            width: 10
            onActiveChanged: {
                if (!active)
                active = true;
            }
        }


Rectangle{
    id: screen
    radius: 0
    color: "#000000"
    opacity: 0.5
    border.color: "#00000000"
    border.width: 0
    width: parent.width
    height: parent.height

}
    Rectangle {
        id: window
        color: "#ffffff"
        radius: 5
        anchors.top: screen.top
        anchors.topMargin: 40
        anchors.horizontalCenter: screen.horizontalCenter
    border.color: "#ffffff"
    border.width: 1
    width: 690
    height: 750


        Button {
            id: closebtn
            onClicked: item1.visible = false
            anchors.right: parent.right
            anchors.rightMargin: 30
            width: 16
            height: 16
            anchors.top: parent.top
            anchors.topMargin: 30
            background: Image {
                fillMode: Image.PreserveAspectFit
                source: "../res/assets/Miscellaneous/close-icon.png"
            }
        }

//////
        SwipeView {
            id: swipeView
            clip: true
            //interactive: false
            anchors.top: tabBar.bottom
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            currentIndex: tabBar.currentIndex
            Item {
                id: backup

                Rectangle {
                  id: bluebox
                  anchors.left: backup.left
                  anchors.right: backup.right
                  anchors.top: backup.top
                  anchors.leftMargin: 30
                  anchors.topMargin: 20
                  anchors.rightMargin: 30
                  color: "#e2e3f3"
                  radius: 3
                  border.color: "#3e45ac"
                  border.width: 1

                  height: 103

                  Text {
                      id: text12
                      color: "#3e45ac"
                      text: qsTr("There are 2 ways to backup your wallet. 1. You can save a wallet.dat file and 2. You can print your private key / paper wallet. We suggest you do both and keep them both safe.")
                      anchors.right: parent.right
                      anchors.rightMargin: 30
                      font.weight: Font.DemiBold
                      font.family: "Montserrat SemiBold"
                      lineHeight: 1.5
                      anchors.bottom: parent.bottom
                      anchors.bottomMargin: 20
                      anchors.left: infoicon.right
                      anchors.leftMargin: 10
                      anchors.top: parent.top
                      anchors.topMargin: 20
                      wrapMode: Text.WordWrap
                      font.pixelSize: 13
                  }

                  Image {
                      id: infoicon
                      anchors.top: parent.top
                      anchors.topMargin: 20
                      anchors.leftMargin: 20
                      anchors.left: parent.left
                      fillMode: Image.PreserveAspectFit
                      source: "../res/assets/Notification Alerts/info-icon.png"
                  }
              }

                  Rectangle {
                    id: yellowbox
                    anchors.left: backup.left
                    anchors.right: backup.right
                    anchors.top: textbackuppaper.bottom
                    anchors.leftMargin: 30
                    anchors.topMargin: 15
                    anchors.rightMargin: 30
                    color: "#fdf5e1"
                    radius: 3
                    border.color: "#efb735"
                    border.width: 1

                    height: 103

                    Text {
                        id: text14
                        color: "#d49f26"
                        text: qsTr("Your public address is the address you can share with other people. The private key should never be shared with anyone otherwise they will have access to all your BitCash. This Private key can be imported later on to access your coins.")
                        anchors.right: parent.right
                        anchors.rightMargin: 30
                        font.weight: Font.DemiBold
                        font.family: "Montserrat SemiBold"
                        lineHeight: 1.5
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 20
                        anchors.left: infoicon2.right
                        anchors.leftMargin: 10
                        anchors.top: parent.top
                        anchors.topMargin: 20
                        wrapMode: Text.WordWrap
                        font.pixelSize: 13
                    }

                    Image {
                        id: infoicon2
                        anchors.top: parent.top
                        anchors.topMargin: 20
                        anchors.leftMargin: 20
                        anchors.left: parent.left
                        fillMode: Image.PreserveAspectFit
                        source: "../res/assets/Miscellaneous/info-yellow.png"
                    }
                }

                Label {
                    id: text1
                    color: "#202124"
                    text: qsTr("Public address")
                    font.weight: Font.DemiBold
                    anchors.top: yellowbox.bottom
                    anchors.topMargin: 20
                    anchors.left: backup.left
                    anchors.leftMargin: 36
                    font.family: "Montserrat SemiBold"
                    font.pixelSize: 14
                }
                Label {
                    id: text15
                    color: "#8f9091"
                    text: qsTr("(to receive coins)")
                    anchors.top: yellowbox.bottom
                    anchors.topMargin: 20
                    anchors.left: text1.right
                    anchors.leftMargin: 5
                    font.family: "Montserrat"
                    font.pixelSize: 14
                }

                Label {
                    id: textbackupfile
                    color: "#202124"
                    text: qsTr("Backup your wallet using *.DAT file")
                    font.weight: Font.DemiBold
                    anchors.top: bluebox.bottom
                    anchors.topMargin: 20
                    anchors.left: backup.left
                    anchors.leftMargin: 36
                    font.family: "Montserrat SemiBold"
                    font.pixelSize: 18
                }

                Label {
                    id: textbackuppaper
                    color: "#202124"
                    text: qsTr("Backup your wallet using Paper wallet")
                    font.weight: Font.DemiBold
                    anchors.top: downloadBtn.bottom
                    anchors.topMargin: 40
                    anchors.left: backup.left
                    anchors.leftMargin: 36
                    font.family: "Montserrat SemiBold"
                    font.pixelSize: 18
                }

                MenuTextField {
                    id: publicaddressEdit
                    height: 48
                    anchors.left: parent.left
                    anchors.leftMargin: 30
                    color: acceptableInput ? "green" : "red"
                    text: ""
                    font.pixelSize: 14
                    font.family: "Montserrat"
                    font.letterSpacing: 0
                    bottomPadding: 16
                    padding: 16
                    rightPadding: 16
                    topPadding: 16
                    leftPadding: 16
                    anchors.top: text1.bottom
                    anchors.topMargin: 20
                    anchors.right: copyBtn1.left
                    anchors.rightMargin: 0
                    selectByMouse: true
                    placeholderText: ""
                    readOnly: true
                }

                MenuTextField {
                    id: privatekeyEdit
                    height: 48
                    font.pixelSize: 14
                    font.family: "Montserrat"
                    padding: 16
                    rightPadding: 16
                    leftPadding: 16
                    topPadding: 16
                    anchors.top: text3.bottom
                    anchors.topMargin: 20
                    anchors.left: parent.horizontalCenter
                    anchors.leftMargin: 15
                    anchors.rightMargin: 0
                    anchors.right: copyBtn2.left
                    selectByMouse: true
                    placeholderText: ""
                    readOnly: true
                }

                Label {
                    id: text3
                    color: "#202124"
                    text: qsTr("Private key")
                    font.weight: Font.DemiBold
                    anchors.top: yellowbox.bottom
                    anchors.topMargin: 20
                    anchors.left: parent.horizontalCenter
                    anchors.leftMargin: 15
                    font.pixelSize: 14
                    font.family: "Montserrat SemiBold"
                }

                Label {
                    id: text35
                    color: "#8f9091"
                    text: qsTr("(to spend coins)")
                    anchors.top: yellowbox.bottom
                    anchors.topMargin: 20
                    anchors.left: text3.right
                    anchors.leftMargin: 5
                    font.family: "Montserrat"
                    font.pixelSize: 14
                }

                Mybutton {
                    id: printBtn
                    width: 102
                    height: 44
                    text: qsTr("Print")
                    font.weight: Font.DemiBold
                    font.pixelSize: 14
                    leftPadding: 42
                    anchors.left: backup.left
                    anchors.leftMargin: 36
                    anchors.top: privatekeyEdit.bottom
                    anchors.topMargin: 25
                    font.capitalization: Font.MixedCase
                    font.family: "Montserrat SemiBold"
                    onClicked: printpaperwalletSignalintern()
                    iconname: "../res/assets/Miscellaneous/print-icon.png"
                    btncolor: "white"
                    txtcolor: "#3e45ac"
                    btncolordown: "#eaeaea"
                    txtcolordown: "#494ea7"
                    btnbordercolor: "#3e45ac"
                    borderwidth: 1
                }

                Mybutton {
                    id: downloadBtn
                    width: 200
                    height: 44
                    text: qsTr("Download your file")
                    font.weight: Font.DemiBold
                    font.pixelSize: 14
                    leftPadding: 42
                    anchors.left: backup.left
                    anchors.leftMargin: 36
                    anchors.top: textbackupfile.bottom
                    anchors.topMargin: 15
                    font.capitalization: Font.MixedCase
                    font.family: "Montserrat SemiBold"
                    onClicked: backupwalletfileSignalintern()
                    iconname: "../res/assets/Miscellaneous/download-icon.png"
                }

                Timer {
                    id: timer
                    function setTimeout(cb, delayTime) {
                        timer.interval = delayTime;
                        timer.repeat = false;
                        timer.triggered.connect(cb);
                        timer.triggered.connect(function() {
                            timer.triggered.disconnect(cb); // This is important
                        });
                        timer.start();
                    }
                }
                Mybutton {
                    id: copyBtn1
                    width: 90
                    height: 48
                    text: qsTr("Copy")
                    anchors.right: parent.horizontalCenter
                    anchors.rightMargin: 15
                    font.weight: Font.DemiBold
                    font.pixelSize: 14
                    leftPadding: 20
                    anchors.top: publicaddressEdit.top
                    anchors.topMargin: 0
                    font.capitalization: Font.MixedCase
                    font.family: "Montserrat SemiBold"
                    onClicked: {
                        publicaddressEdit.selectAll()
                        publicaddressEdit.copy()
                        copyBtn1.text="Copied"
                        timer.setTimeout(function(){
                            copyBtn1.text="Copy"
                        }, 3000);
                    }
                    btncolor: "#f7f7f7"
                    txtcolor: "#4d505e"
                    btncolordown: "#eaeaea"
                    txtcolordown: "#494ea7"
                    btnbordercolor: "#dededf"
                    borderwidth: 1
                }
                Mybutton {
                    id: copyBtn2
                    width: 90
                    height: 48
                    text: qsTr("Copy")
                    anchors.right: parent.right
                    anchors.rightMargin: 30
                    font.weight: Font.DemiBold
                    font.pixelSize: 14
                    leftPadding: 20
                    anchors.top: privatekeyEdit.bottom
                    anchors.topMargin: -48
                    font.capitalization: Font.MixedCase
                    font.family: "Montserrat SemiBold"
                    onClicked: {
                        privatekeyEdit.selectAll()
                        privatekeyEdit.copy()
                        copyBtn2.text="Copied"
                        timer.setTimeout(function(){
                            copyBtn2.text="Copy"
                        }, 3000);
                    }
                    btncolor: "#f7f7f7"
                    txtcolor: "#4d505e"
                    btncolordown: "#eaeaea"
                    txtcolordown: "#494ea7"
                    btnbordercolor: "#dededf"
                    borderwidth: 1
                }
            }

            Item {
                id: restore
                Label {
                    id: textrestorefile
                    color: "#202124"
                    text: qsTr("Restore your wallet using *.DAT file")
                    font.weight: Font.DemiBold
                    anchors.top: parent.top
                    anchors.topMargin: 30
                    anchors.left: parent.left
                    anchors.leftMargin: 30
                    font.family: "Montserrat SemiBold"
                    font.pixelSize: 18
                }

                Label {
                    id: infotext2
                    color: "#202124"
                    text: qsTr("Copy your *.DAT file to following directory:")
                    anchors.top: textrestorefile.bottom
                    anchors.topMargin: 10
                    anchors.left: parent.left
                    anchors.leftMargin: 30
                    font.pixelSize: 14
                    font.family: "Montserrat"
                }

                MenuTextField {
                    id: folderedit
                    height: 48
                    font.pixelSize: 14
                    font.family: "Montserrat"
                    padding: 16
                    rightPadding: 16
                    leftPadding: 16
                    topPadding: 16
                    readOnly: true
                    anchors.top: infotext2.bottom
                    anchors.topMargin: 10
                    anchors.left: parent.left
                    anchors.leftMargin: 30
                    anchors.rightMargin: 30
                    anchors.right: parent.right
                    selectByMouse: true
                    placeholderText: ""
                }

                Rectangle {
                    id: line
                    color: "#e3e3e3"
                    height: 1
                    anchors.top: folderedit.bottom
                    anchors.topMargin: 30
                    anchors.left: parent.left
                    anchors.leftMargin: 30
                    anchors.right: parent.right
                    anchors.rightMargin: 30
                }

                Label {
                    id: textrestorepaper
                    color: "#202124"
                    text: qsTr("Restore your wallet using Paper wallet")
                    font.weight: Font.DemiBold
                    anchors.top: line.top
                    anchors.topMargin: 30
                    anchors.left: parent.left
                    anchors.leftMargin: 30
                    font.family: "Montserrat SemiBold"
                    font.pixelSize: 18
                }

                Label {
                    id: infotext
                    color: "#202124"
                    text: qsTr("Paste your private key saved on your paper wallet. The import will take some time.")
                    anchors.top: textrestorepaper.bottom
                    anchors.topMargin: 10
                    anchors.left: parent.left
                    anchors.leftMargin: 30
                    font.pixelSize: 14
                    font.family: "Montserrat"
                }

                Label {
                    id: text34
                    color: "#202124"
                    text: qsTr("Private key")
                    font.weight: Font.DemiBold
                    anchors.top: infotext.bottom
                    anchors.topMargin: 30
                    anchors.left: parent.left
                    anchors.leftMargin: 30
                    font.pixelSize: 14
                    font.family: "Montserrat SemiBold"
                }

                Label {
                    id: text36
                    color: "#8f9091"
                    text: qsTr("(to spend coins)")
                    anchors.top: infotext.bottom
                    anchors.topMargin: 30
                    anchors.left: text34.right
                    anchors.leftMargin: 5
                    font.family: "Montserrat"
                    font.pixelSize: 14
                }

                MenuTextField {
                    id: privatekeyEditpaste
                    height: 48
                    font.pixelSize: 14
                    font.family: "Montserrat"
                    padding: 16
                    rightPadding: 16
                    leftPadding: 16
                    topPadding: 16
                    anchors.top: text34.bottom
                    anchors.topMargin: 10
                    anchors.left: parent.left
                    anchors.leftMargin: 30
                    anchors.rightMargin: 30
                    anchors.right: parent.right
                    selectByMouse: true
                    placeholderText: "Paste your private key here"
                }

                Mybutton {
                    id: restoreBtn
                    width: 271
                    height: 44
                    text: qsTr("Restore from private key")
                    font.weight: Font.DemiBold
                    font.pixelSize: 14
                    leftPadding: 42
                    anchors.left: parent.left
                    anchors.leftMargin: 30
                    anchors.top: privatekeyEditpaste.bottom
                    anchors.topMargin: 20
                    font.capitalization: Font.MixedCase
                    font.family: "Montserrat SemiBold"
                    onClicked: importkeySignalintern(privatekeyEditpaste.text,false)
                    iconname: "../res/assets/Miscellaneous/import-icon.png"
                }

                Mybutton {
                    id: restoreBtn2
                    width: 375
                    height: 44
                    text: qsTr("Restore from web wallet/mobile wallet")
                    font.weight: Font.DemiBold
                    font.pixelSize: 14
                    leftPadding: 42
                    anchors.left: parent.left
                    anchors.leftMargin: 30
                    anchors.top: restoreBtn.bottom
                    anchors.topMargin: 20
                    font.capitalization: Font.MixedCase
                    font.family: "Montserrat SemiBold"
                    onClicked: importkeySignalintern(privatekeyEditpaste.text,true)
                    iconname: "../res/assets/Miscellaneous/import-icon.png"
                }

                ProgressBar {
                    id: progressBar
                    anchors.top: restoreBtn2.bottom
                    anchors.topMargin: 30
                    to: 100
                    anchors.right: parent.right
                    anchors.rightMargin: 30
                    anchors.left: parent.left
                    anchors.leftMargin: 30
                    value: 0
                }
            }
        }

        Image {
            id: sendicon
            y: 66
            anchors.leftMargin: 30
            anchors.verticalCenter: sendcap.verticalCenter
            anchors.left: parent.left
            fillMode: Image.PreserveAspectFit
            source: "../res/assets/Navigation/backup-icon.png"
        }

        Text {
            id: sendcap
            color: "#202124"
            anchors.left: sendicon.right
            anchors.leftMargin: 10
            text: qsTr("Secure your wallet")
            anchors.top: parent.top
            anchors.topMargin: 30
            font.pixelSize: 18
            font.family: "Montserrat SemiBold"
            font.weight: Font.DemiBold
        }

        TabBar {
            id: tabBar
            font.capitalization: Font.MixedCase
            font.family: "Montserrat SemiBold"
            currentIndex: swipeView.currentIndex
            width: parent.width
            height: 60
            position: TabBar.Header
            anchors.top: sendcap.bottom
            anchors.topMargin: 15
            font.weight: Font.DemiBold
            font.pixelSize: 14


            TabButton {
                id: tabButton1
                y: 0
                text: qsTr("Backup wallet")
                rightPadding: 15
                leftPadding: 30
                width: implicitWidth
                height: 60
                contentItem: Text {
                    id: textoverview
                    x: 58
                    text: parent.text
                    font: parent.font
                    opacity: enabled ? 1.0 : 0.3
                    color: "#4d505e"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
                onCheckedChanged: {
                    if (checked) {
                        textoverview.color="#202124"

                    }else {
                        textoverview.color="#4d505e"
                    }
                }

            }

            TabButton {
                id: tabButton
                text: qsTr("Restore wallet")
                rightPadding: 15
                font.capitalization: Font.MixedCase
                leftPadding: 15
                width: implicitWidth
                height: 60

                contentItem: Text {
                    id: textmining
                    text: parent.text
                    font: parent.font
                    opacity: enabled ? 1.0 : 0.3
                    color: "#4d505e"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
                onCheckedChanged: {
                    if (checked) {
                        textmining.color="#202124"
                    }else {
                        textmining.color="#4d505e"
                    }
                }

            }

        }
//////////////


}

    Rectangle{
        id: infoboxerror
        visible: false
        anchors.left:parent.left
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
        Label{
            id: errorlabel
            text: ""
            font.family: "Montserrat"
            font.pixelSize: 11
            color: "#ffffff"
            anchors.left:parent.left
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
                x:0
                y:0
                fillMode: Image.PreserveAspectFit
                source: "../res/icons/closebtngrey.png"
            }
        }
    }

}
}
