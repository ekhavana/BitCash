import QtQuick 2.7
import QtQuick.Controls 2.0
import QtQuick.Controls.Material 2.0
import QtQuick.Layouts 1.3

Item {
    id: item1
    visible: false
    width: parent.width
    height: parent.height

    property alias twitteredit: twitteredit
    property alias text1: text1
    property alias text2: text2
    property alias sendicon: sendicon
    property alias sendcap: sendcap
    property int sendmode
    property alias printBtn: printBtn

    signal printTwitterBillSignalIntern()

    function displayerrormessageintern(msg) {
        errorlabel.text=msg
        infoboxerror.visible=true
        timer.setTimeout(function(){
            infoboxerror.visible=false
        }, 3000);
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
        radius: 2
        anchors.horizontalCenter: screen.horizontalCenter
        anchors.verticalCenter: screen.verticalCenter
    border.color: "#ffffff"
    border.width: 1
    width: 690
    height: 540


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

        Image {
            id: sendicon
            y: 66
            anchors.leftMargin: 30
            anchors.verticalCenter: sendcap.verticalCenter
            anchors.left: parent.left
            fillMode: Image.PreserveAspectFit
            source: "../res/assets/Miscellaneous/twittericon.png"
        }

        Text {
            id: sendcap
            color: "#202124"
            anchors.left: sendicon.right
            anchors.leftMargin: 10
            text: qsTr("Successfully sent the coins to the Twitter user")
            styleColor: "#202124"
            anchors.top: parent.top
            anchors.topMargin: 30
            font.pixelSize: 18
            font.family: "Montserrat SemiBold"
            font.weight: Font.DemiBold
        }
        //////////////


        Label {
            id: text1
            height: 159
            color: "#3d3e40"
            text: qsTr("Send the below link directly to the Twitter user so that he/she can claim the coins.\n\nYou can even paste the link in public tweets or comments. No need to worry, no other user can gain access to these coins except for the intended Twitter user.\n\nIf you submitted this transaction by accident, you can claim the coins back with your link.")
            anchors.right: parent.right
            anchors.rightMargin: 36
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            font.weight: Font.DemiBold
            anchors.top: sendcap.top
            anchors.topMargin: 60
            anchors.left: parent.left
            anchors.leftMargin: 36
            font.family: "Montserrat SemiBold"
            font.pixelSize: 14
        }

        Label {
            id: text5
            x: 36
            color: "#3d3e40"
            text: qsTr("Only the link:")
            font.weight: Font.DemiBold
            anchors.top: text1.bottom
            anchors.topMargin: 20
            anchors.left: parent.left
            anchors.leftMargin: 36
            font.pixelSize: 14
            font.family: "Montserrat SemiBold"
        }

        MenuTextField {
            id: twitteredit
            height: 48
            anchors.left: parent.left
            anchors.leftMargin: 36
            color: "#202124"
            text: "https://wallet.choosebitcash.com/twitterlogin.php"
            font.pixelSize: 14
            font.family: "Montserrat"
            font.letterSpacing: 0
            bottomPadding: 16
            padding: 16
            rightPadding: 16
            topPadding: 16
            leftPadding: 16
            anchors.top: text5.bottom
            anchors.topMargin: 20
            anchors.right: copyBtn1.right
            anchors.rightMargin: 0
            selectByMouse: true
            placeholderText: ""
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Only the link without short message to the recipient.")
            ToolTip.delay: 100
            ToolTip.timeout: 5000
        }

        Mybutton {
            id: copyBtn1
            width: 90
            height: 48
            text: qsTr("Copy")
            anchors.right: parent.right
            anchors.rightMargin: 36
            font.weight: Font.DemiBold
            font.pixelSize: 14
            leftPadding: 20
            anchors.top: twitteredit.top
            anchors.topMargin: 0
            font.capitalization: Font.MixedCase
            font.family: "Montserrat SemiBold"
            onClicked: {
                twitteredit.selectAll()
                twitteredit.copy()
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


        Label {
            id: text4
            x: 36
            color: "#3d3e40"
            text: qsTr("Description together with the link:")
            font.weight: Font.DemiBold
            anchors.top: twitteredit.bottom
            anchors.topMargin: 20
            anchors.left: parent.left
            anchors.leftMargin: 36
            font.pixelSize: 14
            font.family: "Montserrat SemiBold"
        }

        MenuTextField {
            id: text2
            height: 48
            anchors.left: parent.left
            anchors.leftMargin: 36
            color: "#202124"
            text: "https://wallet.choosebitcash.com/twitterlogin.php"
            selectEnd: 1
            font.pixelSize: 14
            font.family: "Montserrat"
            font.letterSpacing: 0
            bottomPadding: 16
            padding: 16
            rightPadding: 16
            topPadding: 16
            leftPadding: 16
            anchors.top: text4.bottom
            anchors.topMargin: 20
            anchors.right: copyBtn2.right
            anchors.rightMargin: 0
            selectByMouse: true
            placeholderText: ""
            ToolTip.visible: hovered
            ToolTip.text: qsTr("A short message to the recipient together with the link.")
            ToolTip.delay: 100
            ToolTip.timeout: 5000
        }

        Mybutton {
            id: copyBtn2
            width: 90
            height: 48
            text: qsTr("Copy")
            anchors.verticalCenter: text2.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: 36
            font.weight: Font.DemiBold
            font.pixelSize: 14
            leftPadding: 20
            font.capitalization: Font.MixedCase
            font.family: "Montserrat SemiBold"
            onClicked: {
                text2.selectAll()
                text2.copy()
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

        Mybutton {
            id: closeBtn2
            height: 44
            text: qsTr("Close")
            font.weight: Font.DemiBold
            font.pixelSize: 14
            leftPadding: 44
            rightPadding: 20
            anchors.left: parent.left
            anchors.leftMargin: 36
            anchors.top: text2.bottom
            anchors.topMargin: 15
            iconname: "../res/assets/Miscellaneous/closesmall.png"
            font.capitalization: Font.MixedCase
            font.family: "Montserrat SemiBold"
            onClicked: item1.visible = false
        }

        Mybutton {
            id: printBtn
            x: 0
            y: 474
            height: 44
            text: qsTr("Print paper bills")
            anchors.top: text2.bottom
            font.pixelSize: 14
            font.family: "Montserrat SemiBold"
            font.capitalization: Font.MixedCase
            anchors.left: closeBtn2.right
            anchors.topMargin: 15
            font.weight: Font.DemiBold
            leftPadding: 20
            rightPadding: 20
            anchors.leftMargin: 20
            onClicked: printTwitterBillSignalIntern()
        }


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
