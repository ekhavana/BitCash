import QtQuick 2.4

ReceiveForm {

    signal claimlinkBtnSignalIntern(string link)
    signal gotocreatenicksignal()

    function displayerrormessageintern(msg) {
        errorlabel.text=msg
        infoboxerror.visible=true
        timer.setTimeout(function(){
            infoboxerror.visible=false
        }, 5000);
    }

    function bitcashexpressclaimedintern() {
        linkedit.text=""
        text11.text=qsTr("You have successfully claimed the coins from the link.")
        toolBar2.visible=true;
        timer.setTimeout(function(){
            toolBar2.visible=false
        }, 5000);
    }

    function getmybitcashaddress()
    {
        if (receivingnickname.text==="")
        {
            return receivingaddress.text
        } else
        {
            return receivingnickname.text
        }
    }

    function getmydollaraddress()
    {
        return "";
    }

    function getmygoldaddress()
    {
        return "";
    }

    function setreceivingaddressintern(address,nick) {
        receivingaddress.text=address        
        receivingnickname.text=nick        
        receivingaddressEdit.text=address        
        receivingnicknameEdit.text=nick        
        if (nick===""){
           whitebox3.visible=true
           createnickname.visible=true
           receivingnickname2.visible=true
           label22.visible=true
        } else
        {
            whitebox3.visible=false
            createnickname.visible=false
            receivingnickname2.visible=false
            label22.visible=false
        }
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


    copynickname.onClicked: {
        receivingnicknameEdit.selectAll()
        receivingnicknameEdit.copy()
        copynickname.text=qsTr("Copied")
        copynickname.leftPadding= 46
        copynickname.iconname="../res/icons/checkblue.png"
        timer.setTimeout(function(){
            copynickname.text=qsTr("Copy ₡ nickname to clipboard")
            copynickname.leftPadding=20
            copynickname.iconname=""
        }, 3000);
    }    
    copyaddress.onClicked: {
        receivingaddressEdit.selectAll()
        receivingaddressEdit.copy()
        copyaddress.text=qsTr("Copied")
        copyaddress.leftPadding= 46
        copyaddress.iconname="../res/icons/checkblue.png"
        timer.setTimeout(function(){
            copyaddress.text=qsTr("Copy ₡ address to clipboard")
            copyaddress.leftPadding=20
            copyaddress.iconname=""
        }, 3000);
    }    
    createnickname.onClicked: gotocreatenicksignal()
    claimcoinsbtn.onClicked: claimlinkBtnSignalIntern(linkedit.text)
}

