import QtQuick 2.7
import QtQuick.Controls 2.0
import QtQuick.Controls.Material 2.0
import QtQuick.Layouts 1.3

Item {
    id: overviewForm
    width: 1280
    height: 800

    property int newsindex: -1
    property string newstitle: ""
    property string newsdesc: ""
    property string newslink: ""

    Timer {
        id: newstimer
        interval: 5000
        running: true
        repeat: true
        onTriggered: {

            newsindex++;
            if (newsindex > newsmodel.count-1 || newsindex > 5) newsindex = 0

            if (newsindex >= 0)
            {
                newstitle = newsmodel.get(newsindex).title
                newsdesc = newsmodel.get(newsindex).desc
                newslink = newsmodel.get(newsindex).link
            }
        }
    }

    Timer {
        id: currencytimer
        interval: 3000
        running: true
        repeat: true
        onTriggered: {
            if (radioButton3.checked)
            {
                switchcurrencies()
            } else
            if (radioButton1.checked)
            {
                currency = 0
                switchcurrencies()
                running = false
            } else
            if (radioButton2.checked)
            {
                currency = 1
                switchcurrencies()
                running = false
            }
        }
    }

    ListModel {
        id: newsmodel
    }

    function sendnewsitemintern(title, desc, link)
    {
        newstitle = title
        newsdesc = desc
        newslink = link

        newsmodel.insert(0,{
                               "title": title,
                               "desc": desc,
                               "link": link
                           })
    }

    onWidthChanged: {
       if (overviewForm.width<1200){
           if (1200-overviewForm.width>70){
               if (1200-overviewForm.width>350){
                   image.width=0
                   image.anchors.rightMargin=0
                   image.visible=false
                   backup.width=0
                   backup.anchors.rightMargin=0
                   backup.visible=false
                   label4.visible=false
                   label42.visible=false
                   label11.visible=false
                   label.visible=false
                   backupimage.visible=false
                   trading.width = 0
                   trading.visible = false
                   trading.anchors.rightMargin = 0
                   label43.visible=false
                   label12.visible=false
               }else {
                   image.width=200;
                   image.anchors.rightMargin=30
                   image.visible=true
                   backup.width=image.width+12;
                   backup.anchors.rightMargin=24
                   backup.visible=true
                   label4.visible=false
                   label42.visible=false
                   label11.visible=false
                   label.visible=false
                   backupimage.visible=false
                   trading.width = backup.width
                   trading.visible = true
                   trading.anchors.rightMargin = 24
                   label43.visible=false
                   label12.visible=false
               }
           }else {
               image.width=420-(1200-overviewForm.width);
               image.visible=true
               image.anchors.rightMargin=30
               backup.width=image.width+12;
               backup.visible=true
               backup.anchors.rightMargin=24
               label4.visible=true
               label42.visible=true
               label11.visible=true
               label.visible=true
               backupimage.visible=true
               trading.width = backup.width
               trading.visible = true
               trading.anchors.rightMargin = 24
               label43.visible=true
               label12.visible=true
           }
       } else {
           image.width = 420
           image.visible = true
           image.anchors.rightMargin = 30
           backup.width = image.width+12
           backup.visible = true
           backup.anchors.rightMargin = 24
           label4.visible = true
           label42.visible = true
           label11.visible = true
           label.visible = true
           backupimage.visible = true
           trading.width = backup.width
           trading.visible = true
           trading.anchors.rightMargin = 24
           label43.visible = true
           label12.visible = true
       }
    }

    signal openLinkintern(string newslink)
    signal sendtobitcashsignalintern()
    signal sendtodollarsignalintern()
    signal sendtogoldsignalintern()
    signal sendtodollarfromgoldsignalintern()
    signal startMiningSignalInternoverview()
    signal startBackupSignalInternoverview()
    signal startTradingSignalInternoverview()

    property int currency : 1

    function switchcurrencies()
    {
        /*currency++;
        if (currency > 2) currency = 1;
        if (currency == 1)
        {
            totalLabelDo.visible = true
            totalLabelGo.visible = false
        } else
        {
            totalLabelDo.visible = false
            totalLabelGo.visible = true
        }
        totalbitcashiconDo.visible = totalLabelDo.visible
        label3Do.visible = totalLabelDo.visible
        pricecaption.visible = totalLabelDo.visible
        priceLabelDo.visible = totalLabelDo.visible
        price2LabelDo.visible = totalLabelDo.visible
        pricebitcashiconDo.visible = totalLabelDo.visible
        price2bitcashiconDo.visible = totalLabelDo.visible
        convertinfonextprice.visible = totalLabelDo.visible
        convertinfonextprice2.visible = totalLabelDo.visible
        labelsupply2.visible = totalLabelDo.visible
        supplyusdLabel.visible = totalLabelDo.visible
        supplyusdicon.visible = totalLabelDo.visible
        availLabelDo.visible = totalLabelDo.visible
        availbitcashiconDo.visible = totalLabelDo.visible
        immatureLabelDo.visible = totalLabelDo.visible
        immaturebitcashiconDo.visible = totalLabelDo.visible
        pendingLabelDo.visible = totalLabelDo.visible
        pendingbitcashiconDo.visible = totalLabelDo.visible
        converttodollarbtn.visible = totalLabelDo.visible
        converttobitcashbtn.visible = totalLabelDo.visible

        totalbitcashiconGo.visible = totalLabelGo.visible
        label4Do.visible = totalLabelGo.visible
        pricecaption2.visible = totalLabelGo.visible
        priceLabelGo.visible = totalLabelGo.visible
        pricebitcashiconGo.visible = totalLabelGo.visible
        labelsupply3.visible = totalLabelGo.visible
        supplygoldLabel.visible = totalLabelGo.visible
        supplygoldicon.visible = totalLabelGo.visible
        availLabelGo.visible = totalLabelGo.visible
        availbitcashiconGo.visible = totalLabelGo.visible
        immatureLabelGo.visible = totalLabelGo.visible
        immaturebitcashiconGo.visible = totalLabelGo.visible
        pendingLabelGo.visible = totalLabelGo.visible
        pendingbitcashiconGo.visible = totalLabelGo.visible
        convertinfonextprice3.visible = totalLabelGo.visible
        convertinfonextprice4.visible = totalLabelGo.visible
        price2bitcashiconGo.visible = totalLabelGo.visible
        price2LabelGo.visible = totalLabelGo.visible
        convertgoldtodollarbtn.visible = totalLabelGo.visible
        convertdollartogoldbtn.visible = totalLabelGo.visible*/
    }

    function setbalancesintern(avail, pending, immature, total) {
        availLabel.text = avail
        pendingLabel.text = pending
        immatureLabel.text = immature
        totalLabel.text = total
    }

    function setbalancesinternDo(avail, pending, immature, total) {
        /*availLabelDo.text = avail
        pendingLabelDo.text = pending
        immatureLabelDo.text = immature
        totalLabelDo.text = total*/
    }

    function setbalancesinternGo(avail, pending, immature, total) {
        /*availLabelGo.text = avail
        pendingLabelGo.text = pending
        immatureLabelGo.text = immature
        totalLabelGo.text = total*/
    }

    function setpriceDointern(price, price2, pricegold) {
        /*priceLabelDo.text = price
        price2LabelDo.text = price2
        priceLabelGo.text = pricegold
        price2LabelGo.text = pricegold*/
    }

    function setwalletvalueintern(value)
    {
        valueLabel.text= value
    }

    function geticonname(nr)
    {
        if (nr===1) return "../res/assets/Type of Transaction/history-type-sent.png";else
        if (nr===2) return "../res/assets/Type of Transaction/history-type-mined.png";else
        return "../res/assets/Type of Transaction/history-type-received.png";
    } 

    function setsupplyintern(bitcash, dollar, gold, blockheight) {
        supplyLabel.text = bitcash        
        blockheightLabel.text = blockheight;
    }

    ShadowBox {
        id: balancerect
        y: 25
        radius: 1
        border.width: 0
        clip: true
        height: 341
        color: "white"
        anchors.left: parent.left
        anchors.leftMargin: 24
        anchors.right: image.left
        anchors.rightMargin: 24       

        Rectangle {
        id: rectangle5
        height: 1
        color: "#eaeaea"
        radius: 10
        anchors.bottomMargin: 10
        anchors.right: immatureLabel.right
        anchors.bottom: label1.top
        anchors.left: label2.left
        anchors.topMargin: 10
        anchors.rightMargin: 0
        anchors.leftMargin: 0
        }

        Label {
            id: label1
            text: qsTr("Available")
            font.weight: Font.Normal
            font.pixelSize: 13
            font.family: "Montserrat"
            anchors.left: label2.left
            anchors.leftMargin: 0
            anchors.top: availLabel.verticalCenter
            anchors.topMargin: -7
            color: "#202124"
        }

        Label {
            id: labelvalue1
            text: qsTr("Value of wallet")
            font.weight: Font.Normal
            font.pixelSize: 13
            font.family: "Montserrat"
            anchors.left: label2.left
            anchors.leftMargin: 0
            anchors.top: valueLabel.top
            anchors.topMargin: 0
            color: "#202124"
        }

        Label {
            id: labelsupply1
            text: qsTr("BitCash supply")
            font.pixelSize: 12
            font.family: "Montserrat Light"
            font.weight: Font.Light
            anchors.left: label2.left
            anchors.leftMargin: 0
            anchors.top: labelvalue1.bottom
            anchors.topMargin: 0
            color: "#202124"
        }

        Label {
            id: labelblockheight
            text: qsTr("Blockheight")
            font.pixelSize: 12
            font.family: "Montserrat Light"
            font.weight: Font.Light
            anchors.left: label2.left
            anchors.leftMargin: 0
            anchors.top: labelsupply1.bottom
            anchors.topMargin: 15
            color: "#202124"
        }

        Label {
            id: label2
            y: 104
            text: qsTr("Pending")
            font.weight: Font.Normal
            font.pixelSize: 13
            font.family: "Montserrat"
            anchors.right: parent.right
            anchors.rightMargin: 250
            anchors.verticalCenter: parent.verticalCenter
            color: "#202124"

            property string toolTipText: qsTr("You have received Bitcash but it has<br> not yet been confirmed by the Bitcash<br> blockchain network. Once it has been<br> confirmed, it will become available for<br> you to spend. This usually takes a few<br> minutes but can take up to one hour.")
            ToolTip.text: toolTipText
            ToolTip.visible: toolTipText ? ma.containsMouse : false
            ToolTip.delay: 100
            ToolTip.timeout: 5000
            MouseArea {
                id: ma
                anchors.fill: parent
                hoverEnabled: true
            }
        }
        Image {
            id: pendinginfo
            width: 16
            height: 16
            anchors.verticalCenter: label2.verticalCenter
            fillMode: Image.PreserveAspectFit
            anchors.left: label2.right
            anchors.leftMargin: 5
            source: "../res/assets/Navigation/help-inactive.png"

            property string toolTipText: qsTr("You have received Bitcash but it has<br> not yet been confirmed by the Bitcash<br> blockchain network. Once it has been<br> confirmed, it will become available for<br> you to spend. This usually takes a few<br> minutes but can take up to one hour.")
            ToolTip.text: toolTipText
            ToolTip.visible: toolTipText ? ma2.containsMouse : false
            ToolTip.delay: 100
            ToolTip.timeout: 5000
            MouseArea {
                id: ma2
                anchors.fill: parent
                hoverEnabled: true
            }
        }

        Label {
            id: valueicon
            anchors.right: valueLabel.left
            anchors.verticalCenter: valueLabel.verticalCenter
            font.pixelSize: 14
            font.family: "Montserrat SemiBold"
            font.weight: Font.DemiBold
            color: "#202124"
            text: qsTr("$")
        }

        Label {
            id: valueLabel
            x: 285
            y: 33
            text: qsTr("32.950000000")
            font.pixelSize: 14
            font.family: "Montserrat SemiBold"
            font.weight: Font.DemiBold
            leftPadding: 0
            anchors.bottom: availLabel.top
            anchors.bottomMargin: 65
            anchors.right: parent.right
            anchors.rightMargin: 36
            color: "#202124"
        }

        Label {
            id: supplyicon
            anchors.right: supplyLabel.left
            anchors.verticalCenter: supplyLabel.verticalCenter
            font.pixelSize: 12
            font.family: "Montserrat Light"
            font.weight: Font.Light
            color: "#202124"
            text: qsTr("₡")
        }

        Label {
            id: supplyLabel
            x: 285
            y: 33
            text: qsTr("-")
            font.pixelSize: 12
            font.family: "Montserrat Light"
            font.weight: Font.Light
            leftPadding: 0
            anchors.top: labelsupply1.top
            anchors.right: parent.right
            anchors.rightMargin: 36
            color: "#202124"
        }

        Label {
            id: blockheightLabel
            x: 285
            y: 33
            text: qsTr("-")
            font.pixelSize: 12
            font.family: "Montserrat Light"
            font.weight: Font.Light
            leftPadding: 0
            anchors.top: labelblockheight.top
            anchors.right: parent.right
            anchors.rightMargin: 36
            color: "#202124"
        }

        Label {
            id: availLabel
            x: 285
            y: 33
            text: qsTr("32.950000000")
            font.pixelSize: 14
            font.family: "Montserrat SemiBold"
            font.weight: Font.DemiBold
            leftPadding: 0
            anchors.bottom: pendingLabel.top
            anchors.bottomMargin: 45
            anchors.right: parent.right
            anchors.rightMargin: 36
            color: "#202124"
        }

        Label {
            id: availbitcashicon
            anchors.right: availLabel.left
            anchors.verticalCenter: availLabel.verticalCenter
            font.pixelSize: 14
            font.family: "Montserrat SemiBold"
            font.weight: Font.DemiBold
            color: "#202124"
            text: qsTr("₡")
        }

        Label {
            id: pendingLabel
            x: 282
            y: 104
            text: qsTr("8764332,956544566")
            anchors.verticalCenterOffset: -1
            font.pixelSize: 14
            font.family: "Montserrat SemiBold"
            font.weight: Font.DemiBold
            leftPadding: 0
            anchors.verticalCenter: parent.verticalCenter
            horizontalAlignment: Text.AlignRight            
            anchors.right: parent.right
            anchors.rightMargin: 36
            color: "#202124"
            Label {
                id: pendingbitcashicon
                anchors.right: pendingLabel.left
                anchors.verticalCenter: pendingLabel.verticalCenter

                font.pixelSize: 14
                font.family: "Montserrat SemiBold"
                font.weight: Font.DemiBold
                color: "#202124"
                text: qsTr("₡")
            }
        }

        Item {
            id: totalbalancerect
            onWidthChanged: {
               totalLabel.font.pixelSize=42
               totalLabel.font.letterSpacing=0;
               totalbitcashicon.font.pixelSize=totalLabel.font.pixelSize

               while (totalLabel.width+totalbitcashicon.width+30+30>totalbalancerect.width && totalLabel.font.pixelSize>13)
               {
                  totalLabel.font.pixelSize--;
                  totalbitcashicon.font.pixelSize=totalLabel.font.pixelSize
               }
               while (totalLabel.width+totalbitcashicon.width+30+30>totalbalancerect.width && totalLabel.font.letterSpacing>-1)
               {
                  totalLabel.font.letterSpacing-=0.05;
               }

               totalLabelDo.font.pixelSize = totalLabel.font.pixelSize
               totalLabelDo.font.letterSpacing = totalLabel.font.letterSpacing
               totalLabelDo.font.letterSpacing = totalLabel.font.letterSpacing
               totalbitcashiconDo.font.pixelSize = totalbitcashicon.font.pixelSize

               totalLabelGo.font.pixelSize = totalLabel.font.pixelSize
               totalLabelGo.font.letterSpacing = totalLabel.font.letterSpacing
               totalLabelGo.font.letterSpacing = totalLabel.font.letterSpacing
               totalbitcashiconGo.font.pixelSize = totalbitcashicon.font.pixelSize

               priceLabelDo.font.pixelSize = totalLabel.font.pixelSize * 2/3
               priceLabelDo.font.letterSpacing = totalLabel.font.letterSpacing * 2/3
               priceLabelDo.font.letterSpacing = totalLabel.font.letterSpacing * 2/3
               price2LabelDo.font.pixelSize = totalLabel.font.pixelSize * 2/3
               price2LabelDo.font.letterSpacing = totalLabel.font.letterSpacing * 2/3
               price2LabelDo.font.letterSpacing = totalLabel.font.letterSpacing * 2/3
               priceLabelGo.font.pixelSize = totalLabel.font.pixelSize * 2/3
               priceLabelGo.font.letterSpacing = totalLabel.font.letterSpacing * 2/3
               priceLabelGo.font.letterSpacing = totalLabel.font.letterSpacing * 2/3
               pricebitcashiconDo.font.pixelSize = totalbitcashicon.font.pixelSize * 2/3
               pricebitcashiconGo.font.pixelSize = totalbitcashicon.font.pixelSize * 2/3
               price2bitcashiconDo.font.pixelSize = totalbitcashicon.font.pixelSize * 2/3
            }
            y: 25
            //radius: 1
            anchors.bottomMargin: 8
            clip: true
            height: 200
            //color: "#00000000"
            anchors.top: parent.top
            anchors.bottom: parent.bottom

            anchors.left: parent.left
            anchors.right: label2.left

            RadioButton {
                id: radioButton1
                text: qsTr("Dollar")
                visible: false
                font.pixelSize: 13
                font.weight: Font.Normal
                font.family: "Montserrat"
                checked: true
                anchors.left: parent.left
                anchors.leftMargin: 30
                anchors.top: parent.top
                anchors.topMargin: 10
                onClicked: {
                    currency = 0;
                    switchcurrencies()
                    currencytimer.running = false
                }
            }
            RadioButton {
                id: radioButton2
                text: qsTr("Gold")
                visible: false
                font.pixelSize: 13
                font.weight: Font.Normal
                font.family: "Montserrat"
                anchors.left: radioButton1.right
                anchors.leftMargin: 8
                anchors.top: parent.top
                anchors.topMargin: 10
                onClicked: {
                    currency = 1;
                    switchcurrencies()
                    currencytimer.running = false
                }
            }
            RadioButton {
                id: radioButton3
                text: qsTr("Switch")
                visible: false
                font.pixelSize: 13
                font.weight: Font.Normal
                font.family: "Montserrat"
                anchors.left: radioButton2.right
                anchors.leftMargin: 8
                anchors.top: parent.top
                anchors.topMargin: 10
                onClicked: {
                    currencytimer.running = true
                }
            }

            Label {
                id: label3
                text: qsTr("Total BitCash balance")
                font.pixelSize: 18
                font.family: "Montserrat SemiBold"
                font.weight: Font.DemiBold
                anchors.top: radioButton1.bottom
                anchors.topMargin: 0
                anchors.left: parent.left
                anchors.leftMargin: 36
                color: "#212225"
            }

            Label {
                id: totalLabel
                text: qsTr("3345154,000000000")
                font.pixelSize: 42
                font.family: "Montserrat Light"
                font.weight: Font.Light
                leftPadding: 0
                anchors.left: totalbitcashicon.right
                anchors.leftMargin: 0
                anchors.top: label3.bottom
                anchors.topMargin: 10
                color: "#3e45ac"
            }
            Label {
                id: totalbitcashicon
                anchors.left: parent.left
                anchors.leftMargin: 36
                anchors.verticalCenter: totalLabel.verticalCenter

                font.pixelSize: 42
                font.family: "Montserrat Light"
                font.weight: Font.Light
                color: "#3e45ac"
                text: qsTr("₡")
            }


        }

        Label {
            id: immaturecaption
            text: qsTr("Immature")
            font.weight: Font.Normal
            font.pixelSize: 13
            font.family: "Montserrat"
            anchors.top: immatureLabel.top
            anchors.topMargin: 0
            anchors.left: label2.left
            anchors.leftMargin: 0
            color: "#202124"
            property string toolTipText: qsTr("You have received Bitcash from mining,<br> but it has not yet been confirmed by<br> the Bitcash blockchain network. Once it<br> has been confirmed, it will become<br> available for you to spend. This usually<br> takes a few minutes but can take up to<br> one hour.")
            ToolTip.text: toolTipText
            ToolTip.visible: toolTipText ? ma3.containsMouse : false
            ToolTip.delay: 100
            ToolTip.timeout: 5000
            MouseArea {
                id: ma3
                anchors.fill: parent
                hoverEnabled: true
            }
        }
        Image {
            id: immatureinfo
            width: 16
            height: 16
            anchors.verticalCenter: immaturecaption.verticalCenter
            fillMode: Image.PreserveAspectFit
            anchors.left: immaturecaption.right
            anchors.leftMargin: 5
            source: "../res/assets/Navigation/help-inactive.png"
            property string toolTipText: qsTr("You have received Bitcash from mining,<br> but it has not yet been confirmed by<br> the Bitcash blockchain network. Once it<br> has been confirmed, it will become<br> available for you to spend. This usually<br> takes a few minutes but can take up to<br> one hour.")
            ToolTip.text: toolTipText
            ToolTip.visible: toolTipText ? ma5.containsMouse : false
            ToolTip.delay: 100
            ToolTip.timeout: 5000
            MouseArea {
                id: ma5
                anchors.fill: parent
                hoverEnabled: true
            }
        }
        Label {
            id: immaturebitcashicon
            anchors.right: immatureLabel.left
            anchors.verticalCenter: immatureLabel.verticalCenter

            font.pixelSize: 14
            font.family: "Montserrat SemiBold"
            font.weight: Font.DemiBold
            color: "#202124"
            text: qsTr("₡")
        }


        Label {
            id: immatureLabel
            x: 282
            text: qsTr("2,453000")
            font.pixelSize: 14
            font.weight: Font.DemiBold
            font.family: "Montserrat SemiBold"
            leftPadding: 0
            anchors.top: pendingLabel.bottom
            anchors.topMargin: 50
            horizontalAlignment: Text.AlignRight            
            anchors.right: parent.right
            anchors.rightMargin: 36
            color: "#202124"
        }



        Rectangle {
            id: rectangle1
            height: 1
            radius: 10
            anchors.top: pendingLabel.bottom
            anchors.topMargin: 35
            color: "#eaeaea"
            anchors.right: immatureLabel.right
            anchors.rightMargin: 0
            anchors.left: label2.left
            anchors.leftMargin: 0
        }

        Rectangle {
            id: rectangle2
            height: 1
            radius: 10
            anchors.bottom: pendingLabel.top
            anchors.bottomMargin: 15
            color: "#eaeaea"
            anchors.right: immatureLabel.right
            anchors.rightMargin: 0
            anchors.left: label2.left
            anchors.leftMargin: 0
        }
    }

    Label {
        id: newheaderid
        text: qsTr("News")
        font.weight: Font.DemiBold
        font.family: "Montserrat SemiBold"
        font.pixelSize: 18
        color:"#202124"
        anchors.top: balancerect.bottom
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.leftMargin: 30
    }

    Label {
        id: newtitleid
        text: newstitle
        font.weight: Font.DemiBold
        font.family: "Montserrat SemiBold"
        font.pixelSize: 13
        color:"#202124"
        anchors.top: balancerect.bottom
        anchors.topMargin: 10
        anchors.left: newheaderid.right
        anchors.leftMargin: 30
        MouseArea {
            anchors.fill: parent
            onClicked: if (newslink != "") openLinkintern(newslink)
        }
    }
    Label {
        id: newdescid
        text: newsdesc
        font.weight: Font.Normal
        font.family: "Montserrat"
        font.pixelSize: 13
        color:"#202124"
        anchors.top: newtitleid.bottom
        anchors.topMargin: 0
        anchors.left: newheaderid.right
        anchors.leftMargin: 30
        MouseArea {
            anchors.fill: parent
            onClicked: if (newslink != "") openLinkintern(newslink)
        }
    }
    Label {
        id: newlinkid
        text: newslink
        font.weight: Font.Normal
        font.family: "Montserrat"
        font.pixelSize: 13
        color:"#3e45ac"
        anchors.top: newdescid.bottom
        anchors.topMargin: 0
        anchors.left: newheaderid.right
        anchors.leftMargin: 30
        MouseArea {
            anchors.fill: parent
            onClicked: if (newslink != "") openLinkintern(newslink)
        }
    }


    property int  datewidth : 150
    property int  addresswidth : 150
    property int  typewidth : 150
    property int  amountwidth : 220
    property int  amountusdwidth : 220
    property int  amountgoldwidth : 220
    Label{
        id: lasttranslabel
        anchors.top: newlinkid.bottom
        anchors.topMargin: 20
        width: 223
        height: 19
        color: "#202124"
        text: qsTr("Last transactions")
        font.pixelSize: 18
        font.family: "Montserrat SemiBold"
        anchors.left: parent.left
        anchors.leftMargin: 30
        font.weight: Font.DemiBold        
    }

    Row {
        id: header
        height: 44
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 30
        anchors.rightMargin: 30
        anchors.top: lasttranslabel.bottom
        anchors.topMargin: 28
        clip: true
        spacing: 0

                        Label {
                            //without this column the background color of the next colum is not displayed???
                            width: 1
                            height: 44
                            text: ""
                        }

                        Label {
                            width: datewidth
                            height: 44
                            text: qsTr("Date")
                            verticalAlignment: Text.AlignVCenter
                            anchors.verticalCenterOffset: 0
                            rightPadding: 20
                            leftPadding: 20
                            anchors.verticalCenter: parent.verticalCenter
                            font.weight: Font.DemiBold
                            font.family: "Montserrat SemiBold"
                            font.pixelSize: 13
                            color:"#202124"
                            padding: 10
                            background: Rectangle { color: "#ebebeb" }
                        }
                        Label {
                            width: typewidth
                            height: 44
                            text: qsTr("Type")
                            verticalAlignment: Text.AlignVCenter
                            rightPadding: 20
                            leftPadding: 20
                            anchors.verticalCenter: parent.verticalCenter
                            font.weight: Font.DemiBold
                            font.family: "Montserrat SemiBold"
                            font.pixelSize: 13
                            color:"#202124"
                            padding: 10
                            background: Rectangle { color: "#ebebeb" }
                        }
                        Label {
                            width: addresswidth
                            height: 44
                            text: qsTr("Address")
                            verticalAlignment: Text.AlignVCenter
                            rightPadding: 20
                            leftPadding: 20
                            anchors.verticalCenter: parent.verticalCenter
                            font.weight: Font.DemiBold
                            font.family: "Montserrat SemiBold"
                            font.pixelSize: 13
                            color:"#202124"
                            padding: 10
                            background: Rectangle { color: "#ebebeb" }
                        }
                        Label {
                            width: overviewForm.width-datewidth-typewidth-addresswidth-amountwidth-amountusdwidth-amountgoldwidth-30-30
                            height: 44
                            text: qsTr("Description")
                            verticalAlignment: Text.AlignVCenter
                            anchors.verticalCenter: parent.verticalCenter
                            rightPadding: 20
                            leftPadding: 20
                            font.family: "Montserrat SemiBold"
                            font.weight: Font.DemiBold
                            font.pixelSize: 13
                            color:"#202124"
                            padding: 10
                            background: Rectangle { color: "#ebebeb" }
                        }
                        Label {
                            width: amountwidth
                            height: 44
                            text: qsTr("BITC")
                            verticalAlignment: Text.AlignVCenter
                            bottomPadding: 10
                            rightPadding: 20
                            leftPadding: 20
                            anchors.verticalCenter: parent.verticalCenter
                            font.weight: Font.DemiBold
                            font.family: "Montserrat SemiBold"
                            horizontalAlignment: Text.AlignRight                            
                            font.pixelSize: 13
                            color:"#202124"
                            padding: 10
                            background: Rectangle { color: "#ebebeb" }
                        }
                        Label {
                            width: amountusdwidth
                            height: 44
                            text: qsTr("USD")
                            verticalAlignment: Text.AlignVCenter
                            bottomPadding: 10
                            rightPadding: 20
                            leftPadding: 20
                            anchors.verticalCenter: parent.verticalCenter
                            font.weight: Font.DemiBold
                            font.family: "Montserrat SemiBold"
                            horizontalAlignment: Text.AlignRight
                            font.pixelSize: 13
                            color:"#202124"
                            padding: 10
                            background: Rectangle { color: "#ebebeb" }
                        }
                        Label {
                            width: amountgoldwidth
                            height: 44
                            text: qsTr("Gold")
                            verticalAlignment: Text.AlignVCenter
                            bottomPadding: 10
                            rightPadding: 20
                            leftPadding: 20
                            anchors.verticalCenter: parent.verticalCenter
                            font.weight: Font.DemiBold
                            font.family: "Montserrat SemiBold"
                            horizontalAlignment: Text.AlignRight
                            font.pixelSize: 13
                            color:"#202124"
                            padding: 10
                            background: Rectangle { color: "#ebebeb" }
                        }

    }

    property alias listView: listView
    ListView {
        id: listView
        anchors.bottomMargin: 0
        anchors.top: header.bottom
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        clip: true
        flickableDirection: Flickable.VerticalFlick

            contentWidth: header.width

            model: overviewtransactions
            delegate: Column {
                id: delegate
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
                            width: datewidth
                            text: ""
                            clip: true
                            Label {
                                text: transactiondate
                                anchors.leftMargin: 20                                
                                anchors.rightMargin: 8                                
                                anchors.left: parent.left
                                anchors.right: parent.right                                
                                anchors.verticalCenter: parent.verticalCenter
                                font.weight: Font.DemiBold
                                font.family: "Montserrat SemiBold"
                                font.pixelSize: 14
                                color: "#202124"
                            }
                        }
                        ItemDelegate {
                            property int column: 1
                            width: typewidth
                            text: ""
                            clip: true

                            Image {
                                id: typimage
                                anchors.left: parent.left
                                anchors.leftMargin: 20
                                anchors.verticalCenter: parent.verticalCenter

                                fillMode: Image.PreserveAspectFit
                                source: geticonname(transactiontypeno)
                            }
                            Label {
                                text: transactiontype
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8                                
                                anchors.left: typimage.right
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                font.weight: Font.Normal
                                font.family: "Montserrat"                                
                                font.pixelSize: 14
                                color: "#202124"
                            }
                        }
                        ItemDelegate {
                            property int column: 2
                            width: addresswidth
                            text: ""
                            clip: true
                            Label {
                                text: transactionaddress
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
                        }
                        ItemDelegate {
                            property int column: 3
                            width: overviewForm.width-datewidth-typewidth-addresswidth-amountwidth-amountusdwidth-amountgoldwidth-30-30
                            text: ""
                            clip: true
                            Label {
                                text: transactionreferenceline
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
                        }
                        ItemDelegate {
                            property int column: 4
                            width: amountwidth
                            text: ""
                            clip: true
                            Label {
                                text: transactionamountbitc
                                anchors.leftMargin: 8
                                anchors.rightMargin: 20                                
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                horizontalAlignment: Text.AlignRight
                                font.family: "Montserrat SemiBold"
                                font.weight: Font.DemiBold
                                font.pixelSize: 14
                                color: "#202124"
                            }
                        }
                        ItemDelegate {
                            property int column: 5
                            width: amountusdwidth
                            text: ""
                            clip: true
                            Label {
                                text: transactionamountusd
                                anchors.leftMargin: 8
                                anchors.rightMargin: 20
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                horizontalAlignment: Text.AlignRight
                                font.family: "Montserrat SemiBold"
                                font.weight: Font.DemiBold
                                font.pixelSize: 14
                                color: "#202124"
                            }
                        }
                        ItemDelegate {
                            property int column: 6
                            width: amountgoldwidth
                            text: ""
                            clip: true
                            Label {
                                text: transactionamountgold
                                anchors.leftMargin: 8
                                anchors.rightMargin: 20
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                horizontalAlignment: Text.AlignRight
                                font.family: "Montserrat SemiBold"
                                font.weight: Font.DemiBold
                                font.pixelSize: 14
                                color: "#202124"
                            }
                        }
                }
                Rectangle {
                    width: parent.width
                    height: 1
                    color: "#ebebeb"
                }
            }

            ScrollIndicator.horizontal: ScrollIndicator { }
            ScrollIndicator.vertical: ScrollIndicator { }
    }


    Rectangle{
        id: image
        x: 354
        y: 31
        width: 420
        height: 170
        anchors.right: parent.right
        anchors.rightMargin: 30
        radius: 3
        border.width: 0
        color: "#3e45ac"

        Image {            
            fillMode: Image.PreserveAspectFit
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            source: "../res/assets/illustration-overview-copy-2.png"
         }
            Label {
                id: label
                y: 24
                text: qsTr("Start mining")
                font.weight: Font.DemiBold
                color: "white"                
                font.pixelSize: 18
                font.family: "Montserrat SemiBold"
                anchors.top: parent.top
                anchors.topMargin: 30
                anchors.left: parent.left
                anchors.leftMargin: 30
            }

            Text {
                id: label4
                x: -4
                y: 5
                height: 54
                text: qsTr("Use your computer to mine and earn BitCash now.")
                wrapMode: Text.WordWrap
                anchors.right: startminingbutton.left
                anchors.rightMargin: 20
                color: "white"
                anchors.topMargin: 5
                font.pixelSize: 13
                font.family: "Montserrat"
                anchors.left: parent.left
                anchors.top: label.bottom
                anchors.leftMargin: 30
            }

            Mybutton {
                id: startminingbutton
                x: 244
                y: 208
                width: 156
                height: 44
                text: qsTr("Start mining")
                font.bold: false
                anchors.verticalCenter: parent.verticalCenter
                font.weight: Font.DemiBold
                font.pixelSize: 14
                rightPadding: 20
                leftPadding: 40
                font.family: "Montserrat SemiBold"
                font.capitalization: Font.MixedCase
                btncolor: "white"
                txtcolor: "#3e45ac"
                btncolordown: "#eaeaea"
                txtcolordown: "#494ea7"
                anchors.right: parent.right
                anchors.rightMargin: 20                
                iconname: "../res/assets/Miscellaneous/button-start-2.png"
                onClicked: startMiningSignalInternoverview()
            }

    }

    ShadowBox{
        id: backup
        x: 824
        width: 432
        anchors.right: parent.right
        anchors.rightMargin: 24
        radius: 1
        anchors.top: image.bottom
        anchors.topMargin: 20
        anchors.bottom: balancerect.bottom
        anchors.bottomMargin: 0
        border.width: 0

        Image {
            id: backupimage
            anchors.leftMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            fillMode: Image.PreserveAspectFit
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            source: "../res/assets/backup-icon-copy.png"
         }
            Label {
                id: label11
                text: qsTr("Backup wallet")
                font.weight: Font.DemiBold
                color: "#202124"
                font.pixelSize: 18
                font.family: "Montserrat SemiBold"
                anchors.top: parent.top
                anchors.topMargin: 30
                anchors.left: parent.left
                anchors.leftMargin: 30
            }

            Text {
                id: label42
                x: -4
                y: 5
                height: 54
                text: qsTr("Save a backup of your wallet as a precaution now.")
                wrapMode: Text.WordWrap
                anchors.right: backupbutton.left
                anchors.rightMargin: 20
                color: "#4f515a"
                anchors.topMargin: 5
                font.pixelSize: 13
                font.family: "Montserrat"
                anchors.left: parent.left
                anchors.top: label11.bottom
                anchors.leftMargin: 30
            }

            Mybutton {
                id: backupbutton
                x: 244
                y: 208
                width: 154
                height: 44
                text: qsTr("Backup wallet")
                font.bold: false
                anchors.verticalCenter: parent.verticalCenter
                font.weight: Font.DemiBold
                font.pixelSize: 14
                rightPadding: 20
                leftPadding: 20
                font.family: "Montserrat SemiBold"
                font.capitalization: Font.MixedCase
                anchors.right: parent.right
                anchors.rightMargin: 20
                onClicked: startBackupSignalInternoverview()
            }

    }

    property alias progressBar: progressBar
    property alias percentlabel: percentlabel
    property alias progressinfolabel: progressinfolabel
    property alias progressinfolabel2: progressinfolabel2
    property alias fullysynced: fullysynced
    property alias fullysyncedimage: fullysyncedimage

    Rectangle {
        id: statusbar
        y: 720
        height: 44
        color: "#ffffff"
        anchors.right: parent.right
        anchors.rightMargin: 0
        anchors.left: parent.left
        anchors.leftMargin: 0
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 0

        ProgressBar {
            id: progressBar
            x: 854
            y: 20

            width: 300
            anchors.right: parent.right
            anchors.rightMargin: 100
            anchors.verticalCenter: parent.verticalCenter
            value: 0.5
            visible: false
        }

        Label {
            id: percentlabel
            x: 1176
            y: 14
            color: "#3e45ac"
            text: qsTr("")
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            font.pixelSize: 13
            font.family: "Montserrat"
            visible: false
        }

        Label {
            id: fullysynced
            x: 1176
            y: 14
            color: "#1bb846"
            text: qsTr("Fully synced.")
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            font.pixelSize: 13
            font.family: "Montserrat"
            visible: true
        }

        Image {
            id: fullysyncedimage
            fillMode: Image.PreserveAspectFit
            anchors.right: fullysynced.left
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            source: "../res/assets/Notification Alerts/success-icon.png"
         }

        Label {
            id: progressinfolabel
            y: 18
            color: "#4d505e"
            text: qsTr("Label")
            anchors.left: parent.left
            anchors.leftMargin: 20
            font.pixelSize: 13
            font.family: "Montserrat"
            anchors.verticalCenter: parent.verticalCenter
            visible: false
        }
        Label {
            id: progressinfolabel2
            y: 18
            color: "#4d505e"
            text: qsTr("Label")
            anchors.left: progressinfolabel.right
            anchors.leftMargin: 20
            font.pixelSize: 13
            font.family: "Montserrat"
            anchors.verticalCenter: parent.verticalCenter
            visible: false
        }

    }
/*
    Rectangle {
        id: newswindow
        color: "#ffffff"
        radius: 5
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        border.color: "#ffffff"
        border.width: 1
        width: 690
        height: 416


        Component {
            id: newsDelegate
            Row {
                spacing: 10
                Text {
                    color: "#202124"
                    font.family: "Montserrat SemiBold"
                    font.weight: Font.DemiBold
                    font.pixelSize: 18
                    text: title
                }
                Text {
                    color: "#202124"
                    font.pixelSize: 16
                    font.family: "Montserrat"
                    text: desc
                }
                Text {
                    color: "#3e45ac"
                    font.pixelSize: 13
                    font.family: "Montserrat"
                    text: link
                }
            }
        }

        ListView {
            anchors.fill: parent
            model: newsmodel
            delegate: newsDelegate
        }
    }
*/

}
