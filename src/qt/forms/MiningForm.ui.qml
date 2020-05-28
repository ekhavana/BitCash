import QtQuick 2.7
import QtQuick.Controls 2.0
import QtQuick.Controls.Material 2.0
import QtQuick.Layouts 1.3

Item {
    width: 1000
    height: 800
    id: miningform

    property alias toolBar: toolBar
    property alias toolBar2: toolBar2
    property alias toolBar3: toolBar3
    property alias text11: text11
    property alias text12: text12
    property alias animation1: animation1
    property alias animation2: animation2
    property alias animation3: animation3
    property alias whitebox: whitebox
    property alias mininglog1: mininglog1
    property alias mininglog2: mininglog2
    property alias mininglog3: mininglog3
    property alias mininglog4: mininglog4
    property alias mininglog5: mininglog5
    property alias poolcombo: poolcombo
    property alias miningpoollabel: miningpoollabel

    Rectangle {
        id: toolBar
        y: 0
        height: 44
        anchors.right: parent.right
        anchors.rightMargin: 0
        anchors.left: parent.left
        anchors.leftMargin: 0
        color: "#e2e3f3"
        visible: false

        Image {
            x: 30
            y: 13
            width: 16
            height: 16
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
            background: Image {
                fillMode: Image.PreserveAspectFit
                source: "../res/assets/Notification Alerts/info-close.png"
            }
        }

        Label {
            id: text10
            x: 58
            y: 13
            text: qsTr("Mining BitCash is easy. Simply click the Start mining button and your computer will start earning BitCash.")
            font.pixelSize: 13
            font.family: "Montserrat SemiBold"
            font.weight: Font.DemiBold
            color: "#3e45ac"
        }
    }

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
            text: qsTr("You have successfully earned 0 BITC. It's been automatically added to your total balance.")
            font.pixelSize: 13
            font.family: "Montserrat SemiBold"
            font.weight: Font.DemiBold
            color: "#1bb846"
        }
    }

    Rectangle {
        id: toolBar3
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
            onClicked: toolBar3.visible = false
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
            id: text12
            x: 58
            y: 13
            text: qsTr("Mining information")
            font.pixelSize: 13
            font.family: "Montserrat SemiBold"
            font.weight: Font.DemiBold
            color: "#1bb846"
        }
    }

    AnimatedImage {
        id: animation1
        width: 808
        height: 606
        source: "../res/movies/Part-1.gif"

        fillMode: Image.PreserveAspectFit
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        playing: false
        visible: true
    }
    AnimatedImage {
        height: 606
        transformOrigin: Item.Center
        id: animation2
        width: 808
        source: "../res/movies/Part-2.gif"

        fillMode: Image.PreserveAspectFit
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        playing: false
        visible: false
    }
    AnimatedImage {
        height: 606
        id: animation3
        width: 808
        source: "../res/movies/Part-3.gif"

        fillMode: Image.PreserveAspectFit
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        playing: false
        visible: false
    }

    /*Image {
        id: image
        width: 571
        height: 404
        fillMode: Image.PreserveAspectFit
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        source: "../res/assets/illustration-mining.png"
    }*/
    property alias startminingbutton: startminingbutton
    property alias stopminingbutton: stopminingbutton

    Rectangle {
        id: layoutrect
        anchors.top: toolBar.bottom
        anchors.topMargin: 10
        visible: false
        width: 620
        height: 210
        anchors.horizontalCenter: parent.horizontalCenter
    }

    ShadowBox {
        id: whitebox
        anchors.left: layoutrect.left
        anchors.right: layoutrect.right
        color: "white"
        radius: 3
        anchors.leftMargin: -6
        anchors.top: mininglabel.bottom
        anchors.topMargin: 24
        border.width: 0
        width: 632
        height: 130
    }
    Text {
        id: mininglog1
        y: 74
        anchors.left: whitebox.left
        anchors.leftMargin: 15
        anchors.right: whitebox.right
        anchors.rightMargin: 15
        anchors.top: whitebox.top
        anchors.topMargin: 15
        text: qsTr("Miningf ")
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        font.pixelSize: 12
        font.family: "Montserrat"
        font.weight: Font.Normal
        visible: false
    }
    Text {
        id: mininglog2
        y: 74
        anchors.left: whitebox.left
        anchors.leftMargin: 15
        anchors.right: whitebox.right
        anchors.rightMargin: 15
        anchors.top: mininglog1.bottom
        anchors.topMargin: 0
        text: qsTr("Miningf")
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        font.pixelSize: 12
        font.family: "Montserrat"
        font.weight: Font.Normal
        visible: false
    }
    Text {
        id: mininglog3
        y: 74
        anchors.left: whitebox.left
        anchors.leftMargin: 15
        anchors.right: whitebox.right
        anchors.rightMargin: 15
        anchors.top: mininglog2.bottom
        anchors.topMargin: 0
        text: qsTr("sdfs")
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        font.pixelSize: 12
        font.family: "Montserrat"
        font.weight: Font.Normal
        visible: false
    }
    Text {
        id: mininglog4
        y: 74
        anchors.left: whitebox.left
        anchors.leftMargin: 15
        anchors.right: whitebox.right
        anchors.rightMargin: 15
        anchors.top: mininglog3.bottom
        anchors.topMargin: 0
        text: qsTr("sdfs")
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        font.pixelSize: 12
        font.family: "Montserrat"
        font.weight: Font.Normal
        visible: false
    }
    Text {
        id: mininglog5
        y: 74
        anchors.left: whitebox.left
        anchors.leftMargin: 15
        anchors.right: whitebox.right
        anchors.rightMargin: 15
        anchors.top: mininglog4.bottom
        anchors.topMargin: 0
        text: qsTr("sdfs")
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        font.pixelSize: 12
        font.family: "Montserrat"
        font.weight: Font.Normal
        visible: false
    }

    Image {
        id: miningicon
        y: 74
        anchors.left: layoutrect.left
        width: 24
        height: 24
        anchors.verticalCenter: mininglabel.verticalCenter
        fillMode: Image.PreserveAspectFit
        source: "../res/assets/Headlines/mining.png"
    }

    Text {
        id: mininglabel
        y: 74
        anchors.left: miningicon.right
        anchors.leftMargin: 15
        text: qsTr("Mining")
        font.pixelSize: 20
        font.family: "Montserrat SemiBold"
        font.weight: Font.DemiBold
    }

    Mybutton {
        id: startminingbutton
        x: 609
        anchors.right: layoutrect.right
        y: 66
        //width: 250
        height: 44
        text: qsTr("Start mining")
        font.pixelSize: 16
        anchors.verticalCenter: mininglabel.verticalCenter
        leftPadding: 40
        rightPadding: 20
        iconname: "../res/assets/Miscellaneous/button-start.png"
        font.capitalization: Font.MixedCase
        font.family: "Montserrat"
    }

    Mybutton {
        id: stopminingbutton
        y: 66
        //width: 250
        text: qsTr("Stop mining")
        anchors.right: startminingbutton.right
        anchors.rightMargin: 0
        font.weight: Font.DemiBold
        font.pixelSize: 14
        anchors.verticalCenter: mininglabel.verticalCenter
        leftPadding: 46
        rightPadding: 20
        height: 44
        iconname: "../res/assets/Miscellaneous/button-stop.png"
        font.capitalization: Font.MixedCase
        font.family: "Montserrat SemiBold"
        visible: false
        checkable: false
    }

    property alias informationbtn: informationbtn

    Mybutton {
        id: informationbtn
        x: 527
        height: 44
        text: qsTr("Information before you start")
        visible: false
        anchors.top: usecpuming.bottom
        anchors.topMargin: 10
        anchors.right: startminingbutton.right
        anchors.rightMargin: 0
        font.pixelSize: 16
        leftPadding: 40
        rightPadding: 20
        iconname: "../res/assets/Miscellaneous/info-yellow.png"
        font.capitalization: Font.MixedCase
        font.family: "Montserrat"
    }
    property alias labelgraphs: labelgraphs

    Label {
        id: labelgraphs
        color: "#4d505e"
        anchors.left: whitebox.left
        anchors.leftMargin: 36
        anchors.top: whitebox.top
        anchors.topMargin: 36
        text: qsTr("Hashs per second")
        font.weight: Font.DemiBold
        font.pixelSize: 13
        font.family: "Montserrat SemiBold"
        opacity: 0.5
    }

    property alias graphsstat: graphsstat
    property alias stopanimation: stopanimation
    property alias usecpuming: usecpuming
    property alias cpumininginfobtn: cpumininginfobtn

    Label {
        id: graphsstat
        color: "#3e45ac"
        anchors.left: labelgraphs.left
        anchors.top: labelgraphs.bottom
        anchors.topMargin: 10
        text: qsTr("0.000000")
        font.pixelSize: 18
        font.family: "Montserrat"
        opacity: 0.5
    }

    CheckBox {
        id: stopanimation
        text: qsTr("Stop animation")
        font.weight: Font.DemiBold
        font.pixelSize: 14
        font.family: "Montserrat SemiBold"
        bottomPadding: 5
        topPadding: 5
        rightPadding: 22
        anchors.top: whitebox.bottom
        anchors.topMargin: 2
        checkState: Qt.Unchecked
        leftPadding: 0
        anchors.left: layoutrect.left
        anchors.leftMargin: 0
        hoverEnabled: true
        ToolTip.visible: hovered
        ToolTip.text: qsTr("We recommend checking this option if the animation is slowing down the mining.")
        ToolTip.delay: 100
        ToolTip.timeout: 5000
        Image {
            id: imageinfo2
            width: 16
            height: 16
            antialiasing: true
            anchors.verticalCenter: stopanimation.verticalCenter
            fillMode: Image.PreserveAspectFit
            anchors.right: stopanimation.right
            source: "../res/assets/Miscellaneous/info.png"
        }
    }

    CheckBox {
        id: usecpuming
        text: qsTr("CPU mining of MBC coin and autoexchange to BitCash (zergpool.com)")
        font.weight: Font.DemiBold
        font.pixelSize: 14
        font.family: "Montserrat SemiBold"
        bottomPadding: 5
        topPadding: 5
        rightPadding: 22
        anchors.top: stopanimation.bottom
        anchors.topMargin: 2
        checkState: Qt.Unchecked
        leftPadding: 0
        anchors.left: layoutrect.left
        anchors.leftMargin: 0
        hoverEnabled: true
        ToolTip.visible: hovered
        ToolTip.text: qsTr("Please check this option, if you do not have a GPU installed and still want to mine BitCash with your CPU.")
        ToolTip.delay: 100
        ToolTip.timeout: 5000
        visible: false
        Image {
            id: imageinfo3
            width: 16
            height: 16
            antialiasing: true
            anchors.verticalCenter: usecpuming.verticalCenter
            fillMode: Image.PreserveAspectFit
            anchors.right: usecpuming.right
            source: "../res/assets/Miscellaneous/info.png"
        }
    }

    Mybutton {
        id: cpumininginfobtn
        x: 609
        anchors.left: usecpuming.right
        y: 66
        //width: 250
        height: 44
        text: qsTr("Show stats")
        anchors.leftMargin: 20
        font.pixelSize: 16
        anchors.verticalCenter: usecpuming.verticalCenter
        leftPadding: 20
        rightPadding: 20
        font.capitalization: Font.MixedCase
        font.family: "Courier"
        visible: false
    }

    Label {
        id: miningpoollabel
        color: "#3d3e40"
        text: qsTr("Mining pool")
        font.weight: Font.DemiBold
        anchors.top: usecpuming.bottom
        anchors.topMargin: 40
        anchors.left: whitebox.left
        anchors.leftMargin: 4
        font.pixelSize: 14
        font.family: "Montserrat SemiBold"
        visible: false
    }
    ComboBox {
        id: poolcombo
        height: 48
        anchors.right: whitebox.right
        anchors.rightMargin: 4
        anchors.left: whitebox.left
        anchors.leftMargin: 4
        anchors.top: miningpoollabel.bottom
        anchors.topMargin: 1
        currentIndex: 0
        visible: false
        model: ["stratum+tcp://pool.bsod.pw:2564", "stratum+tcp://eu.bsod.pw:2564", "stratum+tcp://us.bsod.pw:2564", "stratum+tcp://asia.bsod.pw:2564", "stratum+tcp://ru.bsod.pw:2564", "stratum+tcp://x25x.mine.zergpool.com:3225", "stratum+tcp://x25x.mine.zpool.ca:3423", "stratum+tcp://us.miner.tokyo:3257", "stratum+tcp://asia.miner.tokyo:3257", "stratum+tcp://europe.miner.tokyo:3257", "stratum+tcp://ru.miner.tokyo:3257"]
    }

    property alias infowhitebox: infowhitebox

    ShadowBox {
        id: infowhitebox
        anchors.left: layoutrect.left
        anchors.right: layoutrect.right
        color: "white"
        radius: 3
        anchors.leftMargin: -6
        anchors.top: mininglabel.bottom
        anchors.topMargin: -40
        border.width: 0
        width: 632
        height: 488
        visible: false
    }

    property alias mininginfotext: mininginfotext

    Text {
        id: mininginfotext
        y: 74
        anchors.left: infowhitebox.left
        anchors.top: infowhitebox.top
        anchors.right: infowhitebox.right
        anchors.leftMargin: 36
        anchors.rightMargin: 36
        anchors.topMargin: 36
        text: qsTr("Welcome to Bitcash mining!  The one click solution for mining!  \n\nBefore we begin, please take note of a few requirements for mining:\n\nNvidia video card\n980 model or later (minimum 3gb vram)\n\nNow select your pool, click start mining and begin receiving bitcash rewards directly to your wallet!  \n\nThat's it!  Leave you wallet open while mining and enjoy!  \n\nThe algo your mining is x25x, and your miner is T-rex miner which has been integrated into the bitcash wallet.\n\nAny questions please visit us on discord https://discord.gg/t2MdnWQ")
        wrapMode: Text.WrapAtWordBoundaryOrAnywhere
        font.pixelSize: 14
        font.family: "Montserrat"
        font.weight: Font.Normal
        visible: false
    }
    property alias closeinformationbtn: closeinformationbtn

    Mybutton {
        id: closeinformationbtn
        height: 44
        text: qsTr("Close")
        anchors.top: mininginfotext.bottom
        anchors.topMargin: 20
        anchors.left: infowhitebox.left
        anchors.leftMargin: 36
        font.pixelSize: 16
        leftPadding: 40
        rightPadding: 20
        iconname: "../res/assets/Miscellaneous/info-yellow.png"
        font.capitalization: Font.MixedCase
        font.family: "Montserrat"
        visible: false
    }
}
