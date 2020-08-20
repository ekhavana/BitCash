import QtQuick 2.7
import QtQuick.Controls 2.0
import QtQuick.Controls.Material 2.0
import QtQuick.Layouts 1.3
import DestCheckValidator 1.0
import AmountEntryValidator 1.0


Item {
    /*
      recipient
      description
      amount
      day of execution
      next month of execution
     */
    id: paymentsForm
    width: 1280
    height: 900

    property alias senddollarCheckre: senddollarCheckre
    property alias listViewlinks: listViewlinks

    property int  recipientwidth : 200
    property int  btnswidth : 50
    property int  descriptionwidth : 400
    property int  amountwidth : 220
    property int  currencywidth : 70
    property int  daywidth : 75
    property int  monthwidth : 75

    signal createPaymentBtnSignalIntern(string recipient, string description, double amount, int day, int month, bool senddollar)
    signal deletepaymentsignalintern(string idstr)
    signal undopaymentremovalSignalintern()

    function closepaymentsundoinfointern(show) {
        infobox.visible = show
    }
    function addpaymentsintern(id, recipient,desc,amount,day,month,currency) {
        linksmodel.insert(0,{
                               "idstr": id,
                               "recipient": recipient,
                               "description": desc,
                               "amount": amount,
                               "day": day,
                               "month": month,
                               "currency": currency
                           })
    }

    function closelinksundoinfointern(show) {
        infobox.visible=show
    }
    function clearpaymentlistmodelintern() {
        linksmodel.clear()
    }
    function clearpaymentfieldsintern(msg) {
        paytoEdit.text=""
        descriptionEdit.text=""
        amountEdit.text=""
        dayEdit.text=""
        monthEdit.text=""
    }
    Timer {
        id: timer
        function setTimeout(cb, delayTime) {
            timer.interval = delayTime
            timer.repeat = false
            timer.triggered.connect(cb)
            timer.triggered.connect(function () {
                timer.triggered.disconnect(cb) // This is important
            })
            timer.start()
        }
    }
    function displayerrormessageintern(msg) {
        errorlabel.text=msg
        infoboxerror.visible=true
        timer.setTimeout(function(){
            infoboxerror.visible=false
        }, 5000);
    }

    DestCheckValidator {
        // use it
        id: destCheckVal
    }
    AmountEntryValidator {
        // use it
        id: amountCheckVal
    }


    ListModel {
        id: linksmodel

/*        ListElement {
            recipient: "willythecat"
            description: "Test description"
            amount: 23.45
            day: 1
            month: 10
        }
        ListElement {
            recipient: "willythecat"
            description: "Test description 2"
            amount: 12.45
            day: 4
            month: 5
        }
        ListElement {
            recipient: "willythecat"
            description: "Test description 3"
            amount: 2.45
            day: 5
            month: 5
        }*/
    }

    Image {
        id: historyicon
        y: 36
        anchors.verticalCenter: historycap.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 30
        fillMode: Image.PreserveAspectFit
        source: "../res/assets/Headlines/history.png"
    }

    Text {
        id: historycap
        y: 30
        width: 97
        color: "#202124"
        anchors.left: historyicon.right
        anchors.leftMargin: 15
        text: qsTr("Recurring payments")
        font.pixelSize: 20
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
        visible: true

        position: TabBar.Header
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: historycap.bottom
        font.weight: Font.DemiBold
        font.pixelSize: 14

        TabButton {
            id: tabButton1
            y: 0
            text: qsTr("Payments")
            rightPadding: 15
            leftPadding: 30
            width: implicitWidth
            height: 60
            contentItem: Text {
                id: textbitcash
                x: 58
                text: parent.text
                leftPadding: 0
                font: parent.font
                opacity: enabled ? 1.0 : 0.3
                color: "#4d505e"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            /*Image {
                id: imagebitcash
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 35
                fillMode: Image.PreserveAspectFit
                source: "../res/assets/Miscellaneous/bitcashicon.png"
            }*/
        }

        TabButton {
            id: tabButton2
            text: qsTr("Create new recurring payment")
            rightPadding: 15
            font.capitalization: Font.MixedCase
            leftPadding: 15
            width: implicitWidth
            height: 60

            contentItem: Text {
                id: textanyone
                text: parent.text
                font: parent.font
                opacity: enabled ? 1.0 : 0.3
                color: "#4d505e"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            /*Image {
                id: imageanyone
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 15
                fillMode: Image.PreserveAspectFit
                source: "../res/assets/Miscellaneous/anyoneicon.png"
            }*/
        }
    }

    SwipeView {
        id: swipeView
        clip: true
        //interactive: false
        anchors.top: tabBar.bottom
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        currentIndex: tabBar.currentIndex
        Component.onCompleted: contentItem.interactive = false
        Item {
            id: anyonelinke
////////////

            Row {
                id: headerlinks
                clip: true
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 30
                anchors.rightMargin: 30
                anchors.top: parent.top
                anchors.topMargin: 20
                spacing: 0
                height: 44

                Label {
                    //without this column the background color of the next colum is not displayed???
                    width: 1
                    height: 44
                    text: ""
                }

                Label {
                    width: recipientwidth
                    height: 44
                    text: qsTr("Recipient")
                    verticalAlignment: Text.AlignVCenter
                    anchors.verticalCenter: parent.verticalCenter
                    rightPadding: 20
                    leftPadding: 20
                    font.weight: Font.DemiBold
                    font.pixelSize: 13
                    font.family: "Montserrat SemiBold"
                    color:"#202124"
                    padding: 10
                    background: Rectangle { color: "#ebebeb" }
                }
                                Label {
                                    width: paymentsForm.width-amountwidth-recipientwidth-btnswidth-daywidth-monthwidth-currencywidth-30-30
                                    height: 44
                                    text: qsTr("Description")
                                    verticalAlignment: Text.AlignVCenter
                                    anchors.verticalCenter: parent.verticalCenter
                                    rightPadding: 20
                                    leftPadding: 20
                                    font.weight: Font.DemiBold
                                    font.pixelSize: 13
                                    font.family: "Montserrat SemiBold"
                                    color:"#202124"
                                    padding: 10
                                    background: Rectangle { color: "#ebebeb" }
                                }
                                Label {
                                    width: amountwidth
                                    height: 44
                                    text: qsTr("Amount")
                                    verticalAlignment: Text.AlignVCenter
                                    anchors.verticalCenter: parent.verticalCenter
                                    rightPadding: 20
                                    leftPadding: 20
                                    font.weight: Font.DemiBold
                                    horizontalAlignment: Text.AlignRight
                                    font.pixelSize: 13
                                    font.family: "Montserrat SemiBold"
                                    color:"black"
                                    padding: 10
                                    background: Rectangle { color: "#ebebeb" }
                                }
                                Label {
                                    width: currencywidth
                                    height: 44
                                    text: ""
                                    verticalAlignment: Text.AlignVCenter
                                    anchors.verticalCenter: parent.verticalCenter
                                    rightPadding: 20
                                    leftPadding: 20
                                    font.weight: Font.DemiBold
                                    horizontalAlignment: Text.AlignRight
                                    font.pixelSize: 13
                                    font.family: "Montserrat SemiBold"
                                    color:"black"
                                    padding: 10
                                    background: Rectangle { color: "#ebebeb" }
                                }
                                Label {
                                    width: daywidth
                                    height: 44
                                    text: qsTr("Day")
                                    verticalAlignment: Text.AlignVCenter
                                    anchors.verticalCenter: parent.verticalCenter
                                    rightPadding: 20
                                    leftPadding: 20
                                    font.weight: Font.DemiBold
                                    horizontalAlignment: Text.AlignRight
                                    font.pixelSize: 13
                                    font.family: "Montserrat SemiBold"
                                    color:"black"
                                    padding: 10
                                    background: Rectangle { color: "#ebebeb" }
                                }
                                Label {
                                    width: monthwidth
                                    height: 44
                                    text: qsTr("Month")
                                    verticalAlignment: Text.AlignVCenter
                                    anchors.verticalCenter: parent.verticalCenter
                                    rightPadding: 20
                                    leftPadding: 20
                                    font.weight: Font.DemiBold
                                    horizontalAlignment: Text.AlignRight
                                    font.pixelSize: 13
                                    font.family: "Montserrat SemiBold"
                                    color:"black"
                                    padding: 10
                                    background: Rectangle { color: "#ebebeb" }
                                }

                                Label {
                                    width: btnswidth
                                    height: 44
                                    text: ""
                                    verticalAlignment: Text.AlignVCenter
                                    anchors.verticalCenter: parent.verticalCenter
                                    rightPadding: 20
                                    leftPadding: 20
                                    font.weight: Font.DemiBold
                                    font.pixelSize: 13
                                    font.family: "Montserrat SemiBold"
                                    color:"black"
                                    padding: 10
                                    background: Rectangle { color: "#ebebeb" }
                                }
            }

            TextField
            {
                id: copytextfieldlinks
                width: 0
                height: 0
                visible: false
            }

            property alias listViewlinks: listViewlinks
            ListView {
                id: listViewlinks
                anchors.topMargin: 0
                anchors.bottomMargin: 0
                anchors.top: headerlinks.bottom
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                clip: true

                    contentWidth: headerlinks.width
                    flickableDirection: Flickable.VerticalFlick

                    model: linksmodel
                    delegate: Column {
                        id: delegatelinks
                        property int row: index


                        Row {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.leftMargin: 30
                                anchors.rightMargin: 30
                                spacing: 0
                                height: 65

                                ItemDelegate {
                                    property int column: 0
                                    width: recipientwidth
                                    id: recipientpay
                                    text: ""
                                    clip: true
                                    Label {
                                        text: recipient
                                        anchors.leftMargin: 20
                                        anchors.rightMargin: 8
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                        font.family: "Montserrat SemiBold"
                                        color: "#202124"
                                    }
                                    ToolTip.delay: 1000
                                    ToolTip.timeout: 5000
                                    ToolTip.visible: hovered
                                    ToolTip.text: recipient
                                    MouseArea {
                                        anchors.fill: parent
                                        acceptedButtons: Qt.RightButton
                                        hoverEnabled: true
                                        onClicked: {
                                            contextMenulinks.x = mouse.x;
                                            contextMenulinks.y = mouse.y;
                                            contextMenulinks.open();
                                        }
                                        onPressAndHold: {
                                            if (mouse.source === Qt.MouseEventNotSynthesized) {
                                                contextMenulinks.x = mouse.x;
                                                contextMenulinks.y = mouse.y;
                                                contextMenulinks.open();
                                            }
                                        }
                                    }
                                    Menu {
                                        id: contextMenulinks
                                        MenuItem {
                                            text: qsTr("Copy recipient")
                                            onTriggered: {
                                                copytextfieldlinks.text=recipient
                                                copytextfieldlinks.selectAll()
                                                copytextfieldlinks.copy()
                                            }
                                        }
                                    }
                                }

                                ItemDelegate {
                                    property int column: 1
                                    width: paymentsForm.width-amountwidth-recipientwidth-btnswidth-daywidth-monthwidth-currencywidth-30-30
                                    text: ""
                                    Label {
                                        text: description
                                        anchors.leftMargin: 20
                                        anchors.rightMargin: 8
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        font.weight: Font.Normal
                                        font.family: "Montserrat"
                                        font.pixelSize: 14
                                        color: "#202124"
                                    }
                                    clip: true
                                    ToolTip.delay: 1000
                                    ToolTip.timeout: 5000
                                    ToolTip.visible: hovered
                                    ToolTip.text: description
                                    MouseArea {
                                        anchors.fill: parent
                                        acceptedButtons: Qt.RightButton
                                        hoverEnabled: true
                                        onClicked: {
                                            contextMenureflinelinks.x = mouse.x;
                                            contextMenureflinelinks.y = mouse.y;
                                            contextMenureflinelinks.open();
                                        }
                                        onPressAndHold: {
                                            if (mouse.source === Qt.MouseEventNotSynthesized) {
                                                contextMenureflinelinks.x = mouse.x;
                                                contextMenureflinelinks.y = mouse.y;
                                                contextMenureflinelinks.open();
                                            }
                                        }
                                    }
                                    Menu {
                                        id: contextMenureflinelinks
                                        MenuItem {
                                            text: qsTr("Copy description")
                                            onTriggered: {
                                                copytextfieldlinks.text=description
                                                copytextfieldlinks.selectAll()
                                                copytextfieldlinks.copy()
                                            }
                                        }
                                    }

                                }

                                ItemDelegate {
                                    property int column: 2
                                    width: amountwidth
                                    text: ""
                                    Label {
                                        text: amount
                                        anchors.leftMargin: 8
                                        anchors.rightMargin: 20
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        horizontalAlignment: Text.AlignRight

                                        font.family: "Montserrat"
                                        font.bold: true
                                        font.pixelSize: 14
                                        color: "#202124"
                                    }
                                    clip: true
                                    ToolTip.delay: 1000
                                    ToolTip.timeout: 5000
                                    ToolTip.visible: hovered
                                    ToolTip.text: amount
                                    MouseArea {
                                        anchors.fill: parent
                                        acceptedButtons: Qt.RightButton
                                        hoverEnabled: true
                                        onClicked: {
                                            contextMenuamountan.x = mouse.x;
                                            contextMenuamountan.y = mouse.y;
                                            contextMenuamountan.open();
                                        }
                                        onPressAndHold: {
                                            if (mouse.source === Qt.MouseEventNotSynthesized) {
                                                contextMenuamountan.x = mouse.x;
                                                contextMenuamountan.y = mouse.y;
                                                contextMenuamountan.open();
                                            }
                                        }
                                    }
                                    Menu {
                                        id: contextMenuamountan
                                        MenuItem {
                                            text: qsTr("Copy amount")
                                            onTriggered: {
                                                copytextfieldlinks.text=amount
                                                copytextfieldlinks.selectAll()
                                                copytextfieldlinks.copy()
                                            }
                                        }
                                    }
                                }
                                ItemDelegate {
                                    property int column: 3
                                    width: currencywidth
                                    text: ""
                                    Label {
                                        text: currency
                                        anchors.leftMargin: 8
                                        anchors.rightMargin: 20
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        horizontalAlignment: Text.AlignRight

                                        font.family: "Montserrat"
                                        font.bold: true
                                        font.pixelSize: 14
                                        color: "#202124"
                                    }
                                    clip: true
                                    ToolTip.delay: 1000
                                    ToolTip.timeout: 5000
                                    ToolTip.visible: hovered
                                    ToolTip.text: amount
                                    MouseArea {
                                        anchors.fill: parent
                                        acceptedButtons: Qt.RightButton
                                        hoverEnabled: true
                                        onClicked: {
                                            contextMenucurrencyan.x = mouse.x;
                                            contextMenucurrencyan.y = mouse.y;
                                            contextMenucurrencyan.open();
                                        }
                                        onPressAndHold: {
                                            if (mouse.source === Qt.MouseEventNotSynthesized) {
                                                contextMenucurrencyan.x = mouse.x;
                                                contextMenucurrencyan.y = mouse.y;
                                                contextMenucurrencyan.open();
                                            }
                                        }
                                    }
                                    Menu {
                                        id: contextMenucurrencyan
                                        MenuItem {
                                            text: qsTr("Copy amount")
                                            onTriggered: {
                                                copytextfieldlinks.text=currency
                                                copytextfieldlinks.selectAll()
                                                copytextfieldlinks.copy()
                                            }
                                        }
                                    }
                                }
                                ItemDelegate {
                                    property int column: 4
                                    width: daywidth
                                    text: ""
                                    Label {
                                        text: day
                                        anchors.leftMargin: 8
                                        anchors.rightMargin: 20
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        horizontalAlignment: Text.AlignRight

                                        font.family: "Montserrat"
                                        font.bold: true
                                        font.pixelSize: 14
                                        color: "#202124"
                                    }
                                    clip: true
                                    ToolTip.delay: 1000
                                    ToolTip.timeout: 5000
                                    ToolTip.visible: hovered
                                    ToolTip.text: day
                                    MouseArea {
                                        anchors.fill: parent
                                        acceptedButtons: Qt.RightButton
                                        hoverEnabled: true
                                        onClicked: {
                                            contextMenuamountan.x = mouse.x;
                                            contextMenuamountan.y = mouse.y;
                                            contextMenuamountan.open();
                                        }
                                        onPressAndHold: {
                                            if (mouse.source === Qt.MouseEventNotSynthesized) {
                                                contextMenuamountan.x = mouse.x;
                                                contextMenuamountan.y = mouse.y;
                                                contextMenuamountan.open();
                                            }
                                        }
                                    }
                                    Menu {
                                        id: contextMenuday
                                        MenuItem {
                                            text: qsTr("Copy day")
                                            onTriggered: {
                                                copytextfieldlinks.text=day
                                                copytextfieldlinks.selectAll()
                                                copytextfieldlinks.copy()
                                            }
                                        }
                                    }
                                }
                                ItemDelegate {
                                    property int column: 5
                                    width: monthwidth
                                    text: ""
                                    Label {
                                        text: month
                                        anchors.leftMargin: 8
                                        anchors.rightMargin: 20
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        horizontalAlignment: Text.AlignRight

                                        font.family: "Montserrat"
                                        font.bold: true
                                        font.pixelSize: 14
                                        color: "#202124"
                                    }
                                    clip: true
                                    ToolTip.delay: 1000
                                    ToolTip.timeout: 5000
                                    ToolTip.visible: hovered
                                    ToolTip.text: month
                                    MouseArea {
                                        anchors.fill: parent
                                        acceptedButtons: Qt.RightButton
                                        hoverEnabled: true
                                        onClicked: {
                                            contextMenuamountan.x = mouse.x;
                                            contextMenuamountan.y = mouse.y;
                                            contextMenuamountan.open();
                                        }
                                        onPressAndHold: {
                                            if (mouse.source === Qt.MouseEventNotSynthesized) {
                                                contextMenuamountan.x = mouse.x;
                                                contextMenuamountan.y = mouse.y;
                                                contextMenuamountan.open();
                                            }
                                        }
                                    }
                                    Menu {
                                        id: contextMenumonth
                                        MenuItem {
                                            text: qsTr("Copy month")
                                            onTriggered: {
                                                copytextfieldlinks.text=month
                                                copytextfieldlinks.selectAll()
                                                copytextfieldlinks.copy()
                                            }
                                        }
                                    }
                                }
                                ItemDelegate {
                                    property int column: 6
                                    width: btnswidth
                                    text: ""
                                    ToolTip.text: qsTr("Remove payment")
                                    ToolTip.delay: 1000
                                    ToolTip.timeout: 5000
                                    ToolTip.visible: hovered
                                    Button {
                                        anchors.leftMargin: 20
                                        anchors.left: parent.left
                                        anchors.verticalCenter: parent.verticalCenter
                                        onClicked: {
                                            deletepaymentsignalintern(idstr)
                                            linksmodel.remove(index)
                                        }
                                        background: Image {
                                            id: imageforbutton2
                                            fillMode: Image.PreserveAspectFit
                                            source: "../res/assets/Miscellaneous/delete.png"
                                        }
                                    }
                                    clip: true

                                }

                        }
                        Rectangle {
                            width: parent.width
                            height: 1
                            color: Material.accent
                        }
                    }

                    ScrollIndicator.horizontal: ScrollIndicator { }
                    ScrollIndicator.vertical: ScrollIndicator { }
            }

            Rectangle{
                id: infobox
                visible: false
                anchors.left:parent.left
                anchors.bottom:parent.bottom
                anchors.leftMargin: 30
                anchors.bottomMargin: 30
                height: 44
                width: 300
                radius: 3
                border.width: 0
                color: "#4d505e"
                Label{
                    text: qsTr("Removed payment.")
                    font.weight: Font.DemiBold
                    font.pixelSize: 14
                    font.family: "Montserrat SemiBold"
                    color: "#ffffff"
                    anchors.left:parent.left
                    anchors.leftMargin: 20
                    anchors.verticalCenter: parent.verticalCenter
                }
                Button {
                    id: closebutton
                    onClicked: infobox.visible = false
                    anchors.right: parent.right
                    anchors.rightMargin: 20
                    anchors.verticalCenter: parent.verticalCenter
                    width: 16
                    height: 16
                    background:  {
                        x:0
                        y:0
                        fillMode: Image.PreserveAspectFit
                        source: "../res/icons/closebtngrey.png"
                    }
                }
                Mybutton{
                    width: 61
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.right: closebutton.left
                    anchors.rightMargin: 20
                    text: qsTr("Undo")
                    font.weight: Font.DemiBold
                    leftPadding: 10
                    height:28
                    font.capitalization: Font.MixedCase
                    font.family: "Montserrat SemiBold"
                    btncolor: "#ffffff"
                    btncolordown: "#e5e5e5"
                    txtcolor: "#3e45ac"
                    txtcolordown: "#353b9b"
                    onClicked: undopaymentremovalSignalintern()
                }
            }

///////////////
        }
        Item {
            id: transactions
            Rectangle {
                id: layoutrect
                anchors.top: parent.top
                anchors.topMargin: 10
                visible: false
                width: 632
                height: 210
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Image {
                id: sendicon
                y: 66
                anchors.verticalCenter: sendcap.verticalCenter
                anchors.left: layoutrect.left
                fillMode: Image.PreserveAspectFit
                source: "../res/assets/Headlines/history.png"
            }

            Text {
                id: sendcap
                anchors.left: sendicon.right
                anchors.leftMargin: 15
                text: qsTr("Create recurring payment")
                anchors.top: parent.top
                anchors.topMargin: 20
                font.pixelSize: 20
                font.family: "Montserrat SemiBold"
                font.weight: Font.DemiBold
            }

            ShadowBox {
                id: whitebox
                y: 122
                width: 620
                anchors.left: layoutrect.left
                anchors.right: layoutrect.right
                color: "white"
                radius: 3
                visible: true
                border.width: 0
                anchors.top: sendcap.bottom
                anchors.topMargin: 30
                //height: 590
                height: 561

            Label {
                id: text1
                color: "#3d3e40"
                text: qsTr("Recipient")
                font.weight: Font.DemiBold
                anchors.top: parent.top
                anchors.topMargin: 36
                anchors.left: parent.left
                anchors.leftMargin: 36
                font.family: "Montserrat SemiBold"
                font.pixelSize: 14
            }

            MenuTextField {
                id: paytoEdit
                height: 48
                anchors.left: parent.left
                anchors.leftMargin: 36
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
                anchors.right: parent.right
                anchors.rightMargin: 36
                selectByMouse: true
                placeholderText: qsTr("Paste the BitCash address or nickname of the recipient")
                validator: destCheckVal

            }

            MenuTextField {
                id: descriptionEdit
                height: 48
                font.pixelSize: 14
                font.family: "Montserrat"
                padding: 16
                rightPadding: 16
                leftPadding: 16
                topPadding: 16
                anchors.top: text3.bottom
                anchors.topMargin: 20
                anchors.left: parent.left
                anchors.leftMargin: 36
                anchors.rightMargin: 36
                anchors.right: parent.right
                selectByMouse: true
                placeholderText: qsTr("What are you paying for?")
            }

            Label {
                id: text3
                color: "#3d3e40"
                text: qsTr("Description")
                font.weight: Font.DemiBold
                anchors.top: paytoEdit.bottom
                anchors.topMargin: 25
                anchors.left: parent.left
                anchors.leftMargin: 36
                font.pixelSize: 14
                font.family: "Montserrat SemiBold"
            }

            MenuTextField {
                id: amountEdit
                width: 228
                height: 48
                font.pixelSize: 14
                font.family: "Montserrat"
                topPadding: 16
                padding: 16
                anchors.top: text4.bottom
                anchors.topMargin: 20
                anchors.left: parent.left
                anchors.leftMargin: 36
                horizontalAlignment: Text.AlignLeft
                selectByMouse: true
                placeholderText: "0.000000000"
                validator: amountCheckVal
            }
            Rectangle {
                anchors.left: amountEdit.right
                anchors.top: amountEdit.top
                anchors.bottom: amountEdit.bottom
                width: 80
                radius: 4
                border.width: 1
                border.color: "#eeeeef"
                color: "#f7f7f7"
                Text {
                    id: currencyid
                    color: "#4d505e"
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    padding: 16
                    text: qsTr("BITC")
                    font.pixelSize: 14
                    font.weight: Font.Normal
                    font.family: "Montserrat"
                }
            }

            CheckBox {
                id: senddollarCheckre
                text: qsTr("Send Dollar")
                anchors.verticalCenter: amountEdit.verticalCenter
                visible: false
                font.weight: Font.DemiBold
                font.pixelSize: 14
                font.family: "Montserrat SemiBold"
                topPadding: 15
                leftPadding: 0
                anchors.left: amountEdit.right
                anchors.leftMargin: 100
                onCheckStateChanged: {
                    if (senddollarCheckre.checked)
                        currencyid.text="USD"; else
                    currencyid.text="BITC";

                }
            }

            Label {
                id: text4
                color: "#3d3e40"
                text: qsTr("Amount")
                font.weight: Font.DemiBold
                anchors.top: descriptionEdit.bottom
                anchors.topMargin: 25
                anchors.left: parent.left
                anchors.leftMargin: 36
                font.pixelSize: 14
                font.family: "Montserrat SemiBold"
            }


            MenuTextField {
                id: dayEdit
                height: 48
                anchors.right: parent.horizontalCenter
                anchors.rightMargin: 15
                font.pixelSize: 14
                font.family: "Montserrat"
                topPadding: 16
                padding: 16
                anchors.top: text5.bottom
                anchors.topMargin: 20
                anchors.left: parent.left
                anchors.leftMargin: 36
                horizontalAlignment: Text.AlignLeft
                selectByMouse: true
                placeholderText: qsTr("Enter the day (1-31).")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Enter the day of the month. The payment will be excuted every month on the same day.")
                ToolTip.delay: 100
                ToolTip.timeout: 5000
            }

            Label {
                id: text5
                color: "#3d3e40"
                text: qsTr("Day of execution")
                font.weight: Font.DemiBold
                anchors.top: senddollarCheckre.bottom
                anchors.topMargin: 25
                anchors.left: parent.left
                anchors.leftMargin: 36
                font.pixelSize: 14
                font.family: "Montserrat SemiBold"
            }

            MenuTextField {
                id: monthEdit
                height: 48
                anchors.right: parent.right
                anchors.rightMargin: 36
                font.pixelSize: 14
                font.family: "Montserrat"
                topPadding: 16
                padding: 16
                anchors.top: text6.bottom
                anchors.topMargin: 20
                anchors.left: parent.horizontalCenter
                anchors.leftMargin: 15
                horizontalAlignment: Text.AlignLeft
                selectByMouse: true
                placeholderText: qsTr("Enter the month (1-12).")
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Enter the Month of next execution. The payment will be executed the first time in this month.")
                ToolTip.delay: 100
                ToolTip.timeout: 5000
            }

            Label {
                id: text6
                color: "#3d3e40"
                text: qsTr("Month of next execution")
                font.weight: Font.DemiBold
                anchors.top: text5.top
                anchors.topMargin: 0
                anchors.left: parent.horizontalCenter
                anchors.leftMargin: 15
                font.pixelSize: 14
                font.family: "Montserrat SemiBold"
            }

            Mybutton {
                id: sendBtn
                height: 44
                text: qsTr("Create")
                font.weight: Font.DemiBold
                font.pixelSize: 14
                leftPadding: 44
                rightPadding: 20
                anchors.left: parent.left
                anchors.leftMargin: 36
                anchors.top: dayEdit.bottom
                anchors.topMargin: 30
                iconname: "../res/assets/Miscellaneous/button-send.png"
                font.capitalization: Font.MixedCase
                font.family: "Montserrat SemiBold"
                onClicked: createPaymentBtnSignalIntern(paytoEdit.text,descriptionEdit.text,amountEdit.text,dayEdit.text,monthEdit.text, senddollarCheckre.checked)
            }

            }
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
