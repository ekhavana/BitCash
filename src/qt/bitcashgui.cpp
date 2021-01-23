// Copyright (c) 2011-2017 The Bitcash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/bitcashgui.h>

#include <qt/bitcashunits.h>
#include <qt/clientmodel.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>
#include <qt/modaloverlay.h>
#include <qt/networkstyle.h>
#include <qt/notificator.h>
#include <qt/openuridialog.h>
#include <qt/optionsdialog.h>
#include <qt/optionsmodel.h>
#include <qt/platformstyle.h>
#include <qt/rpcconsole.h>
#include <qt/utilitydialog.h>
#include <qt/transactiondescdialog.h>
#include <qt/transactiontablemodel.h>

#ifdef ENABLE_WALLET
#include <qt/walletframe.h>
#include <qt/walletmodel.h>
#include <qt/walletview.h>
#endif // ENABLE_WALLET

#ifdef Q_OS_MAC
#include <qt/macdockiconhandler.h>
#endif

#include <qt/bitcashaddressvalidator.h>
#include <wallet/coincontrol.h>
#include <amount.h>
#include <rpc/blockchain.h>
#include <utilmoneystr.h>
#include <chainparams.h>
#include <init.h>
#include <interfaces/handler.h>
#include <wallet/walletutil.h>
#include <interfaces/node.h>
#include <miner.h>
#include <links.h>
#include <payments.h>
#include <ui_interface.h>
#include <util.h>
#include <consensus/validation.h>
#include <validation.h>
#include "QtNetwork/QNetworkInterface"

#include <iostream>

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QNetworkReply>
#include <QDateTime>
#include <QDesktopWidget>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressDialog>
#include <QSettings>
#include <QShortcut>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QQuickView>
#include <QQuickStyle>
#include <QQuickWidget>
#include <QQuickItem>
#include <QDesktopServices>
#include <QQmlError>
#include <QtGlobal>
#include <QClipboard>
#include <QPrinter>
#include <QPainter>
#include <QPrintDialog>
#include <QQmlApplicationEngine>
#include <stdio.h>
#include <stdlib.h>
#include <iomanip>
#include <sstream>
#include <key_io.h>
#include <nicknames.h>
#include <links.h>
#include <QFontDatabase>


#ifdef USE_QRCODE
#include <qrencode.h>
#endif

#if QT_VERSION < 0x050000
#include <QTextDocument>
#include <QUrl>
#else
#include <QUrlQuery>
#endif

const std::string BitcashGUI::DEFAULT_UIPLATFORM =
#if defined(Q_OS_MAC)
        "macosx"
#elif defined(Q_OS_WIN)
        "windows"
#else
        "other"
#endif
        ;

QObject *qmlrootitem;
QQmlContext *qmlrootctxt;
QTimer *miningtimer;
QTimer *priceupdatetimer;
QAction *exportAction;

extern std::unique_ptr<CConnman> g_connman;

void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QByteArray localMsg = msg.toLocal8Bit();
    switch (type) {
    case QtDebugMsg:
        fprintf(stderr, "Debug: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        break;
    case QtInfoMsg:
        fprintf(stderr, "Info: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        break;
    case QtWarningMsg:
        fprintf(stderr, "Warning: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        break;
    case QtCriticalMsg:
        fprintf(stderr, "Critical: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        break;
    case QtFatalMsg:
        fprintf(stderr, "Fatal: %s (%s:%u, %s)\n", localMsg.constData(), context.file, context.line, context.function);
        abort();
    }
}

WalletModel* BitcashGUI::getCurrentWalletModel() 
{
    if (!walletFrame) {
        return NULL;
    }
    WalletView * const walletView = walletFrame->currentWalletView();
    if (!walletView) {
        return NULL;
    }
    WalletModel * const walletModel = walletView->getWalletModel();
    return walletModel;
}

void hexchar(unsigned char c, unsigned char &hex1, unsigned char &hex2)
{
    hex1 = c / 16;
    hex2 = c % 16;
    hex1 += hex1 <= 9 ? '0' : 'a' - 10;
    hex2 += hex2 <= 9 ? '0' : 'a' - 10;
}

std::string urlencode(std::string s)
{
    const char *str = s.c_str();
    std::vector<char> v(s.size());
    v.clear();
    for (size_t i = 0, l = s.size(); i < l; i++)
    {
        char c = str[i];
        if ((c >= '0' && c <= '9') ||
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            c == '-' || c == '_' || c == '.' || c == '!' || c == '~' ||
            c == '*' || c == '\'' || c == '(' || c == ')')
        {
            v.push_back(c);
        }
        else if (c == ' ')
        {
            v.push_back('+');
        }
        else
        {
            v.push_back('%');
            unsigned char d1, d2;
            hexchar(c, d1, d2);
            v.push_back(d1);
            v.push_back(d2);
        }
    }

    return std::string(v.cbegin(), v.cend());
}

std::string timestr(time_t t) {
   std::stringstream strm;
   strm << t;
   return strm.str();
}

struct TTransaction {
    qint64 timestamp;
    CAmount amount;
    QString desc, typestr, toaddressstr, amountstr, datestr;
    bool displayaddress;
};

void BitcashGUI::printTwitterBillClicked(QString strlink, int denomination, int curr)
{
    currentlinkforbill = strlink.toStdString();
    currentbilldenomination = denomination;
    currentbillcurrency = curr;

    QVariant returnedValue;
    if (currentbillcurrency == 1) {
        QMetaObject::invokeMethod(qmlrootitem, "startprintingpaperbill", Q_RETURN_ARG(QVariant, returnedValue));
    } else {
        QMetaObject::invokeMethod(qmlrootitem, "startprintingpaperbillbitcash", Q_RETURN_ARG(QVariant, returnedValue));
    }
}

void BitcashGUI::printStatementsBtnClicked(int month, int year, int currency)
{
    std::list<TTransaction> transactions;
    TTransaction trans;

    WalletModel * const walletModel = getCurrentWalletModel();
    if (!walletModel) return;

    TransactionTableModel *ttm = walletModel->getTransactionTableModel();
    if (!ttm || ttm->processingQueuedTransactions())
        return;

    CAmount balancenow = walletModel->wallet().getBalance(currency);
    CAmount balanceafter = 0;
    CAmount balancetosubstract = 0;
    CAmount sentamount = 0;
    CAmount receivedamount = 0;

//    std::cout << "Balance" << FormatMoney(balancenow) << std::endl;

    QDateTime firstdayofmonth = QDateTime(QDate(year, month, 1), QTime(0,0));
    QDateTime lastdayofmonth =  firstdayofmonth.addMonths(1);
    qint64 unixtimefrom = firstdayofmonth.toMSecsSinceEpoch() / 1000;
    qint64 unixtimeto = lastdayofmonth.toMSecsSinceEpoch() / 1000 - 1;
    lastdayofmonth = lastdayofmonth.addDays(-1);

    int rows = ttm->rowCount({});
    for (int row = 0; row < rows; ++row) {
        QModelIndex index = ttm->index(row, TransactionTableModel::Date, {});
        qint64 unixtime = ttm->data(index, Qt::EditRole).toInt();
        if (currency == 1) {
            index = ttm->index(row, TransactionTableModel::Amountusd, {});
        } else  {
            index = ttm->index(row, TransactionTableModel::Amountbitc, {});
        }
        CAmount amount = ttm->data(index, Qt::EditRole).toLongLong();

        if (unixtime >= unixtimefrom && unixtime <= unixtimeto) {
            
            index = ttm->index(row, TransactionTableModel::Ismined, {});
            bool ismined = ttm->data(index, Qt::EditRole).toBool();         
            bool displayaddress = false;

            if (ismined && amount != 0) {
           
                QString amountstr = "";
                if (currency == 1) {
                    index = ttm->index(row, TransactionTableModel::Amountusd, {});
                    amountstr = ttm->data(index, Qt::DisplayRole).toString();

                    //std::cout << "USD:" << amountusdstr.toStdString() << std::endl;
                } else  {
                    index = ttm->index(row, TransactionTableModel::Amountbitc, {});
                    amountstr = ttm->data(index, Qt::DisplayRole).toString();

                    //std::cout << "BITC:" << amountstr.toStdString() << std::endl;
                }
                index = ttm->index(row, TransactionTableModel::Date, {});
                QString datestr = ttm->data(index, Qt::DisplayRole).toString();
                //std::cout << "Date:" << dater.toStdString() << std::endl;

                index = ttm->index(row, TransactionTableModel::Type, {});
                displayaddress = (ttm->data(index, TransactionTableModel::TableTypeAsNumberRole).toInt() == 1);

                index = ttm->index(row, TransactionTableModel::Type, {});
                QString typestr = ttm->data(index, Qt::DisplayRole).toString();

                QString toaddressstr = "";
                if (amount < 0) {
                    index = ttm->index(row, TransactionTableModel::ToAddress, {});
                    toaddressstr = ttm->data(index, Qt::DisplayRole).toString();
                    //std::cout << "ToAddress:" << toaddressstr.toStdString() << std::endl;
                }

                index = ttm->index(row, TransactionTableModel::ReferenceLine, {});
                QString desc = ttm->data(index, Qt::DisplayRole).toString();
                //std::cout << "desc:" << desc.toStdString() << std::endl;

                balancetosubstract += amount;
                trans.timestamp = unixtime;
                trans.amount = amount;
                if (amount < 0) {
                    sentamount += amount;
                } else {
                    receivedamount += amount;
                }
                trans.desc = desc;
                trans.typestr = typestr;
                trans.toaddressstr = toaddressstr;
                trans.amountstr = amountstr;
                trans.datestr = datestr;
                trans.displayaddress = displayaddress;
                transactions.push_back(trans);
            }
        } else
        if (unixtime > unixtimeto) {
            balanceafter += amount;
        }
    }
    
    if (transactions.size() == 0)
    {
        QMessageBox::StandardButton reply = QMessageBox::question(this, "No transactions for this month", 
                                            "There are no transactions for the selected month. Do you want to print the eStatement anyway?", QMessageBox::Yes|QMessageBox::No);
        if (reply == QMessageBox::No) {
            return;
        }

    }


//    std::cout << "Balance to substract" << FormatMoney(balancetosubstract) << std::endl;
//    std::cout << "Balance after" << FormatMoney(balanceafter) << std::endl;
    CAmount balanceatthebeginning = balancenow - balanceafter - balancetosubstract;
    CAmount balanceattheend = balancenow - balanceafter;
//    std::cout << "Balance at the beginning" << FormatMoney(balanceatthebeginning) << std::endl;
//    std::cout << "Balance at the end" << FormatMoney(balanceattheend) << std::endl;

    transactions.sort([](const TTransaction & a, const TTransaction & b) { return a.timestamp < b.timestamp; });

    QPrinter printer;

    QPrintDialog *dialog = new QPrintDialog(&printer, this);
    dialog->setWindowTitle(tr("Print eStatement"));
    if (dialog->exec() != QDialog::Accepted)
        return;
    
    QPainter painter;
    painter.begin(&printer);


    int x, y, margintop, x2, marginright, x3, pagebreakaty, apage, pagenumbery, headery;

    painter.setFont(QFont("Montserrat DemiBold", 14));

    x = 0.5 * printer.resolution();
    y = 0.5 * printer.resolution() + painter.font().pointSize();
    x2 = x + printer.resolution();
    marginright = printer.pageRect().width() - 0.5 * printer.resolution();
    pagebreakaty = printer.pageRect().height() - 2 * printer.resolution();
    pagenumbery = printer.pageRect().height() - 0.25 * printer.resolution();
    headery = 0.25 * printer.resolution();

    x3 = marginright - 1.5 * printer.resolution();
    margintop = y;

    painter.setPen(QColor(0x20,0x21,0x24));
    QString currstr;
    if (currency == 1) currstr = "BitCash Dollar"; else currstr = "BitCash";
    painter.drawText(x, y, QString(currstr + " eStatement %1/%2").arg(QString::number(month)).arg(QString::number(year)));
    y = y + painter.font().pointSize() * 1.5;
    y = y + painter.font().pointSize() * 1.5;

    painter.setFont(QFont("Montserrat", 11));

    QDateTime dt;
    QDate lastdate;
    QRect boundingRect;
    bool isfirst = true;

    apage = 1;
    
    painter.setFont(QFont("Montserrat", 9));

    painter.drawText(QRect(x, pagenumbery - painter.font().pointSize() * 1.5, marginright - x, 100000000), Qt::AlignHCenter, QString("- Page %1 -").arg(QString::number(apage)));
    painter.drawText(QRect(x, headery - painter.font().pointSize() * 1.5, marginright - x, 100000000), Qt::AlignHCenter, 
                        QString(currstr + " eStatement %1/%2").arg(QString::number(month)).arg(QString::number(year)), &boundingRect);

    QImage myImage;
    myImage.load(":/icons/bitcash");

    if (myImage.width() != 0){
        double scale = (x / 2) / double(myImage.width());
        painter.save();
        painter.scale(scale, scale);
        int picx = boundingRect.left();
        int picy = boundingRect.top();
        painter.drawImage(picx/scale-myImage.width(), picy/scale-myImage.height()/6, myImage);
        painter.restore();
    }


    painter.setFont(QFont("Montserrat", 11));

    if (balanceatthebeginning < 0)
        painter.setPen(QColor(0xa2,0x06,0x35));
    else
        painter.setPen(QColor(0x06,0x72,0x35));

    painter.drawText(QRect(x2, y - painter.font().pointSize()*1.5, marginright - x2, 100000000), Qt::AlignRight | 
                Qt::TextWrapAnywhere, QString("Balance on " + firstdayofmonth.date().toString(Qt::SystemLocaleShortDate)+": "+QString::fromStdString(FormatMoney(balanceatthebeginning))));
    painter.setPen(QColor(0x20,0x21,0x24));
    y = y + painter.font().pointSize() * 2.0;


    for (const auto& atrans : transactions)
    {
//    QString desc, typestr, toaddressstr, amountstr, datestr;
        dt.setTime_t(atrans.timestamp);

        if (y > pagebreakaty) {
            printer.newPage();
            apage++;
            isfirst = true;
            y = margintop;
            painter.setFont(QFont("Montserrat", 9));
            painter.drawText(QRect(x, pagenumbery - painter.font().pointSize() * 1.5, marginright - x, 100000000), Qt::AlignHCenter, QString("- Page %1 -").arg(QString::number(apage)));
            painter.drawText(QRect(x, headery - painter.font().pointSize() * 1.5, marginright - x, 100000000), Qt::AlignHCenter, 
                         QString(currstr + " eStatement %1/%2").arg(QString::number(month)).arg(QString::number(year)), &boundingRect);

            if (myImage.width() != 0){
                double scale = (x / 2) / double(myImage.width());
                painter.save();
                painter.scale(scale, scale);
                int picx = boundingRect.left();
                int picy = boundingRect.top();
                painter.drawImage(picx/scale-myImage.width(), picy/scale-myImage.height()/6, myImage);
                painter.restore();
            }

            painter.setFont(QFont("Montserrat", 11));
        }

        int starty = y - painter.font().pointSize() * 1.5;

        if (isfirst || dt.date() != lastdate)
        {
            painter.setFont(QFont("Montserrat DemiBold", 11));
            painter.drawText(x, y, dt.date().toString(Qt::SystemLocaleShortDate));
            y = y + painter.font().pointSize() * 2.0;
            painter.setFont(QFont("Montserrat", 11));
        }
        isfirst = false;
        lastdate = dt.date();

        painter.drawText(x, y, dt.toString("hh:mm"));
        //y = y + painter.font().pointSize()*1.5;

        if (atrans.amount < 0)
            painter.setPen(QColor(0xa2,0x06,0x35));
        else
            painter.setPen(QColor(0x06,0x72,0x35));

        painter.drawText(QRect(x2, y - painter.font().pointSize()*1.5, marginright - x2, 100000000), Qt::AlignRight | Qt::TextWrapAnywhere, atrans.amountstr);

        painter.setPen(QColor(0x20,0x21,0x24));

        painter.drawText(QRect(x2, y - painter.font().pointSize() * 1.5, x3 - x2, 100000000), Qt::AlignLeft | Qt::TextWrapAnywhere, atrans.typestr, &boundingRect);
        y = y + boundingRect.height() + painter.font().pointSize() * 0.5;

        if (atrans.displayaddress) {
            painter.drawText(QRect(x2, y - painter.font().pointSize() * 1.5, x3 - x2, 100000000), Qt::AlignLeft | Qt::TextWrapAnywhere, "To: " + atrans.toaddressstr, &boundingRect);
            y = y + boundingRect.height() + painter.font().pointSize() * 0.5;
        }

        if (atrans.desc != "") {
            painter.setFont(QFont("Montserrat DemiBold", 11));
            y = y + painter.font().pointSize() * 0.5;
            painter.drawText(QRect(x2, y - painter.font().pointSize() * 1.5, x3 - x2, 100000000), Qt::AlignLeft | Qt::TextWrapAnywhere, atrans.desc, &boundingRect);
            y = y + boundingRect.height();
            painter.setFont(QFont("Montserrat", 11));
        }
        y = y + painter.font().pointSize() * 1.5;

        int endy = y - painter.font().pointSize() * 1.5;

        painter.drawLine(x2 - 5, starty, x2 - 5, endy);
//        painter.drawLine(x3 - 5, starty, x3 - 5, endy);
        painter.drawLine(x - 5, starty, x - 5, endy);
        painter.drawLine(marginright + 5, starty, marginright + 5, endy);

        painter.drawLine(x - 5, starty, marginright + 5, starty);
        painter.drawLine(x - 5, endy, marginright + 5, endy);
    }

    if (balanceattheend < 0)
        painter.setPen(QColor(0xa2,0x06,0x35));
    else
        painter.setPen(QColor(0x06,0x72,0x35));

    painter.drawText(QRect(x2, y - painter.font().pointSize()*1.5, marginright - x2, 100000000), Qt::AlignRight | 
                Qt::TextWrapAnywhere, QString("Balance on " + lastdayofmonth.date().toString(Qt::SystemLocaleShortDate)+": "+QString::fromStdString(FormatMoney(balanceattheend))));
    painter.setPen(QColor(0x20,0x21,0x24));
    y = y + painter.font().pointSize() * 2.0;

    painter.drawText(QRect(x2, y - painter.font().pointSize()*1.5, marginright - x2, 100000000), Qt::AlignRight | 
                Qt::TextWrapAnywhere, QString("Sum sent: "+QString::fromStdString(FormatMoney(sentamount))));
    painter.setPen(QColor(0x20,0x21,0x24));
    y = y + painter.font().pointSize() * 2.0;
    painter.drawText(QRect(x2, y - painter.font().pointSize()*1.5, marginright - x2, 100000000), Qt::AlignRight | 
                Qt::TextWrapAnywhere, QString("Sum received: "+QString::fromStdString(FormatMoney(receivedamount))));
    painter.setPen(QColor(0x20,0x21,0x24));
    y = y + painter.font().pointSize() * 2.0;

    painter.end();

}

void BitcashGUI::openpictureClicked()
{
    QString fileName = QFileDialog::getOpenFileName(this,
    tr("Open Image"), "", tr("Image Files (*.png *.jpg *.bmp)"));

    bool currentfaceforbillloaded = currentfaceforbill.load(fileName);

    if (!currentfaceforbillloaded) {
        QMessageBox::critical(this, tr("Error"), tr("Could not load the image!"));
    } else {
        QMessageBox::information(this, tr("Image loaded"), tr("Image successfully loaded!"));
    }
}

QImage BitcashGUI::GetbillbackImage()
{
    QImage myImage;
    bool c;
/*currentbilldenomination = 10;
currentlinkforbill = "test";
currentbillcurrency = 1;*/
    if (currentbillcurrency == 1) {
        if (currentfaceforbill.width() != 0){
            c = myImage.load(QString(":/res/assets/bills/bill%1backown.png").arg(QString::number(currentbilldenomination)));
        } else
        {
            c = myImage.load(QString(":/res/assets/bills/bill%1back.png").arg(QString::number(currentbilldenomination)));
        }
    }else {
        if (currentfaceforbill.width() != 0){
            c = myImage.load(QString(":/res/assets/bills/bitcashbill%1backown.png").arg(QString::number(currentbilldenomination)));
        } else
        {
            c = myImage.load(QString(":/res/assets/bills/bitcashbill%1back.png").arg(QString::number(currentbilldenomination)));
        }
    }

    if (!c) QMessageBox::critical(this, tr("Error"), tr("Could not load bill image!"));

    QPainter painter;
    
#ifdef USE_QRCODE
    //do not really print the QT code on the back side of the bill, but use QT code to calculate the same 
    //scaling for the bill as for the front side.
    QString uri = QString::fromStdString(currentlinkforbill);
    if(!uri.isEmpty())
    {
        // limit URI length
        if (uri.length() > MAX_URI_LENGTH)
        {
            QMessageBox::critical(this, tr("Error"), tr("Resulting URI too long, try to reduce the text for label / message."));
        } else {
            QRcode *code = QRcode_encodeString(uri.toUtf8().constData(), 0, QR_ECLEVEL_L, QR_MODE_8, 1);
            if (!code)
            {
                QMessageBox::critical(this, tr("Error"), tr("Error encoding URI into QR Code."));
                return QImage();
            }
            QImage qrImage = QImage(code->width, code->width, QImage::Format_RGB32);
            qrImage.fill(0xd5e4e9);
            unsigned char *p = code->data;
            for (int y = 0; y < code->width; y++)
            {
                for (int x = 0; x < code->width; x++)
                {
                    qrImage.setPixel(x, y, ((*p & 1) ? 0x0 : 0xd5e4e9));
                    p++;
                }
            }

            int widthqr = code->width * 3;
            QRcode_free(code);

            int widthofimage = widthqr * 6.14 / 0.55;
            int heightofimage = widthofimage * myImage.height() / myImage.width();

            int leftqr = 0.944 * widthofimage / 6.14;
            int topqr = 0.85 * widthofimage / 6.14;

            QImage image(widthofimage, heightofimage, QImage::Format_RGB32);

            painter.begin(&image);

            if (myImage.width() != 0){
                double scale = (widthofimage) / double(myImage.width());
                painter.save();
                painter.scale(scale, scale);
                painter.drawImage(0, 0, myImage);
                painter.restore();
            }

            if (currentfaceforbill.width() != 0){

                int widthim = 1.17 * widthofimage / 6.14;
                int leftim = 4.4 * widthofimage / 6.14;
                int topim = 0.6 * widthofimage / 6.14;

                double scale = (widthim) / double(currentfaceforbill.width());
                painter.save();
                painter.setOpacity(0.5);
                QPainterPath path;
                path.addEllipse(leftim, topim, widthim, widthim);
                painter.setClipPath(path);

                painter.scale(scale, scale);
                painter.drawImage((leftim) / scale, (topim) / scale, currentfaceforbill);

                painter.restore();
            }

            painter.end();
            return image;

        }
    }
#endif
}


void BitcashGUI::savebillbackSignalClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save File"),
                           "billback.png",
                           tr("Images (*.png *.jpg)"));

    
    bool b = GetbillbackImage().save(fileName); 
    if (!b) QMessageBox::critical(this, tr("Error"), tr("Could not save image!"));
}

QImage BitcashGUI::GetbillfrontImage()
{
/*currentbilldenomination = 10;
currentlinkforbill = "test";
currentbillcurrency = 1;*/

    QImage myImage;
    bool c;
    if (currentbillcurrency == 1) {
        c = myImage.load(QString(":/res/assets/bills/bill%1front.png").arg(QString::number(currentbilldenomination)));
    }else {
        c = myImage.load(QString(":/res/assets/bills/bitcashbill%1front.png").arg(QString::number(currentbilldenomination)));
    }

    if (!c) QMessageBox::critical(this, tr("Error"), tr("Could not load bill image!"));

    QPainter painter;
    
#ifdef USE_QRCODE
    QString uri = QString::fromStdString(currentlinkforbill);
    if(!uri.isEmpty())
    {
        // limit URI length
        if (uri.length() > MAX_URI_LENGTH)
        {
            QMessageBox::critical(this, tr("Error"), tr("Resulting URI too long, try to reduce the text for label / message."));
        } else {
            QRcode *code = QRcode_encodeString(uri.toUtf8().constData(), 0, QR_ECLEVEL_L, QR_MODE_8, 1);
            if (!code)
            {
                QMessageBox::critical(this, tr("Error"), tr("Error encoding URI into QR Code."));
                return QImage();
            }
            QImage qrImage = QImage(code->width, code->width, QImage::Format_RGB32);
            qrImage.fill(0xd5e4e9);
            unsigned char *p = code->data;
            for (int y = 0; y < code->width; y++)
            {
                for (int x = 0; x < code->width; x++)
                {
                    qrImage.setPixel(x, y, ((*p & 1) ? 0x0 : 0xd5e4e9));
                    p++;
                }
            }

            int widthqr = code->width * 3;
            QRcode_free(code);

            int widthofimage = widthqr * 6.14 / 0.55;
            int heightofimage = widthofimage * myImage.height() / myImage.width();

            int leftqr = 0.944 * widthofimage / 6.14;
            int topqr = 0.85 * widthofimage / 6.14;

            QImage image(widthofimage, heightofimage, QImage::Format_RGB32);

            painter.begin(&image);

            if (myImage.width() != 0){
                double scale = (widthofimage) / double(myImage.width());
                painter.save();
                painter.scale(scale, scale);
                painter.drawImage(0, 0, myImage);
                painter.restore();
            }

            if (qrImage.width() != 0){
                double scale = (widthqr) / double(qrImage.width());
                painter.save();
                painter.scale(scale, scale);
                painter.drawImage((leftqr) / scale, (topqr) / scale, qrImage);
                painter.restore();
            }

            if (currentfaceforbill.width() != 0){

                int widthim = 1.17 * widthofimage / 6.14;
                int leftim = 4.4 * widthofimage / 6.14;
                int topim = 0.6 * widthofimage / 6.14;

                double scale = (widthim) / double(currentfaceforbill.width());
                painter.save();
                painter.setOpacity(0.5);
                QPainterPath path;
                path.addEllipse(leftim, topim, widthim, widthim);
                painter.setClipPath(path);

                painter.scale(scale, scale);
                painter.drawImage((leftim) / scale, (topim) / scale, currentfaceforbill);

                painter.restore();
            }

            painter.end();
            return image;
        }
    }
#endif

}

void BitcashGUI::savebillSignalClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save File"),
                           "bill.png",
                           tr("Images (*.png *.jpg)"));

    bool b = GetbillfrontImage().save(fileName); 
    if (!b) QMessageBox::critical(this, tr("Error"), tr("Could not save image!"));
}

void BitcashGUI::printfrontbillClicked()
{
    QPrinter printer;

    QPrintDialog *dialog = new QPrintDialog(&printer, this);
    dialog->setWindowTitle(tr("Print Document"));
    if (dialog->exec() != QDialog::Accepted)
        return;
    
    QPainter painter;
    painter.begin(&printer);

/*currentbilldenomination = 10;
currentlinkforbill = "test";
currentbillcurrency = 1;*/
    
    QImage myImage;
    int width = 6.14 * printer.resolution();//6.15 inch long
    qreal margintop = printer.pageRect(QPrinter::DevicePixel).top();
//    printer.setFullPage(true);
    qreal margin = (printer.paperRect(QPrinter::DevicePixel).width() - width) / 2 - printer.pageRect(QPrinter::DevicePixel).left();

    myImage = GetbillfrontImage();

    if (myImage.width() != 0){
        double scale = (width) / double(myImage.width());
        painter.save();
        painter.scale(scale, scale);
        painter.drawImage(margin / scale, margintop / scale, myImage);
        painter.restore();
    }

    painter.end();
}

void BitcashGUI::printbackbillClicked()
{
    QPrinter printer;

    QPrintDialog *dialog = new QPrintDialog(&printer, this);
    dialog->setWindowTitle(tr("Print Document"));
    if (dialog->exec() != QDialog::Accepted)
        return;
    
    QPainter painter;
    painter.begin(&printer);

/*currentbilldenomination = 10;
currentlinkforbill = "test";
currentbillcurrency = 1;*/
    
    QImage myImage;
    int width = 6.14 * printer.resolution();//6.15 inch long
    qreal margintop = printer.pageRect(QPrinter::DevicePixel).top();
//    printer.setFullPage(true);
    qreal margin = (printer.paperRect(QPrinter::DevicePixel).width() - width) / 2 - printer.pageRect(QPrinter::DevicePixel).left();

    myImage = GetbillbackImage();

    if (myImage.width() != 0){
        double scale = (width) / double(myImage.width());
        painter.save();
        painter.scale(scale, scale);
        painter.drawImage(margin / scale, margintop / scale, myImage);
        painter.restore();
    }

    painter.end();
}

void BitcashGUI::generateBillClicked(int denomination)
{
    if (!UserKnowsPassword()) return;
    WalletModel * const walletModel = getCurrentWalletModel();
    if (!walletModel) return;

    if (denomination == 1 || denomination == 2 || denomination == 5 || denomination == 10 || denomination == 20 || denomination == 50 || denomination == 100)
    {
        CAmount nAmount = denomination * COIN;
        std::string referenceline = "Paper bill"; 
        std::string strlink, strerr;
   
        int curr = 1;

        if (!walletModel->wallet().SendAsLink(nAmount, referenceline, strlink, strerr, curr, curr))
        {
            QMessageBox::critical(this, tr("Could not create link."),QString::fromStdString(strerr));   
        } else {
            QVariant returnedValue;
            const QString& s = QString::fromStdString(strlink);
            QDateTime currentDate = QDateTime::currentDateTime();
            uint64_t timestamp=currentDate.toMSecsSinceEpoch();
            int unit = walletModel->getOptionsModel()->getDisplayUnit();
            QVariant amountstr=BitcashUnits::format(unit, nAmount, false, BitcashUnits::separatorNever);       
                                                
            SetLink(strlink, referenceline, amountstr.toString().toStdString(), 0, timestamp, curr);

            QDateTime qtimestamp;
            qtimestamp.setMSecsSinceEpoch(timestamp);
            QVariant datestr=GUIUtil::dateTimeStr(qtimestamp);
            QVariant currencystr = "USD";

            QMetaObject::invokeMethod(qmlrootitem, "addbitcashexpresslink", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s), Q_ARG(QVariant, QString::fromStdString(referenceline)), Q_ARG(QVariant, amountstr), Q_ARG(QVariant, datestr), Q_ARG(QVariant, currencystr));

            QClipboard *p_Clipboard = QApplication::clipboard();
            p_Clipboard->setText(s);

            currentlinkforbill = strlink;
            currentbilldenomination = denomination;
            currentbillcurrency = curr;
            QMetaObject::invokeMethod(qmlrootitem, "startprintingpaperbill", Q_RETURN_ARG(QVariant, returnedValue));
       }
  
    }
}

void BitcashGUI::generateBillBitCashClicked(int denomination)
{
    if (!UserKnowsPassword()) return;
    WalletModel * const walletModel = getCurrentWalletModel();
    if (!walletModel) return;

    if (denomination == 10 || denomination == 20 || denomination == 50 || denomination == 100 || denomination == 200 || denomination == 500 || denomination == 1000)
    {
        CAmount nAmount = denomination * COIN;
        std::string referenceline = "Paper bill"; 
        std::string strlink, strerr;
   
        int curr = 0;

        if (!walletModel->wallet().SendAsLink(nAmount, referenceline, strlink, strerr, curr, curr))
        {
            QMessageBox::critical(this, tr("Could not create link."),QString::fromStdString(strerr));   
        } else {
            QVariant returnedValue;
            const QString& s = QString::fromStdString(strlink);
            QDateTime currentDate = QDateTime::currentDateTime();
            uint64_t timestamp=currentDate.toMSecsSinceEpoch();
            int unit = walletModel->getOptionsModel()->getDisplayUnit();
            QVariant amountstr=BitcashUnits::format(unit, nAmount, false, BitcashUnits::separatorNever);       
                                                
            SetLink(strlink, referenceline, amountstr.toString().toStdString(), 0, timestamp, curr);

            QDateTime qtimestamp;
            qtimestamp.setMSecsSinceEpoch(timestamp);
            QVariant datestr=GUIUtil::dateTimeStr(qtimestamp);
            QVariant currencystr = "BITC";

            QMetaObject::invokeMethod(qmlrootitem, "addbitcashexpresslink", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s), Q_ARG(QVariant, QString::fromStdString(referenceline)), Q_ARG(QVariant, amountstr), Q_ARG(QVariant, datestr), Q_ARG(QVariant, currencystr));

            QClipboard *p_Clipboard = QApplication::clipboard();
            p_Clipboard->setText(s);

            currentlinkforbill = strlink;
            currentbilldenomination = denomination;
            currentbillcurrency = curr;
            QMetaObject::invokeMethod(qmlrootitem, "startprintingpaperbillbitcash", Q_RETURN_ARG(QVariant, returnedValue));
       }
  
    }
}

int sendmode = 0;

void BitcashGUI::InstaSwapSendBtnClicked(bool buybitcash, double amount, QString bitcoinaddress)
{
    WalletModel * const walletModel = getCurrentWalletModel();
    if (!walletModel) return;

    CPubKey pkey = walletModel->wallet().GetCurrentAddressPubKey();
    CTxDestination dest = PubKeyToDestination(pkey);
        
    QString bitcashaddress = QString::fromStdString(EncodeDestination(dest, pkey));

    if (amount <= 0)
    {
        QVariant returnedValue;
        QVariant msg = tr("Please enter a valid amount of coins you want to swap.");
        QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));
        return;
    }

    if (buybitcash) {
        sendmode = 11;
        lastamountforinstaswap = QString::number(amount, 'f', 4);

        if (amount * 10000 != (int)(amount * 10000)) {
            QVariant returnedValue;
            QVariant msg = tr("You can only use 4 decimal places when you select the amount.");
            QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));
            return;
        }

    }else {
        sendmode = 10;

        if (amount != (int)amount) {
            QVariant returnedValue;
            QVariant msg = tr("You can not use any decimal places when you select the amount.");
            QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));
            return;
        }

        if (amount * COIN > walletModel->wallet().getBalance(0))
        {
            QVariant returnedValue;
            QVariant msg = tr("This amount exceeds your current BitCash balance.");
            QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));
            return;
        }

        if (bitcoinaddress == "")
        {
            QVariant returnedValue;
            QVariant msg = tr("Please enter your Bitcoin address.");
            QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));
            return;
        }
        lastamountforinstaswap = QString::number(amount, 'f', 0);
    }
    QUrlQuery postData;    
    postData.addQueryItem("amount", lastamountforinstaswap);
    postData.addQueryItem("btcaddress", bitcoinaddress);
    postData.addQueryItem("bitcaddress", bitcashaddress);
    if (buybitcash) {
        postData.addQueryItem("buybitcash", "1");
    } else
    {
        postData.addQueryItem("buybitcash", "0");
    }
    postData.addQueryItem("sent", "1");

    QNetworkRequest request(QUrl("https://price.choosebitcash.com/instaswap/starttransaction.php"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    this->managerinstaswap->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());
}

void BitcashGUI::InstaSwapCheckAmountClicked(bool buybitcash, double amount)
{
    if (amount <= 0)
    {
        QVariant returnedValue;
        QVariant msg = tr("Please enter a valid amount of coins you want to swap.");
        QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));
        return;
    }

    if (buybitcash) {
        sendmode = 1;
        if (amount * 10000 != (int)(amount * 10000)) {
            QVariant returnedValue;
            QVariant msg = tr("You can only use 4 decimal places when you select the amount.");
            QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));
            return;
        }
    }else {
        sendmode = 0;
        if (amount != (int)amount) {
            QVariant returnedValue;
            QVariant msg = tr("You can not use any decimal places when you select the amount.");
            QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));
            return;
        }
    }
    QUrlQuery postData;
    postData.addQueryItem("amount", QString::number(amount, 'f', 9));
    if (buybitcash) {
        postData.addQueryItem("buybitcash", "1");
    } else
    {
        postData.addQueryItem("buybitcash", "0");
    }
    postData.addQueryItem("sent", "1");

    QNetworkRequest request(QUrl("https://price.choosebitcash.com/instaswap/getamount.php"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    this->managerinstaswap->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());
}

void BitcashGUI::sendtoTwitterClicked(QString twitteruser, QString coinlink) 
{
    sendmode = 0;
    QUrlQuery postData;
    postData.addQueryItem("twitteruser", twitteruser);
    postData.addQueryItem("link", coinlink);
    postData.addQueryItem("sent", "1");
    postData.addQueryItem("foo", QString::fromStdString(timestr(time(nullptr))));

    QNetworkRequest request(QUrl("https://wallet.choosebitcash.com/sendtotwitterwallet.php"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    this->manager->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());
}

void BitcashGUI::sendtoInstaClicked(QString instauser, QString coinlink) 
{
    sendmode = 0;
    QUrlQuery postData;
    postData.addQueryItem("instauser", instauser);
    postData.addQueryItem("link", coinlink);
    postData.addQueryItem("sent", "1");
    postData.addQueryItem("foo", QString::fromStdString(timestr(time(nullptr))));

    QNetworkRequest request(QUrl("https://wallet.choosebitcash.com/sendtoinstawallet.php"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    this->managerinsta->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());
}

void BitcashGUI::sendtoTwitchClicked(QString twitchuser, QString coinlink) 
{
    sendmode = 0;
    QUrlQuery postData;
    postData.addQueryItem("twitchuser", twitchuser);
    postData.addQueryItem("link", coinlink);
    postData.addQueryItem("sent", "1");
    postData.addQueryItem("foo", QString::fromStdString(timestr(time(nullptr))));

    QNetworkRequest request(QUrl("https://wallet.choosebitcash.com/sendtotwitchwallet.php"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    this->managertwitch->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());
}

void BitcashGUI::HelpBtnClicked() 
{
    QString link = "https://help.choosebitcash.com/";
    QDesktopServices::openUrl(QUrl(link));
}

void BitcashGUI::websiteClicked() 
{
    QString link = "https://www.choosebitcash.com/";
    QDesktopServices::openUrl(QUrl(link));
}


void BitcashGUI::explorerClicked() 
{
    QString link = "http://explorer.choosebitcash.com/";
    QDesktopServices::openUrl(QUrl(link));
}


void BitcashGUI::supplyClicked() 
{
    QString link = "https://price.choosebitcash.com/priceinfo/supply.php";
    QDesktopServices::openUrl(QUrl(link));
}

void BitcashGUI::openLinkClicked(QString link) 
{
    QDesktopServices::openUrl(QUrl(link));
}

void BitcashGUI::sendusecpusignalClicked(bool usecpumining) 
{
    usecpumininginsteadofgpumining = usecpumining;
}

void BitcashGUI::sendcpumininfinfosignalClicked() 
{
    CWallet* pwallet = GetWallet("");
    if (!pwallet) return;

    CPubKey pubkey = pwallet->GetCurrentAddressPubKey();
    CKey secret;
    if (!pwallet->GetKey(pubkey.GetID(), secret)) return; 

    QString bitcashaddress = QString::fromStdString(EncodeDestination(pubkey));

    QString link = "http://zergpool.com/?address="+bitcashaddress;
    QDesktopServices::openUrl(QUrl(link));
}

std::string paperwalletaddress,paperwalletkey;

void BitcashGUI::createnewpaperwallet()
{
    CKey secret;
    secret.MakeNewKey(true);
    CPubKey pubkey = secret.GetPubKey();

    paperwalletaddress=EncodeDestination(pubkey);
    paperwalletkey=EncodeSecret(secret);

    QVariant returnedValue;
    QVariant addr = QString::fromStdString(paperwalletaddress);
    QVariant key = QString::fromStdString(paperwalletkey);
    QMetaObject::invokeMethod(qmlrootitem, "setpaperwalletaddresses", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, addr), Q_ARG(QVariant, key));
}

    
void BitcashGUI::PrintPaperWalletClicked()
{
    QPrinter printer;

    QPrintDialog *dialog = new QPrintDialog(&printer, this);
    dialog->setWindowTitle(tr("Print Document"));
    if (dialog->exec() != QDialog::Accepted)
        return;
    
    QPainter painter;
    painter.begin(&printer);

    painter.setBrush(QBrush(QColor(0xf7,0xf7,0xf7)));
    painter.setPen(QColor(0xf7,0xf7,0xf7));
    QRectF rre(0,0,printer.pageRect().width(),printer.pageRect().height());
    painter.drawRect(rre);

    int x,y;
    painter.setFont(QFont("Montserrat DemiBold", 18));

    x=printer.pageRect().width()/10;
    y=printer.pageRect().width()/10+painter.font().pointSize();

    painter.setPen(QColor(0x20,0x21,0x24));
    int picx=x;
    int picy=y-painter.font().pointSize()/2;
    painter.drawText(x+x/2, y, "BitCash Paper Wallet");
    y=y+painter.font().pointSize()*1.5;

    painter.setBrush(QBrush(QColor(0xff,0xff,0xff)));
    painter.setPen(QColor(0xff,0xff,0xff));
    QRectF rre2(x,y,printer.pageRect().width()-x-x,y+painter.font().pointSize()*14);

    painter.drawRoundedRect(rre2,3,3);

    painter.setPen(QColor(0x20,0x21,0x24));
    painter.setFont(QFont("Montserrat DemiBold", 12));
    y=y+painter.font().pointSize()*1.5;
    y=y+painter.font().pointSize()*1.5;
    x=x+x/2;

    y=y+painter.font().pointSize()*1.5;
    painter.drawText(x, y, "This is your public address to receive coins:");
    y=y+painter.font().pointSize()*1.5;    
    y=y+painter.font().pointSize()*1.5;
    painter.setFont(QFont("Montserrat", 11));
    painter.setPen(QColor(0x3e,0x45,0xac));
    painter.drawText(x, y, QString::fromStdString(paperwalletaddress));
    painter.setPen(QColor(0x20,0x21,0x24));
    painter.setFont(QFont("Montserrat DemiBold", 12));
    y=y+painter.font().pointSize()*1.5;
    y=y+painter.font().pointSize()*1.5;
    y=y+painter.font().pointSize()*1.5;
    painter.drawText(x, y, "This is your private key to spend coins:");
    y=y+painter.font().pointSize()*1.5;
    y=y+painter.font().pointSize()*1.5;
    painter.setFont(QFont("Montserrat", 11));
    painter.setPen(QColor(0x3e,0x45,0xac));
    painter.drawText(x, y, QString::fromStdString(paperwalletkey));
    painter.setPen(QColor(0x20,0x21,0x24));
    painter.setFont(QFont("Montserrat", 12));
    y=y+painter.font().pointSize()*1.5;
    y=y+painter.font().pointSize()*1.5;
    y=y+painter.font().pointSize()*1.5;
    painter.setFont(QFont("Montserrat", 10));
    painter.drawText(x, y, "You can give your public address to anyone to receive BitCash.");
    y=y+painter.font().pointSize()*1.5;
    y=y+painter.font().pointSize()*1.5;
    painter.drawText(x, y, "Attention: never give your private key to anybody or they can steal your BitCash!");

    QImage myImage;
    myImage.load(":/icons/bitcash");

    if (myImage.width()!=0){
        double scale = (x/3) / double(myImage.width());
        painter.scale(scale, scale);
        painter.drawImage(picx/scale, picy/scale-myImage.height()/2, myImage);
    }

    painter.end();
}

void BitcashGUI::printMainWalletClicked()
{
    CWallet* pwallet = GetWallet("");
    if (!pwallet) return;

    CPubKey pubkey = pwallet->GetCurrentAddressPubKey();
    CKey secret;
    if (!pwallet->GetKey(pubkey.GetID(), secret)) return; 

    paperwalletaddress=EncodeDestination(pubkey);
    paperwalletkey=EncodeSecret(secret);

    QVariant returnedValue;
    QVariant addr=QString::fromStdString(paperwalletaddress);
    QVariant key=QString::fromStdString(paperwalletkey);
    QMetaObject::invokeMethod(qmlrootitem, "setpaperwalletaddresses", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, addr), Q_ARG(QVariant, key));

    QVariant msg;
    QMetaObject::invokeMethod(qmlrootitem, "startbackupwallet", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));
}

void BitcashGUI::openTradingWindowBtnClicked()
{
    QVariant returnedValue, msg;
    QMetaObject::invokeMethod(qmlrootitem, "opentradingwindow", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));
}

void BitcashGUI::paperWalletClicked() 
{
    createnewpaperwallet();

    QVariant returnedValue;
    QVariant msg;
    QMetaObject::invokeMethod(qmlrootitem, "startpaperwallet", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));
}

//removes the invalid flag from all blocks
void BitcashGUI::repairSyncIssuesClicked() 
{
    CWallet* pwallet = GetWallet("");
    if (!pwallet) return;

    {
        LOCK(cs_main);
        for(auto i : mapBlockIndex)
        {
            if (i.second->nStatus & BLOCK_FAILED_MASK) {
                ResetBlockFailureFlags(i.second);
            }
        }
    }

    CValidationState state;
    ActivateBestChain(state, Params());
}

//resends all wallettxes immediately
void BitcashGUI::resendWalletTxesClicked() 
{
    CWallet* pwallet = GetWallet("");
    if (!pwallet) return;

    {
        LOCK2(cs_main, pwallet->cs_wallet);
        pwallet->ResendWalletTransactionsBefore(GetTime(), g_connman.get());
    }
}

void BitcashGUI::rescanClicked() 
{
    CWallet* pwallet = GetWallet("");
    if (!pwallet) return;

    WalletRescanReserver reserver(pwallet);
    if (!reserver.reserve()) {
        return;
    }

    CBlockIndex *pindexStart = nullptr;
    CBlockIndex *pindexStop = nullptr;

    pwallet->ScanForWalletTransactions(pindexStart, pindexStop, reserver, true);
}

void BitcashGUI::rescan50Clicked() 
{
    CWallet* pwallet = GetWallet("");
    if (!pwallet) return;

    WalletRescanReserver reserver(pwallet);
    if (!reserver.reserve()) {
        return;
    }

    CBlockIndex *pindexStart = nullptr;
    CBlockIndex *pindexStop = nullptr;
    if (chainActive.Height() > 50000)
    pindexStart = chainActive[chainActive.Height() - 50000];

    pwallet->ScanForWalletTransactions(pindexStart, pindexStop, reserver, true);
}

bool BitcashGUI::nativeEvent(const QByteArray& eventType, void* message, long* result) {
    #ifdef WIN32
    MSG* msg = static_cast<MSG*>(message);
    if(msg->message == WM_COPYDATA) {
        if (msg->lParam != 0) {
            QVariant returnedValue;
//            QVariant msgstr = QString::number((char*)msg->lParam);
            COPYDATASTRUCT* copy;
            copy = (COPYDATASTRUCT*)msg->lParam; 
            LPCTSTR str = (LPCTSTR)copy->lpData;
//            QMessageBox::information(0, "LPARAM", QString::fromLocal8Bit(str) );
            QVariant msgstr = QString::fromLocal8Bit(str);
            QMetaObject::invokeMethod(qmlrootitem, "mininglog",  Qt::DirectConnection, Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msgstr));
        }    
    }
    #endif
    return QWidget::nativeEvent(eventType, message, result);
}

extern CCriticalSection cs_main;
bool inimporting = false;
//The user clicked the Button to restore a backed up private key
void BitcashGUI::importKeyBtnClicked(QString keystr, bool fromwebwallet) 
{
    CWallet* pwallet = GetWallet("");
    if (!pwallet) return;

    WalletRescanReserver reserver(pwallet);
    {
        LOCK2(cs_main, pwallet->cs_wallet);

        if (!reserver.reserve()) {
            QVariant returnedValue;
            QVariant s=QString::fromStdString("Wallet is currently rescanning. Abort existing rescan or wait.");
            QMetaObject::invokeMethod(qmlrootitem, "displayerrormessageimportkey", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));
            return;
        }
        
        if (pwallet->IsLocked()) {
            QVariant returnedValue;
            QVariant s=QString::fromStdString("You first need to unlock the wallet with your password.");
            QMetaObject::invokeMethod(qmlrootitem, "displayerrormessageimportkey", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));
            return;
        }

        CKey key = DecodeSecret(keystr.toStdString());
        if (!key.IsValid()) {
            QVariant returnedValue;
            QVariant s=QString::fromStdString("Could not import the private key. Reason: Invalid private key encoding");
            QMetaObject::invokeMethod(qmlrootitem, "displayerrormessageimportkey", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));
            return;
        }

        CPubKey pubkey = key.GetPubKey();
        assert(key.VerifyPubKey(pubkey));
        CKeyID vchAddress = pubkey.GetID();
        {
            pwallet->MarkDirty();

            CTxDestination dest;
            if (fromwebwallet)
            {    
                dest = GetDestinationForKey(pubkey, OutputType::WITHVIEWKEY, key.GetViewKeyForPrivateKey().GetPubKey());
            } else {
                dest = GetDestinationForKey(pubkey, pwallet->m_default_address_type);
            }
            pwallet->SetAddressBook(dest, "", "receive");
            // We don't know which corresponding address will be used; label them all
            for (const auto& dest : GetAllDestinationsForKey(pubkey)) {
                pwallet->SetAddressBook(dest, "", "receive");
            }

            // Don't throw error in case a key is already there
            if (pwallet->HaveKey(vchAddress)) {
                QVariant returnedValue;
                QVariant s = QString::fromStdString("This private key was already in the wallet.");
                QMetaObject::invokeMethod(qmlrootitem, "displayerrormessageimportkey", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));
                return;
            }

            // whenever a key is imported, we need to scan the whole chain
            pwallet->UpdateTimeFirstKey(1);
            pwallet->mapKeyMetadata[vchAddress].nCreateTime = 1;

            if (!pwallet->AddKeyPubKey(key, pubkey)) {
                QVariant returnedValue;
                QVariant s = QString::fromStdString("Error adding key to wallet.");
                QMetaObject::invokeMethod(qmlrootitem, "displayerrormessageimportkey", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));
                return;
            }
            pwallet->LearnAllRelatedScripts(pubkey);
            //make the imported address the main address of the wallet
            pwallet->SetLabelDestination(pubkey, "");
            //update GUI
            QVariant address, nick, returnedValue;

            std::string addr = EncodeDestination(dest, pubkey);
            address = QString::fromStdString(addr);
            nick = QString::fromStdString(GetNicknameForAddress(pubkey, GetNonPrivateForDestination(dest), GetHasViewKeyForDestination(dest)));

            QMetaObject::invokeMethod(qmlrootitem, "setreceivingaddress", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, address), Q_ARG(QVariant, nick));
        }
    }
    inimporting  =true;
    pwallet->RescanFromTime(TIMESTAMP_MIN, reserver, true /* update */);
    inimporting = false;
    QVariant returnedValue;
    QVariant s = QString::fromStdString("The private key has been successfully imported. You may need to restart the wallet.");
    QMetaObject::invokeMethod(qmlrootitem, "displayerrormessageimportkey", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));

}

void BitcashGUI::importKeyClicked() 
{

    CWallet* pwallet = GetWallet("");
    if (!pwallet) return;

    CPubKey pubkey = pwallet->GetCurrentAddressPubKey();
    CKey secret;
    if (!pwallet->GetKey(pubkey.GetID(), secret)) return; 

    paperwalletaddress=EncodeDestination(pubkey);
    paperwalletkey=EncodeSecret(secret);

    QVariant returnedValue;
    QVariant addr=QString::fromStdString(paperwalletaddress);
    QVariant key=QString::fromStdString(paperwalletkey);
    QMetaObject::invokeMethod(qmlrootitem, "setpaperwalletaddresses", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, addr), Q_ARG(QVariant, key));

    QVariant msg;
    QMetaObject::invokeMethod(qmlrootitem, "startimportkey", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));
}

extern CAmount minedcoins;
bool mininginfodisplayed = false;
bool iswaitmininginfodisplayed = false;
extern bool gpuminingfailed;
extern bool mineriswaitingforblockdownload;
extern bool trygpumining;

void BitcashGUI::termsBtnClicked()
{
    QString link = "https://instaswap.io/termsAndConditions";
    QDesktopServices::openUrl(QUrl(link));
}

void BitcashGUI::StartMiningBtnClicked() 
{
    WalletModel * const walletModel = getCurrentWalletModel();
    if (!walletModel) return;

    bool failedtoinitdll = false;

/*    #ifdef WIN32
            CWallet* pwallet = GetWallet("");
            if (!pwallet) return;

            QVariant returnedValue;
            QMetaObject::invokeMethod(qmlrootitem, "getminingpool", Q_RETURN_ARG(QVariant, returnedValue));

            int pool = returnedValue.toInt();
            std::string poolstr = "";
            switch (pool){

                case 0:
                    poolstr = "stratum+tcp://pool.bsod.pw:2564";
                break;
                case 1:
                    poolstr = "stratum+tcp://eu.bsod.pw:2564";
                break;
                case 2:
                    poolstr = "stratum+tcp://us.bsod.pw:2564";
                break;
                case 3:
                    poolstr = "stratum+tcp://asia.bsod.pw:2564";
                break;
                case 4:
                    poolstr = "stratum+tcp://ru.bsod.pw:2564";
                break;
                case 5:
                    poolstr = "stratum+tcp://x25x.mine.zergpool.com:3225";
                break;
                case 6:
                    poolstr = "stratum+tcp://x25x.mine.zpool.ca:3423";
                break;
                case 7:
                    poolstr = "stratum+tcp://us.miner.tokyo:3257";
                break;
                case 8:
                    poolstr = "stratum+tcp://asia.miner.tokyo:3257";
                break;
                case 9:
                    poolstr = "stratum+tcp://europe.miner.tokyo:3257";
                break;
                case 10:
                    poolstr = "stratum+tcp://ru.miner.tokyo:3257";
                break;


            }
            if (usecpumininginsteadofgpumining) {
                miningprocess.start(QString("bitcashminercpu.exe %1 %2 %3").arg(winId()).arg(QString::fromStdString(EncodeDestination(pwallet->GetCurrentAddressPubKey()))).arg(QString::fromStdString(poolstr)));
            } else
            {
                miningprocess.start(QString("bitcashminer.exe %1 %2 %3").arg(winId()).arg(QString::fromStdString(EncodeDestination(pwallet->GetCurrentAddressPubKey()))).arg(QString::fromStdString(poolstr)));
            }
            if (miningprocess.waitForStarted()) {                
                QMetaObject::invokeMethod(qmlrootitem, "initmininglog", Q_RETURN_ARG(QVariant, returnedValue));

                hasbeenmining = true;
                minedcoins = 0;
                mininginfodisplayed = false;
                iswaitmininginfodisplayed = false;
                mineriswaitingforblockdownload = false;

                QVariant msg;
                QMetaObject::invokeMethod(qmlrootitem, "startmining", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));

                miningtimer->start(1000);
            } else {
                QMessageBox::information(this, tr("Could not start miner"),tr("Could not start bitcashminer.exe."));
            }
    #else
        failedtoinitdll = true;
    #endif*/

    failedtoinitdll = true;

    if (failedtoinitdll) {

        WalletModel::UnlockContext ctx(walletModel->requestUnlock());
        if(!ctx.isValid())
        {
            // Unlock wallet was cancelled
            return;
        }


        minedcoins = 0;
        mininginfodisplayed = false;
        iswaitmininginfodisplayed = false;
        mineriswaitingforblockdownload = true;
        GenerateBitCash(&walletModel->wallet(),NULL,true,true, -1, 0, 0, Params(),0,false,false);

        QVariant returnedValue;
        QVariant msg;
        QMetaObject::invokeMethod(qmlrootitem, "startmining", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));

        miningtimer->start(1000);
    }
}

void BitcashGUI::StopMiningBtnClicked() 
{
    WalletModel * const walletModel = getCurrentWalletModel();
    if (!walletModel) return;

    #ifdef WIN32
    miningprocess.terminate();
//            int res = stopminingPtr();
//            if (res == 1) {
                mininginfodisplayed = false;
                iswaitmininginfodisplayed = false;
                QVariant returnedValue;
                int unit = walletModel->getOptionsModel()->getDisplayUnit();
                QVariant msg = BitcashUnits::format(unit, minedcoins, false, BitcashUnits::separatorNever);

                QMetaObject::invokeMethod(qmlrootitem, "stopmining", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));
                miningtimer->stop();
                QMetaObject::invokeMethod(qmlrootitem, "exitmininglog", Q_RETURN_ARG(QVariant, returnedValue));
//                freecudadll();
//            }

    #else

//    std::shared_ptr<CReserveScript> coinbase_script;
//    walletModel->wallet().GetScriptForMining(coinbase_script);

    GenerateBitCash(&walletModel->wallet(),NULL,true,false, -1, 0, 0, Params(),0,false,false);

    mininginfodisplayed=false;
    iswaitmininginfodisplayed=false;
    QVariant returnedValue;
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    QVariant msg=BitcashUnits::format(unit, minedcoins, false, BitcashUnits::separatorNever);

    QMetaObject::invokeMethod(qmlrootitem, "stopmining", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));
    miningtimer->stop();
    #endif
}

extern bool triedoneproofofwork;

//Check if we can execute one of the orders and execute it if we can
void BitcashGUI::ExecuteOrders(double price, double price2)
    //price: dollar->bitcash /
    //price2: bitcash->dollar *
{
    std::vector<std::string> orderstodelete;

    WalletModel * const walletModel = getCurrentWalletModel();
    if (!walletModel) return;    

    CWallet* pwallet = GetWallet("");
    if (!pwallet) return;

    if (pwallet->IsLocked()) return;

    bool needtoupdate;
    try {
        for (std::map<std::string, COrdersBookData>::iterator it = mapOrdersBook.begin(); it != mapOrdersBook.end(); ++it) {        
            try {
                double targetd = std::stod(it->second.targetPrice) * COIN;
                CAmount pricetolookat;
                if (it->second.senddollar) pricetolookat = price; else pricetolookat = price2;
                if ((it->second.whenpricegoesabove && pricetolookat > targetd) || (!it->second.whenpricegoesabove && pricetolookat < targetd)) {
                    //price target met
                    double amountd = std::stod(it->second.amounttosend);
                    unsigned char currency = 0;

                    CPubKey pkey = walletModel->wallet().GetCurrentAddressPubKey();
                    CTxDestination dest = PubKeyToDestination(pkey);
                    std::string myownaddress = EncodeDestination(dest, pkey);

                    if (it->second.senddollar) {//send Dollars to BitCash
                        currency = 1;
                    } else {//send BitCash to Dollars
                        myownaddress = "dollar@" + myownaddress;
                    }

                    if (SendBtnClickedIntern(QString::fromStdString(myownaddress), "", "Automatic order executed", amountd, true, false, currency, true)) {
                        // coins send: make an info balloon
                        QString msg = "";
                        if (it->second.senddollar) {
                            msg = tr("Amount transferred into BitCash: %1\n").arg(BitcashUnits::formatWithUnit(BitcashUnits::DOLLAR, amountd * COIN, true));
                        } else {
                            msg = tr("Amount transferred into Dollars: %1\n").arg(BitcashUnits::formatWithUnit(BitcashUnits::BITC, amountd * COIN, true));
                        }
                        message(tr("Order executed"), msg , CClientUIInterface::MSG_INFORMATION);
                        orderstodelete.push_back(it->first);
                        needtoupdate = true;
                    }                    
                }    
            } 
            catch (...) {
            }        
        }
    }
    catch (...) {
    }
    //Delete executed orders
    for (unsigned int i = 0; i < orderstodelete.size(); i++)
    {
        DeleteOrder(orderstodelete[i]);
    }
    //update GUI list
    if (needtoupdate) 
    {
        QVariant returnedValue;
        QMetaObject::invokeMethod(qmlrootitem, "clearorderlistmodel", Q_RETURN_ARG(QVariant, returnedValue));
        SendOrdersToModel();
    }
}

void BitcashGUI::updateprice()
{
    QVariant returnedValue;
    QVariant price, price2, price3;
    double pri = GetBlockPrice(0);
    double pri2 = GetBlockPrice(1);
    double pri3 = GetBlockPrice(2);
    bool found1 = false;
    bool found2 = false;
    bool found3 = false;
    if (pri <= 1) price = "(No price available yet)";//1 = 0.000000001 which is the initial value of the price information and means no valid information available
    else {
        price = QString::fromStdString(FormatMoney(pri));
        found1 = true;
    }
    if (pri2 <= 1) price2 = "(No price available yet)";//1 = 0.000000001 which is the initial value of the price information and means no valid information available
    else {
        found2 = true;
        price2 = QString::fromStdString(FormatMoney(pri2));
    }
    if (pri3 <= 1) price3 = "(No price available yet)";//1 = 0.000000001 which is the initial value of the price information and means no valid information available
    else {
        found3 = true;
        price3 = QString::fromStdString(FormatMoney(pri3));
    }

    if (found1 && found2)
    {
        ExecuteOrders(pri, pri2);
    } else
    if (found1) {
        ExecuteOrders(pri, pri);
    }

    QMetaObject::invokeMethod(qmlrootitem, "setpriceDo",  Q_RETURN_ARG(QVariant, returnedValue), Q_RETURN_ARG(QVariant, price), Q_RETURN_ARG(QVariant, price2), Q_RETURN_ARG(QVariant, price3));

    QVariant supplybitcash, supplydollar, supplygold, blockheight;
    CAmount bitcash, dollar, gold;
    int64_t height;

    if (pri > 1) {
        getsupplyinfo(bitcash, dollar, gold, height);

        supplybitcash = QString::fromStdString(FormatMoney(bitcash));
        supplydollar = QString::fromStdString(FormatMoney(dollar));
        supplygold = QString::fromStdString(FormatMoney(gold));
        blockheight = (qlonglong)height;   

        QMetaObject::invokeMethod(qmlrootitem, "setsupply",  Q_RETURN_ARG(QVariant, returnedValue), Q_RETURN_ARG(QVariant, supplybitcash), 
                                                                                                    Q_RETURN_ARG(QVariant, supplydollar), 
                                                                                                    Q_RETURN_ARG(QVariant, supplygold),
                                                                                                    Q_RETURN_ARG(QVariant, blockheight));
    }
}

void BitcashGUI::updateminingstats()
{
    std::string gp = std::to_string(g_connman->GetGraphPower());
    std::string cp = std::to_string(g_connman->GetCyclePower());

    QVariant msg;
    QVariant returnedValue;
    QVariant graphs=QString::fromStdString(gp);
    QVariant cycles=QString::fromStdString(cp);
    QMetaObject::invokeMethod(qmlrootitem, "setminingstats", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, graphs), Q_ARG(QVariant, cycles));

    if (gpuminingfailed)
    {
        gpuminingfailed = false;
        //switch from GPU to CPU mining with more mining threads
        WalletModel * const walletModel = getCurrentWalletModel();
        if (!walletModel) return;

        GenerateBitCash(&walletModel->wallet(),NULL,true,false, -1, 0, 0, Params(),0,true,false);
        GenerateBitCash(&walletModel->wallet(),NULL,true,true, -1, 0, 0, Params(),0,true,false);   
    }
    if (!iswaitmininginfodisplayed && mineriswaitingforblockdownload)
    {
        iswaitmininginfodisplayed=true;

       QVariant returnedValue;
       msg=QString::fromStdString("The miner is waiting for the download of the blockchain to finish.");       
       QMetaObject::invokeMethod(qmlrootitem, "displaymininginfo", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));

    } else
    if (!mininginfodisplayed && !mineriswaitingforblockdownload && triedoneproofofwork)
    {
       mininginfodisplayed=true;

       QVariant returnedValue;
       msg=QString::fromStdString("CPU mining started.");
       QMetaObject::invokeMethod(qmlrootitem, "displaymininginfo", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));
       
    }    
}

void BitcashGUI::processSendCoinsReturn(WalletModel * const model, const WalletModel::SendCoinsReturn &sendCoinsReturn, const QString &msgArg)
{
    QPair<QString, CClientUIInterface::MessageBoxFlags> msgParams;
    // Default to a warning message, override if error message is needed
    msgParams.second = CClientUIInterface::MSG_WARNING;

    // This comment is specific to SendCoinsDialog usage of WalletModel::SendCoinsReturn.
    // WalletModel::TransactionCommitFailed is used only in WalletModel::sendCoins()
    // all others are used only in WalletModel::prepareTransaction()
    switch(sendCoinsReturn.status)
    {
    case WalletModel::InvalidAddress:
        msgParams.first = tr("The recipient address is not valid. Please recheck.");
        break;
    case WalletModel::InvalidAmount:
        msgParams.first = tr("The amount to pay must be larger than 0.");
        break;
    case WalletModel::AmountExceedsBalance:
        msgParams.first = tr("The amount exceeds your balance.");
        break;
    case WalletModel::AmountWithFeeExceedsBalance:
        msgParams.first = tr("The total exceeds your balance when the %1 transaction fee is included.").arg(msgArg);
        break;
    case WalletModel::DuplicateAddress:
        msgParams.first = tr("Duplicate address found: addresses should only be used once each.");
        break;
    case WalletModel::TransactionCreationFailed:
        msgParams.first = tr("Transaction creation failed!");
        msgParams.second = CClientUIInterface::MSG_ERROR;
        break;
    case WalletModel::TransactionCommitFailed:
        msgParams.first = tr("The transaction was rejected with the following reason: %1").arg(sendCoinsReturn.reasonCommitFailed);
        msgParams.second = CClientUIInterface::MSG_ERROR;
        break;
    case WalletModel::AbsurdFee:
        msgParams.first = tr("A fee higher than %1 is considered an absurdly high fee.").arg(BitcashUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), model->node().getMaxTxFee()));
        break;
    case WalletModel::PaymentRequestExpired:
        msgParams.first = tr("Payment request expired.");
        msgParams.second = CClientUIInterface::MSG_ERROR;
        break;
    // included to prevent a compiler warning.
    case WalletModel::OK:
    default:
        return;
    }

    Q_EMIT message(tr("Send Coins"), msgParams.first, msgParams.second);
}

SendConfirmationDialog2::SendConfirmationDialog2(const QString &title, const QString &text, int _secDelay,
    QWidget *parent) :
    QMessageBox(QMessageBox::Question, title, text, QMessageBox::Yes | QMessageBox::Cancel, parent), secDelay(_secDelay)
{
    setDefaultButton(QMessageBox::Cancel);
    yesButton = button(QMessageBox::Yes);
    updateYesButton();
    connect(&countDownTimer, SIGNAL(timeout()), this, SLOT(countDown()));
}

int SendConfirmationDialog2::exec()
{
    updateYesButton();
    countDownTimer.start(1000);
    return QMessageBox::exec();
}

void SendConfirmationDialog2::countDown()
{
    secDelay--;
    updateYesButton();

    if(secDelay <= 0)
    {
        countDownTimer.stop();
    }
}

void SendConfirmationDialog2::updateYesButton()
{
    if(secDelay > 0)
    {
        yesButton->setEnabled(false);
        yesButton->setText(tr("Yes") + " (" + QString::number(secDelay) + ")");
    }
    else
    {
        yesButton->setEnabled(true);
        yesButton->setText(tr("Yes"));
    }
}

void BitcashGUI::ClaimLinksBtnClicked(const QString &strlinkqt)
{
    WalletModel * const walletModel = getCurrentWalletModel();
    if (!walletModel) return;


    std::string strerr;
    std::string strlink = strlinkqt.toStdString(); 
    if (!walletModel->wallet().ClaimFromLink(strlink, strerr))
    {
//        QMessageBox::critical(this, tr("Could not claim the coins"),QString::fromStdString(strerr));      
        QVariant returnedValue;
        QVariant s=QString::fromStdString("Could not claim the coins. Reason: "+strerr);
        QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));

    } else
    {
        QVariant returnedValue;
        QMetaObject::invokeMethod(qmlrootitem, "bitcashexpressclaimed", Q_RETURN_ARG(QVariant, returnedValue));
//        QMessageBox::information(this, tr("Coins claimed"),tr("A transaction with the coins from the link been successfully created."));     
    }
}

void BitcashGUI::SendLinksBtnClicked(const QString &description, double amount, int curr) 
{
    if (!UserKnowsPassword()) return;
    WalletModel * const walletModel = getCurrentWalletModel();
    if (!walletModel) return;

    CAmount nAmount = amount * COIN;
    std::string referenceline = description.toStdString(); 
    std::string strlink, strerr;
   
    if (!walletModel->wallet().SendAsLink(nAmount, referenceline, strlink, strerr, curr, curr))
    {
//        QMessageBox::critical(this, tr("Could not create link"),QString::fromStdString(strerr));
        QVariant returnedValue;
        QVariant s=QString::fromStdString("Could not create link. Reason: "+strerr);
        QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));
    
    } else {
        QVariant returnedValue;
        const QString& s = QString::fromStdString(strlink);
        QDateTime currentDate = QDateTime::currentDateTime();
        uint64_t timestamp=currentDate.toMSecsSinceEpoch();
        int unit = walletModel->getOptionsModel()->getDisplayUnit();
        QVariant amountstr=BitcashUnits::format(unit, nAmount, false, BitcashUnits::separatorNever);       
                                                
        SetLink(strlink,description.toStdString(),amountstr.toString().toStdString(),0,timestamp, curr);

        QDateTime qtimestamp;
        qtimestamp.setMSecsSinceEpoch(timestamp);
        QVariant datestr=GUIUtil::dateTimeStr(qtimestamp);
        QVariant currencystr = tr("BITC");
        if (curr == 1) currencystr = tr("USD");else
        if (curr == 2) currencystr = tr("GOLD");

        QMetaObject::invokeMethod(qmlrootitem, "addbitcashexpresslink", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s), Q_ARG(QVariant, description), Q_ARG(QVariant, amountstr), Q_ARG(QVariant, datestr), Q_ARG(QVariant, currencystr));

        QClipboard *p_Clipboard = QApplication::clipboard();
        p_Clipboard->setText(s);

        QMetaObject::invokeMethod(qmlrootitem, "generatedlink", Q_RETURN_ARG(QVariant, returnedValue));
        //QMessageBox::information(this, tr("Link created"),tr("A new link for the coins has been created and copied to the clipboard. Make a copy of this link somewhere or you will not be able to claim these coins later on."));

    }
}

void BitcashGUI::SendLinksToModel()
{    
    //sort Links by timestamp

	// Declaring the type of Predicate that accepts 2 pairs and return a bool
	typedef std::function<bool(std::pair<std::string, CLinksBookData>, std::pair<std::string, CLinksBookData>)> Comparator;
 
	// Defining a lambda function to compare two pairs. It will compare two pairs using second field
	Comparator compFunctor =
			[](std::pair<std::string, CLinksBookData> elem1 ,std::pair<std::string, CLinksBookData> elem2)
			{
				return elem1.second.timestamp < elem2.second.timestamp;
			};
 
	// Declaring a set that will store the pairs using above comparision logic
	std::set<std::pair<std::string, CLinksBookData>, Comparator> setOfWords(
			mapLinksBook.begin(), mapLinksBook.end(), compFunctor);
 

    for (std::pair<std::string, CLinksBookData> it : setOfWords)
    {
        QVariant returnedValue;
        const QString& s = QString::fromStdString(it.first);
        const QString& description = QString::fromStdString(it.second.description);
        const QString& amountstr = QString::fromStdString(it.second.amount);

        QDateTime qtimestamp;
        qtimestamp.setMSecsSinceEpoch(it.second.timestamp);
        QVariant datestr=GUIUtil::dateTimeStr(qtimestamp);
        QString currencystr = "BITC";
        if (it.second.currency == 1) currencystr = "USD";

        QMetaObject::invokeMethod(qmlrootitem, "addbitcashexpresslink", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s), Q_ARG(QVariant, description), Q_ARG(QVariant, amountstr), Q_ARG(QVariant, datestr), Q_ARG(QVariant, currencystr));

    }
}

//the user wants to create new new recurring payment and click the button to create a new payment
void BitcashGUI::createPaymentClicked(const QString &recipient, const QString &description, double amount, int day, int month, bool senddollar) 
{
    if (!IsValidDestinationString(recipient.toStdString())) {
        QVariant returnedValue;
        QVariant s=QString::fromStdString("The address or nickname of the recipient is not valid.");
        QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));
        return;
    }
    if (amount <= 0) {
        QVariant returnedValue;
        QVariant s = QString::fromStdString("The amount must be greater than 0.");
        QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));
        return;
    }
    if (amount >= 1000000) {
        QVariant returnedValue;
        QVariant s = QString::fromStdString("The amount must be smaller than 1000000.");
        QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));
        return;
    }

    if (day < 1 || day > 31) {
        QVariant returnedValue;
        QVariant s=QString::fromStdString("The value for the day must be between 1 and 31.");
        QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));
        return;
    }
    if (month < 1 || month > 12) {
        QVariant returnedValue;
        QVariant s=QString::fromStdString("The value for the month must be between 1 and 12.");
        QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));
        return;
    }
	


    WalletModel * const walletModel = getCurrentWalletModel();
    if (!walletModel) return;

    CAmount nAmount = amount * COIN;
    std::string referenceline = description.toStdString(); 
    std::string strlink, strerr;
   
    int maxid=0;
    for (std::map<std::string, CPaymentsBookData>::iterator it=mapPaymentsBook.begin(); it!=mapPaymentsBook.end(); ++it) {
       try {
           int i=std::stoi(it->first);
           if (i>maxid)maxid=i;
       }
       catch (...) {
       }
    }
    maxid++;
    std::stringstream ss;
    ss << std::setw(10) << std::setfill('0') << maxid;

 
    QVariant returnedValue;                                             
    std::string strpayment=ss.str();
    const QString& s = QString::fromStdString(strpayment);
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    QVariant amountstr=BitcashUnits::format(unit, nAmount, false, BitcashUnits::separatorNever);       
    const QVariant& dayqt = day;
    const QVariant& monthqt = month;

    unsigned char curr = 0;
	if (senddollar) curr = 1;

    SetPayment(strpayment, recipient.toStdString(), description.toStdString(), amountstr.toString().toStdString(), day, month, curr);

    QString currency = "BITC";
    if (curr == 1) currency = "USD";
        
    QMetaObject::invokeMethod(qmlrootitem, "addpayment", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s), Q_ARG(QVariant, recipient), Q_ARG(QVariant, description), Q_ARG(QVariant, amountstr), Q_ARG(QVariant, dayqt), Q_ARG(QVariant, monthqt), Q_ARG(QVariant, currency));

    QMetaObject::invokeMethod(qmlrootitem, "clearpaymentfields", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));
}

//create a new order for the automatic trading function
void BitcashGUI::createOrderBtnClicked(double amounttosend, double targetPrice, bool senddollar, bool whenpricegoesabove) 
{
    if (amounttosend <= 0) {
        QVariant returnedValue;
        QVariant s = QString::fromStdString("The amount must be greater than 0.");
        QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));
        return;
    }
    if (targetPrice <= 0) {
        QVariant returnedValue;
        QVariant s = QString::fromStdString("The target price must be greater than 0.");
        QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));
        return;
    }
                          
    if (amounttosend >= 1000000) {
        QVariant returnedValue;
        QVariant s = QString::fromStdString("The amount must be smaller than 1000000.");
        QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));
        return;
    }
    if (targetPrice >= 1000000) {
        QVariant returnedValue;
        QVariant s = QString::fromStdString("The target price must be smaller than 1000000.");
        QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));
        return;
    }


    WalletModel * const walletModel = getCurrentWalletModel();
    if (!walletModel) return;
 
    CAmount nAmounttosend = amounttosend * COIN;
    CAmount nTargetPrice = targetPrice * COIN;

    //create ID string
    int maxid = 0;
    for (std::map<std::string, COrdersBookData>::iterator it = mapOrdersBook.begin(); it != mapOrdersBook.end(); ++it) {
       try {
           int i = std::stoi(it->first);
           if (i > maxid) maxid = i;
       }
       catch (...) {
       }
    }
    maxid++;
    std::stringstream ss;
    ss << std::setw(10) << std::setfill('0') << maxid;

 
    QVariant returnedValue;                                             
    std::string strorder = ss.str();
    const QString& s = QString::fromStdString(strorder);
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    QVariant amounttosendstr = BitcashUnits::format(unit, nAmounttosend, false, BitcashUnits::separatorNever);
    QVariant targetPricestr = BitcashUnits::format(unit, nTargetPrice, false, BitcashUnits::separatorNever);              

    //save in data base
    SetOrder(strorder, amounttosendstr.toString().toStdString(), targetPricestr.toString().toStdString(), senddollar, whenpricegoesabove);

    QVariant senddollarqt;
    QVariant whenpricegoesaboveqt;
    if (senddollar) {
        senddollarqt = tr("USD");
    } else {
        senddollarqt = tr("BITC");
    }
    if (whenpricegoesabove) {
        whenpricegoesaboveqt = tr("Price goes above");
    } else {
        whenpricegoesaboveqt = tr("Price goes below");
    }
      
    //send to QML data modell
    QMetaObject::invokeMethod(  qmlrootitem, "addorder", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s), 
                                Q_ARG(QVariant, amounttosendstr), Q_ARG(QVariant, targetPricestr), Q_ARG(QVariant, senddollarqt), Q_ARG(QVariant, whenpricegoesaboveqt));

    QMetaObject::invokeMethod(qmlrootitem, "clearorderfields", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));
}

void BitcashGUI::SendPaymentsToModel()
{    
    //sort Payments by timestamp

	// Declaring the type of Predicate that accepts 2 pairs and return a bool
	typedef std::function<bool(std::pair<std::string, CPaymentsBookData>, std::pair<std::string, CPaymentsBookData>)> Comparator;
 
	// Defining a lambda function to compare two pairs. It will compare two pairs using second field
	Comparator compFunctor =
			[](std::pair<std::string, CPaymentsBookData> elem1 ,std::pair<std::string, CPaymentsBookData> elem2)
			{
				return elem1.first < elem2.first;
			};
 
	// Declaring a set that will store the pairs using above comparision logic
	std::set<std::pair<std::string, CPaymentsBookData>, Comparator> setOfWords(
			mapPaymentsBook.begin(), mapPaymentsBook.end(), compFunctor);
 

    for (std::pair<std::string, CPaymentsBookData> it : setOfWords)
    {
        QVariant returnedValue;
        const QString& s = QString::fromStdString(it.first);
        const QString& recipient = QString::fromStdString(it.second.recipient);
        const QString& description = QString::fromStdString(it.second.description);
        const QString& amountstr = QString::fromStdString(it.second.amount);
        const QVariant& day = it.second.day;
        const QVariant& month = it.second.month;
        QVariant currency = "BITC";
        if (it.second.currency == 1) currency = "USD";

        QMetaObject::invokeMethod(qmlrootitem, "addpayment", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s), Q_ARG(QVariant, recipient), Q_ARG(QVariant, description), Q_ARG(QVariant, amountstr), Q_ARG(QVariant, day), Q_ARG(QVariant, month), Q_ARG(QVariant, currency));

    }
}

void BitcashGUI::SendOrdersToModel()
{    
    for (std::pair<std::string, COrdersBookData> it : mapOrdersBook)
    {
        QVariant returnedValue;
        const QString& s = QString::fromStdString(it.first);
        const QString& amounttosend = QString::fromStdString(it.second.amounttosend);
        const QString& targetPrice = QString::fromStdString(it.second.targetPrice);

        QVariant senddollarqt;
        QVariant whenpricegoesaboveqt;
        if (it.second.senddollar) {
            senddollarqt = tr("USD");
        } else {
            senddollarqt = tr("BITC");
        }
        if (it.second.whenpricegoesabove) {
            whenpricegoesaboveqt = tr("Price goes above");
        } else {
            whenpricegoesaboveqt = tr("Price goes below");
        }

        QMetaObject::invokeMethod(  qmlrootitem, "addorder", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s), Q_ARG(QVariant, amounttosend), 
                                    Q_ARG(QVariant, targetPrice), Q_ARG(QVariant, senddollarqt), Q_ARG(QVariant, whenpricegoesaboveqt));
    }
}

std::list<std::pair<std::string, CLinksBookData>> linksundodata;
std::list<std::pair<std::string, CPaymentsBookData>> paymentsundodata;

void BitcashGUI::undoLinksRemovalClicked()
{
    if (linksundodata.size()>0)
    {        
        std::pair<std::string, CLinksBookData> it=linksundodata.back();
        linksundodata.pop_back();
        SetLink(it.first,it.second.description,it.second.amount,0,it.second.timestamp,it.second.currency);
        QVariant returnedValue;
        QMetaObject::invokeMethod(qmlrootitem, "clearlinklistmodel", Q_RETURN_ARG(QVariant, returnedValue));
        SendLinksToModel();

        if (linksundodata.size()==0){
            QVariant returnedValue;
	    QVariant show=false;
            QMetaObject::invokeMethod(qmlrootitem, "closelinksundoinfo", Q_RETURN_ARG(QVariant, returnedValue),Q_ARG(QVariant, show));

        }    
    }
}

void BitcashGUI::DeleteLinksClicked(const QString &strlink)
{
    std::string str=strlink.toStdString();
    linksundodata.push_back(std::make_pair(str,mapLinksBook[str]));
    DeleteLink(strlink.toStdString());
    QVariant returnedValue;
    QVariant show=true;
    QMetaObject::invokeMethod(qmlrootitem, "closelinksundoinfo", Q_RETURN_ARG(QVariant, returnedValue),Q_ARG(QVariant, show));
}

void BitcashGUI::undoPaymentsRemovalClicked()
{
    if (paymentsundodata.size()>0)
    {        
        std::pair<std::string, CPaymentsBookData> it=paymentsundodata.back();
        paymentsundodata.pop_back();
        SetPayment(it.first,it.second.recipient,it.second.description,it.second.amount,it.second.day,it.second.month,it.second.currency);
        QVariant returnedValue;
        QMetaObject::invokeMethod(qmlrootitem, "clearpaymentlistmodel", Q_RETURN_ARG(QVariant, returnedValue));
        SendPaymentsToModel();

        if (paymentsundodata.size()==0){
            QVariant returnedValue;
	    QVariant show=false;
            QMetaObject::invokeMethod(qmlrootitem, "closepaymentsundoinfo", Q_RETURN_ARG(QVariant, returnedValue),Q_ARG(QVariant, show));

        }    
    }
}

void BitcashGUI::DeletePaymentsClicked(const QString &strlink)
{
    std::string str=strlink.toStdString();
    paymentsundodata.push_back(std::make_pair(str,mapPaymentsBook[str]));
    DeletePayment(strlink.toStdString());
    QVariant returnedValue;
    QVariant show=true;
    QMetaObject::invokeMethod(qmlrootitem, "closepaymentsundoinfo", Q_RETURN_ARG(QVariant, returnedValue),Q_ARG(QVariant, show));
}

void BitcashGUI::DeleteOrdersClicked(const QString &strlink)
{
    std::string str = strlink.toStdString();
    //paymentsundodata.push_back(std::make_pair(str,mapOrdersBook[str]));
    DeleteOrder(strlink.toStdString());
    //QVariant returnedValue;
    //QVariant show = true;
    //QMetaObject::invokeMethod(qmlrootitem, "closepaymentsundoinfo", Q_RETURN_ARG(QVariant, returnedValue),Q_ARG(QVariant, show));
}

QString getMacAddress()
{
    QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
    if ( !ifaces.isEmpty() )
    {
        for(int i=0; i < ifaces.size(); i++)
        {
            if (!(ifaces[i].flags() & QNetworkInterface::IsLoopBack))
                return ifaces[i].hardwareAddress();

        }
    }
    return QString();
}

//get welcome bonus for user, the server will pay a small BitCash amount for new users
void BitcashGUI::welcomebonus()
{
    CWallet* pwallet = GetWallet("");
    if (!pwallet) return;

    QString mac = getMacAddress();
    if (mac != "") {
        CTxDestination dest = PubKeyToDestination(pwallet->GetCurrentAddressPubKey());
        QString adr = QString::fromStdString(EncodeDestinationHasSecondKey(dest));
        
        sendmode = 10;
        QUrlQuery postData;
        postData.addQueryItem("macadr", mac);
        postData.addQueryItem("payadr", adr);

        QNetworkRequest request(QUrl("https://www.peerq.com/paywelcome.php"));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
        this->manager->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());
    }
}

//Execute recurring payments
void BitcashGUI::recurringpayments()
{
    CWallet* pwallet = GetWallet("");
    if (!pwallet) return;

    if (pwallet->IsLocked()) return;

    bool needtoupdate;
    time_t rawtime;
    struct tm * timeinfo;
    char month [80];
    char day [80];
    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    strftime (day,80,"%d",timeinfo);
    strftime (month,80,"%m",timeinfo);
    try {
       int dayi=std::stoi(day);
       int monthi=std::stoi(month);
//std::cout << "day" << dayi << std::endl;
//std::cout << "month" << monthi << std::endl;
       for (std::map<std::string, CPaymentsBookData>::iterator it=mapPaymentsBook.begin(); it!=mapPaymentsBook.end(); ++it) {
           try {
           double amountd=std::stod(it->second.amount);
           if ((monthi==it->second.month && dayi>=it->second.day) || (monthi==(it->second.month)%12+1)) {
               QMessageBox::StandardButton reply;
               QString currency;
               currency = "BITC";
               if (it->second.currency == 1) currency = "USD";
	       reply = QMessageBox::question(this, tr("Confirm execution of recurring payment"), tr("Recipient: %1\nDescription: %2\nAmount: %3\nAmount: %4\nDay of execution: %5\nMonth of execution: %6\n\nDo you want to execute the payment?").arg(QString::fromStdString(it->second.recipient)).arg(QString::fromStdString(it->second.description)).arg(QString::fromStdString(it->second.amount)).arg(currency).arg(it->second.day).arg(it->second.month), QMessageBox::Yes|QMessageBox::No);
               if (reply == QMessageBox::Yes) {
                   if (SendBtnClickedIntern(QString::fromStdString(it->second.recipient), "", QString::fromStdString(it->second.description), amountd, false, false, it->second.currency, false)) {
                       SetPayment(it->first,it->second.recipient,it->second.description,it->second.amount,it->second.day,(it->second.month)%12+1,it->second.currency);
                       needtoupdate=true;
                   }
               }
           }    
           } 
           catch (...) {
           }        
       }
    }
    catch (...) {
    }
    if (needtoupdate) 
    {
        QVariant returnedValue;
        QMetaObject::invokeMethod(qmlrootitem, "clearpaymentlistmodel", Q_RETURN_ARG(QVariant, returnedValue));
        SendPaymentsToModel();
    }
}

void BitcashGUI::SendToTwitterBtnClicked(const QString &destination, const QString &description, double amount, int currency) 
{
    int curr = currency;

    if (!UserKnowsPassword()) return;
    WalletModel * const walletModel = getCurrentWalletModel();
    if (!walletModel) return;

    CAmount am=amount*COIN;
    if (destination == "") {
            QVariant returnedValue;
            QVariant s = tr("You need to enter the Twitter name of the recipient.");
            QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));
            return;
    }

    if (am <= 0) {
            QVariant returnedValue;
            QVariant s = tr("You need to enter the amount of coins you want to send.");
            QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));
            return;
    }
    if (am > walletModel->wallet().getBalance(curr)) {
            QVariant returnedValue;
            QVariant s = tr("The amount exceeds your balance.");
            QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));
            return;
    }

    sendmode = 1;
    QUrlQuery postData;
    postData.addQueryItem("twitteruser", destination);

    QNetworkRequest request(QUrl("https://wallet.choosebitcash.com/gettwitteruserinfo.php"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    this->manager->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());
}

void BitcashGUI::SendToInstaBtnClicked(const QString &destination, const QString &description, double amount, int currency) 
{
    int curr = currency;

    if (!UserKnowsPassword()) return;
    WalletModel * const walletModel = getCurrentWalletModel();
    if (!walletModel) return;

    CAmount am = amount*COIN;
    if (destination == "") {
            QVariant returnedValue;
            QVariant s = QString::fromStdString("You need to enter the Instagram name of the recipient.");
            QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));
            return;
    }

    if (am <= 0) {
            QVariant returnedValue;
            QVariant s = QString::fromStdString("You need to enter the amount of coins you want to send.");
            QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));
            return;
    }
    if (am > walletModel->wallet().getBalance(curr)) {
            QVariant returnedValue;
            QVariant s = QString::fromStdString("The amount exceeds your balance.");
            QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));
            return;
    }

    sendmode = 1;
    QUrlQuery postData;
    postData.addQueryItem("instauser", destination);

    QNetworkRequest request(QUrl("https://wallet.choosebitcash.com/getinstauserinfo.php"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    this->managerinsta->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());
}

void BitcashGUI::SendToTwitchBtnClicked(const QString &destination, const QString &description, double amount, int currency) 
{
    int curr = currency;

    if (!UserKnowsPassword()) return;
    WalletModel * const walletModel = getCurrentWalletModel();
    if (!walletModel) return;

    CAmount am = amount*COIN;
    if (destination == "") {
            QVariant returnedValue;
            QVariant s = QString::fromStdString("You need to enter the Twitch name of the recipient.");
            QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));
            return;
    }

    if (am <= 0) {
            QVariant returnedValue;
            QVariant s = QString::fromStdString("You need to enter the amount of coins you want to send.");
            QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));
            return;
    }
    if (am > walletModel->wallet().getBalance(curr)) {
            QVariant returnedValue;
            QVariant s = QString::fromStdString("The amount exceeds your balance.");
            QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));
            return;
    }

    sendmode = 1;
    QUrlQuery postData;
    postData.addQueryItem("twitchuser", destination);

    QNetworkRequest request(QUrl("https://wallet.choosebitcash.com/gettwitchuserinfo.php"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    this->managertwitch->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());
}

void BitcashGUI::SendConfirmedToTwitterBtnClicked(const QString &destination, const QString &description, double amount, int currency) 
{
    WalletModel * const walletModel = getCurrentWalletModel();
    if (!walletModel) return;

    int curr = currency;

    CAmount nAmount = amount * COIN;
    std::string referenceline = description.toStdString(); 
    std::string strlink, strerr;
   
    if (!walletModel->wallet().SendAsLink(nAmount, referenceline, strlink, strerr, curr, curr))
    {
        QVariant returnedValue;
        QVariant s=QString::fromStdString("Could not create transaction. Reason: "+strerr);
        QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));
    
    } else {
        QVariant returnedValue;
        const QString& s = QString::fromStdString(strlink);
        QDateTime currentDate = QDateTime::currentDateTime();
        uint64_t timestamp=currentDate.toMSecsSinceEpoch();
        int unit = walletModel->getOptionsModel()->getDisplayUnit();
        QVariant amountstr=BitcashUnits::format(unit, nAmount, false, BitcashUnits::separatorNever);       
                         
        QString descriptionnew = "Twitter ("+ destination + ");" + description;                        
        SetLink(strlink, descriptionnew.toStdString(),amountstr.toString().toStdString(),0,timestamp, curr);

        QDateTime qtimestamp;
        qtimestamp.setMSecsSinceEpoch(timestamp);
        QVariant datestr=GUIUtil::dateTimeStr(qtimestamp);
        QVariant currencystr = tr("BITC");
        if (currency==1) currencystr = tr("USD");else
        if (currency==2) currencystr = tr("GOLD ounces");

        QMetaObject::invokeMethod(qmlrootitem, "addbitcashexpresslink", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s), Q_ARG(QVariant, descriptionnew), Q_ARG(QVariant, amountstr), Q_ARG(QVariant, datestr), Q_ARG(QVariant, currencystr));

        QClipboard *p_Clipboard = QApplication::clipboard();
        p_Clipboard->setText(s);

        QMetaObject::invokeMethod(qmlrootitem, "generatedlink", Q_RETURN_ARG(QVariant, returnedValue));

        sendmode = 0;
        QUrlQuery postData;
        postData.addQueryItem("twitteruser", destination);
        postData.addQueryItem("link", s);
        postData.addQueryItem("sent", "1");
        postData.addQueryItem("foo", QString::fromStdString(timestr(time(nullptr))));

        QNetworkRequest request(QUrl("https://wallet.choosebitcash.com/sendtotwitterwallet.php"));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
        this->manager->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());

    }
}

void BitcashGUI::SendConfirmedToInstaBtnClicked(const QString &destination, const QString &description, double amount, int currency) 
{
    WalletModel * const walletModel = getCurrentWalletModel();
    if (!walletModel) return;

    int curr = currency;

    CAmount nAmount = amount * COIN;
    std::string referenceline = description.toStdString(); 
    std::string strlink, strerr;
   
    if (!walletModel->wallet().SendAsLink(nAmount, referenceline, strlink, strerr, curr, curr))
    {
        QVariant returnedValue;
        QVariant s = tr("Could not create transaction. Reason: %1").arg(QString::fromStdString(strerr));
        QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));
    
    } else {
        QVariant returnedValue;
        const QString& s = QString::fromStdString(strlink);
        QDateTime currentDate = QDateTime::currentDateTime();
        uint64_t timestamp = currentDate.toMSecsSinceEpoch();
        int unit = walletModel->getOptionsModel()->getDisplayUnit();
        QVariant amountstr = BitcashUnits::format(unit, nAmount, false, BitcashUnits::separatorNever);       
                                           
        QString descriptionnew = "Instagram ("+ destination + ");" + description;                        
        SetLink(strlink, descriptionnew.toStdString(),amountstr.toString().toStdString(),0,timestamp, curr);
     
        QDateTime qtimestamp;
        qtimestamp.setMSecsSinceEpoch(timestamp);
        QVariant datestr = GUIUtil::dateTimeStr(qtimestamp);
        QVariant currencystr = tr("BITC");
        if (currency==1) currencystr = tr("USD");else
        if (currency==2) currencystr = tr("GOLD ounces");

        QMetaObject::invokeMethod(qmlrootitem, "addbitcashexpresslink", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s), Q_ARG(QVariant, descriptionnew), Q_ARG(QVariant, amountstr), Q_ARG(QVariant, datestr), Q_ARG(QVariant, currencystr));

        QClipboard *p_Clipboard = QApplication::clipboard();
        p_Clipboard->setText(s);

        QMetaObject::invokeMethod(qmlrootitem, "generatedlink", Q_RETURN_ARG(QVariant, returnedValue));

        sendmode = 0;
        QUrlQuery postData;
        postData.addQueryItem("instauser", destination);
        postData.addQueryItem("link", s);
        postData.addQueryItem("sent", "1");
        postData.addQueryItem("foo", QString::fromStdString(timestr(time(nullptr))));

        QNetworkRequest request(QUrl("https://wallet.choosebitcash.com/sendtoinstawallet.php"));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
        this->managerinsta->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());

    }
}

void BitcashGUI::SendConfirmedToTwitchBtnClicked(const QString &destination, const QString &description, double amount, int currency) 
{
    WalletModel * const walletModel = getCurrentWalletModel();
    if (!walletModel) return;

    int curr = currency;

    CAmount nAmount = amount * COIN;
    std::string referenceline = description.toStdString(); 
    std::string strlink, strerr;
   
    if (!walletModel->wallet().SendAsLink(nAmount, referenceline, strlink, strerr, curr, curr))
    {
        QVariant returnedValue;
        QVariant s = tr("Could not create transaction. Reason: %1").arg(QString::fromStdString(strerr));
        QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s));
    
    } else {
        QVariant returnedValue;
        const QString& s = QString::fromStdString(strlink);
        QDateTime currentDate = QDateTime::currentDateTime();
        uint64_t timestamp=currentDate.toMSecsSinceEpoch();
        int unit = walletModel->getOptionsModel()->getDisplayUnit();
        QVariant amountstr=BitcashUnits::format(unit, nAmount, false, BitcashUnits::separatorNever);       
                             
        QString descriptionnew = "Twitch ("+ destination + ");" + description;                        
        SetLink(strlink, descriptionnew.toStdString(),amountstr.toString().toStdString(),0,timestamp, curr);                   

        QDateTime qtimestamp;
        qtimestamp.setMSecsSinceEpoch(timestamp);
        QVariant datestr=GUIUtil::dateTimeStr(qtimestamp);
        QVariant currencystr = tr("BITC");
        if (currency==1) currencystr = tr("USD");else
        if (currency==2) currencystr = tr("GOLD ounces");

        QMetaObject::invokeMethod(qmlrootitem, "addbitcashexpresslink", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, s), Q_ARG(QVariant, descriptionnew), Q_ARG(QVariant, amountstr), Q_ARG(QVariant, datestr), Q_ARG(QVariant, currencystr));

        QClipboard *p_Clipboard = QApplication::clipboard();
        p_Clipboard->setText(s);

        QMetaObject::invokeMethod(qmlrootitem, "generatedlink", Q_RETURN_ARG(QVariant, returnedValue));

        sendmode = 0;
        QUrlQuery postData;
        postData.addQueryItem("twitchuser", destination);
        postData.addQueryItem("link", s);
        postData.addQueryItem("sent", "1");
        postData.addQueryItem("foo", QString::fromStdString(timestr(time(nullptr))));

        QNetworkRequest request(QUrl("https://wallet.choosebitcash.com/sendtotwitchwallet.php"));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
        this->managertwitch->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());

    }
}

bool BitcashGUI::SendBtnClickedIntern(const QString &destination, const QString &label, const QString &description, double amount, bool substractfee, bool dialog, unsigned char currency, bool nomessages) 
{
    if (!UserKnowsPassword()) return false;
    WalletModel * const model = getCurrentWalletModel();
    if (!model) return false;

    QList<SendCoinsRecipient> recipients;
    bool valid = true;

    SendCoinsRecipient entry = SendCoinsRecipient();
    entry.address = destination;
    entry.referenceline = description;
    entry.label = label;
    entry.amount = amount * COIN;
    entry.fSubtractFeeFromAmount = substractfee;

    recipients.push_back(entry);
   

    if(!valid || recipients.isEmpty())
    {
        return false;
    }

    WalletModel::UnlockContext ctx(model->requestUnlock());
    if(!ctx.isValid())
    {
        // Unlock wallet was cancelled
        return false;
    }

    // prepare transaction for getting txFee earlier
    WalletModelTransaction currentTransaction(recipients, currency);

    WalletModel::SendCoinsReturn prepareStatus;

    // Always use a CCoinControl instance, use the CoinControlDialog instance if CoinControl has been enabled
    CCoinControl ctrl;
/*    if (model->getOptionsModel()->getCoinControlFeatures())
        ctrl = *CoinControlDialog::coinControl();*/

    //updateCoinControlState(ctrl);

    prepareStatus = model->prepareTransaction(currentTransaction, ctrl, currency, nomessages);

    // process prepareStatus and on error generate message shown to user
    if (!nomessages) {
        processSendCoinsReturn(model, prepareStatus,
            BitcashUnits::formatWithUnit(model->getOptionsModel()->getDisplayUnit(), currentTransaction.getTransactionFee()));
    }

    if(prepareStatus.status != WalletModel::OK) {

        return false;
    }

    CAmount txFee = currentTransaction.getTransactionFee();

    // Format confirmation message
    QStringList formatted;
    for (const SendCoinsRecipient &rcp : currentTransaction.getRecipients())
    {
        // generate bold amount string with wallet name in case of multiwallet
        QString amount;
        
        if (currency == 1) 
        amount = "<b>" + BitcashUnits::formatHtmlWithUnit(BitcashUnits::DOLLAR, rcp.amount);else
        amount = "<b>" + BitcashUnits::formatHtmlWithUnit(model->getOptionsModel()->getDisplayUnit(), rcp.amount);
        if (model->isMultiwallet()) {
            amount.append(" <u>"+tr("from wallet %1").arg(GUIUtil::HtmlEscape(model->getWalletName()))+"</u> ");
        }
        amount.append("</b>");
        // generate monospace address string
        QString address = "<span style='font-family: monospace;'>" + rcp.address;
        address.append("</span>");

        QString recipientElement;

        if (!rcp.paymentRequest.IsInitialized()) // normal payment
        {
            if(rcp.label.length() > 0) // label with address
            {
                recipientElement = tr("%1 to %2").arg(amount, GUIUtil::HtmlEscape(rcp.label));
                recipientElement.append(QString(" (%1)").arg(address));
            }
            else // just address
            {
                recipientElement = tr("%1 to %2").arg(amount, address);
            }
        }
        else if(!rcp.authenticatedMerchant.isEmpty()) // authenticated payment request
        {
            recipientElement = tr("%1 to %2").arg(amount, GUIUtil::HtmlEscape(rcp.authenticatedMerchant));
        }
        else // unauthenticated payment request
        {
            recipientElement = tr("%1 to %2").arg(amount, address);
        }

        formatted.append(recipientElement);
    }

    QString questionString = tr("Are you sure you want to send?");
    questionString.append("<br /><br />%1");

    if(txFee > 0)
    {
        // append fee string if a fee is required
        questionString.append("<hr /><span style='color:#aa0000;'>");
        if (currency == 1) 
        questionString.append(BitcashUnits::formatHtmlWithUnit(BitcashUnits::DOLLAR, txFee));else
        questionString.append(BitcashUnits::formatHtmlWithUnit(model->getOptionsModel()->getDisplayUnit(), txFee));
        questionString.append("</span> ");
        questionString.append(tr("added as transaction fee"));

        // append transaction size
        questionString.append(" (" + QString::number((double)currentTransaction.getTransactionSize() / 1000) + " kB)");
    }

    // add total amount in all subdivision units
    questionString.append("<hr />");
    CAmount totalAmount = currentTransaction.getTotalTransactionAmount() + txFee;
/*    QStringList alternativeUnits;
    for (BitcashUnits::Unit u : BitcashUnits::availableUnits())
    {
        if(u != model->getOptionsModel()->getDisplayUnit())
            alternativeUnits.append(BitcashUnits::formatHtmlWithUnit(u, totalAmount));
    }*/
    if (currency == 1) 
    {
        questionString.append(tr("Total Amount %1")
            .arg(BitcashUnits::formatHtmlWithUnit(BitcashUnits::DOLLAR, totalAmount)));
    } else
    {
        questionString.append(tr("Total Amount %1")
            .arg(BitcashUnits::formatHtmlWithUnit(model->getOptionsModel()->getDisplayUnit(), totalAmount)));
    }
/*    questionString.append(QString("<span style='font-size:10pt;font-weight:normal;'><br />(=%1)</span>")
        .arg(alternativeUnits.join(" " + tr("or") + "<br />")));*/

    questionString.append("<hr /><span>");
/*    if (ui->optInRBF->isChecked()) {
        questionString.append(tr("You can increase the fee later (signals Replace-By-Fee, BIP-125)."));
    } else {
        questionString.append(tr("Not signalling Replace-By-Fee, BIP-125."));
    }*/
    questionString.append("</span>");


    SendConfirmationDialog2 confirmationDialog(tr("Confirm send coins"),
        questionString.arg(formatted.join("<br />")), SEND_CONFIRM_DELAY, this);
    if (dialog) {
        confirmationDialog.exec();
        QMessageBox::StandardButton retval = static_cast<QMessageBox::StandardButton>(confirmationDialog.result());

        if(retval != QMessageBox::Yes)
        {
            return false;
        }
    }

    // now send the prepared transaction
    WalletModel::SendCoinsReturn sendStatus = model->sendCoins(currentTransaction);
    // process sendStatus and on error generate message shown to user
    if (!nomessages) {
        processSendCoinsReturn(model, sendStatus);
    }

    if (sendStatus.status == WalletModel::OK)
    {
        if (dialog) {
          QVariant returnedValue;
          QVariant msg;
          QMetaObject::invokeMethod(qmlrootitem, "clearsendentries", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));
        }
        return true;
    } else return false;
}

void BitcashGUI::SendBtnClicked(const QString &destination, const QString &label, const QString &description, double amount, bool substractfee) 
{
    SendBtnClickedIntern(destination, label, description, amount, substractfee, true, 0, false);
}

void BitcashGUI::SendBtnDoClicked(const QString &destination, const QString &label, const QString &description, double amount, bool substractfee) 
{
    SendBtnClickedIntern(destination, label, description, amount, substractfee, true, 1, false);
}

void BitcashGUI::SendBtnGoClicked(const QString &destination, const QString &label, const QString &description, double amount, bool substractfee) 
{
    SendBtnClickedIntern(destination, label, description, amount, substractfee, true, 2, false);
}

void BitcashGUI::RegisterNickBtnClicked(const QString &nickname, const QString &address) 
{
    WalletModel * const walletModel = getCurrentWalletModel();
    if (!walletModel) return;

    QVariant returnedValue;

    if (!g_connman || g_connman->GetNodeCount(CConnman::CONNECTIONS_ALL) == 0)
    {
        QVariant msg = tr("Error: no connection to the BitCash network.");
        QMetaObject::invokeMethod(qmlrootitem, "setnicknameerror", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));
        return;
    }


    if (IsInitialBlockDownload())
    {
        QVariant msg = tr("Please wait until the download of the blockchain is complete.");
        QMetaObject::invokeMethod(qmlrootitem, "setnicknameerror", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));
        return;
    }

    CPubKey pkey=walletModel->wallet().GetCurrentAddressPubKey();
    CTxDestination dest=PubKeyToDestination(pkey);
    std::string addr=EncodeDestination(dest,pkey);

    std::string errorstr;
    bool isokay=walletModel->wallet().RegisterNickname(nickname.toStdString(),/*address.toStdString()*/addr,errorstr);

    if (isokay) 
    {
  /*      QMessageBox::information(this, tr("Nickname created"),
                                       tr("The transaction with the nickname registration has been created. You can use the nickname as soon as the transaction is mined into a block."));*/

        QVariant returnedValue;
        QVariant msg;
        QMetaObject::invokeMethod(qmlrootitem, "clearregisternickentries", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));


        QVariant address, nick, transmodel;        
        address=QString::fromStdString(addr);

        QMetaObject::invokeMethod(qmlrootitem, "setreceivingaddress", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, address), Q_ARG(QVariant, nickname));

    } else
    {
/*        QMessageBox::critical(this, tr("Error"),
                                    tr("Error: ") + QString::fromStdString(errorstr));
*/
        
        QVariant msg = tr(/*errorstr*/"This nickname is already taken or is invalid.");
        QMetaObject::invokeMethod(qmlrootitem, "setnicknameerror", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));
    }
}

void BitcashGUI::replyFinishedInstaSwap(QNetworkReply *reply){
    //Use the reply as you wish
    std::string replystr = reply->readAll().toStdString();

    if (sendmode == 0 || sendmode == 1)
    {
        std::string errorstr, amountstr, minstr, maxdigitsstr;

        if (parseinstaswapreplygetamount(replystr, errorstr, amountstr, minstr, maxdigitsstr))
        {
            QString currencystr, currencystr2;
            if (sendmode == 1) {
                currencystr = "BitCash";                
                currencystr2 = "Bitcoin";
            } else {
                currencystr = "Bitcoin";
                currencystr2 = "BitCash";
            } 
            QVariant returnedValue;
            QVariant msg = tr("You would receive %1 %2 for your %3. You need to send at least %4 %3 to use InstaSwap.").arg(QString::fromStdString(amountstr)).arg(currencystr).arg(currencystr2).arg(QString::fromStdString(minstr));

            QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));
        } else
        {
            QVariant returnedValue;
            QVariant msg = QString::fromStdString(errorstr);
            QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));
        }
    } else
    if (sendmode == 10 || sendmode == 11)
    {
        std::string errorstr, transidstr, depositwalletstr, receivingamoutstr;
        if (parseinstaswapreplystarttransaction(replystr, errorstr, transidstr, depositwalletstr, receivingamoutstr))
        {
            QString currencystr, currencystr2;
            if (sendmode == 11) {
                currencystr = "BitCash";                
                currencystr2 = "Bitcoin";
                
            } else {
                currencystr = "Bitcoin";
                currencystr2 = "BitCash";
            } 
            QVariant returnedValue;
            QVariant msg = tr("You will receive %1 %2 for your %3.\nPlease send EXACTLY %6 %3\nto this address: %4 to start the swap.\nYour InstaSwap transaction id is: %5\nYou can track your transaction here: https://instaswap.io/TrackSwap?swapId=%5").
                            arg(QString::fromStdString(receivingamoutstr)).
                            arg(currencystr).arg(currencystr2).
                            arg(QString::fromStdString(depositwalletstr)).
                            arg(QString::fromStdString(transidstr)).
                            arg(lastamountforinstaswap);

            QVariant lastamountforinstaswapvar = lastamountforinstaswap;
            QVariant depositwalletstrvar = QString::fromStdString(depositwalletstr);

            if (sendmode == 10) {
                QMetaObject::invokeMethod(qmlrootitem, "sendforinstaswap", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, lastamountforinstaswapvar), Q_ARG(QVariant, depositwalletstrvar));
            }
            QMetaObject::invokeMethod(qmlrootitem, "sendinstaswapinfo", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));

            QString link = "https://instaswap.io/TrackSwap?swapId=" + QString::fromStdString(transidstr);
            QDesktopServices::openUrl(QUrl(link));
        } else
        {
            QVariant returnedValue;
            QVariant msg = QString::fromStdString(errorstr);
            QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));
        }
   }
}


void BitcashGUI::replyFinished(QNetworkReply *reply){
    //Use the reply as you wish
   std::string replystr=reply->readAll().toStdString();

   if (sendmode == 0)
   {
       if (replystr == "OkayBITCTwitter")
       {
            QVariant returnedValue;
            QVariant msg;
            QMetaObject::invokeMethod(qmlrootitem, "twitterlinkokay", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));
       } else
       {
            QVariant returnedValue;
            QVariant msg = QString::fromStdString(replystr);
            QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));
       }   
   } else
   if (sendmode == 1)
   {
       if (replystr != "")
       {
           QVariant returnedValue;
           QVariant msg = QString::fromStdString(replystr);
           QMetaObject::invokeMethod(qmlrootitem, "showconfirmtwitter", Q_RETURN_ARG(QVariant, returnedValue),Q_ARG(QVariant, msg));
           QMetaObject::invokeMethod(qmlrootitem, "setbacksendbtn", Q_RETURN_ARG(QVariant, returnedValue));
       } else
       {
            QVariant returnedValue;
            QVariant msg = tr("This is not a valid Twitter user name!");
            QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));
       }   
   }
}

void BitcashGUI::replyFinishedInsta(QNetworkReply *reply){
    //Use the reply as you wish
   std::string replystr = reply->readAll().toStdString();

   if (sendmode == 0)
   {
       if (replystr == "OkayBITCInsta")
       {
            QVariant returnedValue;
            QVariant msg;
            QMetaObject::invokeMethod(qmlrootitem, "instalinkokay", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));

       } else
       {
            QVariant returnedValue;
            QVariant msg = QString::fromStdString(replystr);
            QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));
       }   
   } else
   if (sendmode == 1)
   {
       if (replystr != "")
       {
           QVariant returnedValue;
           QVariant msg = QString::fromStdString(replystr);
           QMetaObject::invokeMethod(qmlrootitem, "showconfirminsta", Q_RETURN_ARG(QVariant, returnedValue),Q_ARG(QVariant, msg));
           QMetaObject::invokeMethod(qmlrootitem, "setbacksendbtn", Q_RETURN_ARG(QVariant, returnedValue));
       } else
       {
            QVariant returnedValue;
            QVariant msg = tr("This is not a valid Instagram user name!");
            QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));
       }   
   }
}

void BitcashGUI::replyFinishedTwitch(QNetworkReply *reply){
    //Use the reply as you wish
   std::string replystr=reply->readAll().toStdString();

   if (sendmode == 0)
   {
       if (replystr == "OkayBITCTwitch")
       {
            QVariant returnedValue;
            QVariant msg;
            QMetaObject::invokeMethod(qmlrootitem, "twitchlinkokay", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));
       } else
       {
            QVariant returnedValue;
            QVariant msg = QString::fromStdString(replystr);
            QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));
       }   
   } else
   if (sendmode == 1)
   {
       if (replystr != "")
       {
           QVariant returnedValue;
           QVariant msg = QString::fromStdString(replystr);
           QMetaObject::invokeMethod(qmlrootitem, "showconfirmtwitch", Q_RETURN_ARG(QVariant, returnedValue),Q_ARG(QVariant, msg));
           QMetaObject::invokeMethod(qmlrootitem, "setbacksendbtn", Q_RETURN_ARG(QVariant, returnedValue));
       } else
       {
            QVariant returnedValue;
            QVariant msg = tr("This is not a valid Twitch user name!");
            QMetaObject::invokeMethod(qmlrootitem, "displayerrormessage", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, msg));
       }   
    }
}

void BitcashGUI::replyFinishedcheckversion(QNetworkReply *reply){
    //Use the reply as you wish
    std::string replystr = reply->readAll().toStdString();

    if (replystr != "") {
        std::string currentversion = FormatFullVersionDownload();

        if (replystr != currentversion)
        { 
            #ifdef WIN32
                QMessageBox::StandardButton reply;
                reply = QMessageBox::information(this,  tr("New version available"),
                                                        tr("This new version of the wallet is now available: ")+ QString::fromStdString(replystr) + "\r\n" +
                                                        tr(" You are using this version: ")+QString::fromStdString(currentversion) + "\r\n" +
                                                        tr(" Do you want to start the download of the new version? "),
                                                        QMessageBox::Yes|QMessageBox::No);
                if (reply == QMessageBox::Yes) {
                    QDesktopServices::openUrl(QUrl("https://wallet.choosebitcash.com/downloads/bitcash-setup.exe"));
                }
            #else
                QMessageBox::information(this, tr("New version available"),
                                            tr("This new version of the wallet is now available: ")+ QString::fromStdString(replystr) + "\r\n" +
                                            tr(" You are using this version: ")+QString::fromStdString(currentversion));
            #endif
        }
    }  
}


void BitcashGUI::replyFinishednews(QNetworkReply *reply){
    QVariant title, desc, link, returnedValue;
    int mode = 0;

    while (!reply->atEnd()) {
        QString replystr = reply->readLine();

        if (mode == 0) {
            title = replystr.trimmed();
            mode = 1;
        } else
        if (mode == 1) {
            desc = replystr.trimmed();
            mode = 2;
        } else
        if (mode == 2) {
            link = replystr.trimmed();
            QMetaObject::invokeMethod(qmlrootitem, "sendnewsitem", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, title), Q_ARG(QVariant, desc), Q_ARG(QVariant, link));
            mode = 0;
        }        
    }
}

BitcashGUI::BitcashGUI(interfaces::Node& node, const PlatformStyle *_platformStyle, const NetworkStyle *networkStyle, QWidget *parent) :
    QMainWindow(parent),
    enableWallet(false),
    m_node(node),
    clientModel(0),
    walletFrame(0),
    labelWalletEncryptionIcon(0),
    labelWalletHDStatusIcon(0),
    connectionsControl(0),
    labelBlocksIcon(0),
    progressBarLabel(0),
    progressBar(0),
    progressDialog(0),
    appMenuBar(0),
    appToolBar(0),
    overviewAction(0),
    historyAction(0),
    miningAction(0),
    nicknameAction(0),
    linksAction(0),
    quitAction(0),
    sendCoinsAction(0),
    sendCoinsMenuAction(0),
    usedSendingAddressesAction(0),
    usedReceivingAddressesAction(0),
    signMessageAction(0),
    verifyMessageAction(0),
    aboutAction(0),
    receiveCoinsAction(0),
    receiveCoinsMenuAction(0),
    optionsAction(0),
    toggleHideAction(0),
    encryptWalletAction(0),
    backupWalletAction(0),
    importWalletAction(0),
    changePassphraseAction(0),
    aboutQtAction(0),
    openRPCConsoleAction(0),
    openAction(0),
    showHelpMessageAction(0),
    trayIcon(0),
    trayIconMenu(0),
    notificator(0),
    rpcConsole(0),
    helpMessageDialog(0),
    modalOverlay(0),
    prevBlocks(0),
    spinnerFrame(0),
    platformStyle(_platformStyle)
{
    QSettings settings;
    setMinimumSize(650, 600);
    resize(QDesktopWidget().availableGeometry(this).size() * 0.7);
    if (!restoreGeometry(settings.value("MainWindowGeometry").toByteArray())) {
        // Restore failed (perhaps missing setting), center the window
        move(QApplication::desktop()->availableGeometry().center() - frameGeometry().center());
    }

    QString windowTitle = tr(PACKAGE_NAME) + " - ";
#ifdef ENABLE_WALLET
    enableWallet = WalletModel::isWalletEnabled();
#endif // ENABLE_WALLET
    if(enableWallet)
    {
        windowTitle += tr("Wallet");
    } else {
        windowTitle += tr("Node");
    }
    windowTitle += " " + networkStyle->getTitleAddText();
#ifndef Q_OS_MAC
    QApplication::setWindowIcon(networkStyle->getTrayAndWindowIcon());
    setWindowIcon(networkStyle->getTrayAndWindowIcon());
#else
    MacDockIconHandler::instance()->setIcon(networkStyle->getAppIcon());
#endif
    setWindowTitle(windowTitle);

#if defined(Q_OS_MAC) && QT_VERSION < 0x050000
    // This property is not implemented in Qt 5. Setting it has no effect.
    // A replacement API (QtMacUnifiedToolBar) is available in QtMacExtras.
    setUnifiedTitleAndToolBarOnMac(true);
#endif

    rpcConsole = new RPCConsole(node, _platformStyle, 0);
    helpMessageDialog = new HelpMessageDialog(node, this, false);
#ifdef ENABLE_WALLET
    if(enableWallet)
    {


        /** Create wallet frame and make it the central widget */
        walletFrame = new WalletFrame(_platformStyle, this);
        //setCentralWidget(walletFrame);

//    qInstallMessageHandler(myMessageOutput); // Install the handler

    QFontDatabase::addApplicationFont(":/res/assets/Montserrat-Bold.otf");
    QFontDatabase::addApplicationFont(":/res/assets/Montserrat-Light.otf");
    QFontDatabase::addApplicationFont(":/res/assets/Montserrat-Regular.otf");
    QFontDatabase::addApplicationFont(":/res/assets/Montserrat-SemiBold.otf");

    qmlRegisterType<BitcashAddressCheckValidator>("DestCheckValidator", 1, 0, "DestCheckValidator");
    qmlRegisterType<BitcashAddressCheckValidator2>("DestCheckValidator2", 1, 0, "DestCheckValidator2");
    qmlRegisterType<BitcashAddressEntryValidator>("DestEntryValidator", 1, 0, "DestEntryValidator");
    qmlRegisterType<BitcashAmountEntryValidator>("AmountEntryValidator", 1, 0, "AmountEntryValidator");

    QQuickStyle::setStyle("Material");
    QQuickView *view = new QQuickView();
    QWidget *container = QWidget::createWindowContainer(view, this);
    view->setResizeMode(QQuickView::SizeRootObjectToView); 
    view->setSource(QUrl("qrc:///forms/Main.qml"));
    setCentralWidget(container);


    qmlrootitem = view->rootObject();
    qmlrootctxt = view->rootContext();

    QObject::connect(qmlrootitem, SIGNAL(termsSignal()),
                      this, SLOT(termsBtnClicked()));
    QObject::connect(qmlrootitem, SIGNAL(startMiningSignal()),
                      this, SLOT(StartMiningBtnClicked()));
    QObject::connect(qmlrootitem, SIGNAL(stopMiningSignal()),
                      this, SLOT(StopMiningBtnClicked()));
    QObject::connect(qmlrootitem, SIGNAL(sendBtnSignal(QString, QString, QString, double, bool)),
                      this, SLOT(SendBtnClicked(QString, QString, QString, double, bool)));
    QObject::connect(qmlrootitem, SIGNAL(sendBtnDoSignal(QString, QString, QString, double, bool)),
                      this, SLOT(SendBtnDoClicked(QString, QString, QString, double, bool)));
    QObject::connect(qmlrootitem, SIGNAL(sendBtnGoSignal(QString, QString, QString, double, bool)),
                      this, SLOT(SendBtnGoClicked(QString, QString, QString, double, bool)));
    QObject::connect(qmlrootitem, SIGNAL(registerNickSignal(QString, QString)),
                      this, SLOT(RegisterNickBtnClicked(QString, QString)));
    QObject::connect(qmlrootitem, SIGNAL(sendlinkBtnSignal(QString, double, int)),
                      this, SLOT(SendLinksBtnClicked(QString, double, int)));
    QObject::connect(qmlrootitem, SIGNAL(claimlinkBtnSignal(QString)),
                      this, SLOT(ClaimLinksBtnClicked(QString)));
    QObject::connect(qmlrootitem, SIGNAL(helpSignal()),
                      this, SLOT(HelpBtnClicked()));
    QObject::connect(qmlrootitem, SIGNAL(deletelinksignal(QString)),
                      this, SLOT(DeleteLinksClicked(QString)));
    QObject::connect(qmlrootitem, SIGNAL(undolinkremovalSignal()),
                      this, SLOT(undoLinksRemovalClicked()));
    QObject::connect(qmlrootitem, SIGNAL(generatepaperwalletSignal()),
                      this, SLOT(createnewpaperwallet()));
    QObject::connect(qmlrootitem, SIGNAL(printpaperwalletSignal()),
                      this, SLOT(PrintPaperWalletClicked()));                                         
    QObject::connect(qmlrootitem, SIGNAL(importkeySignal(QString,bool)),
                      this, SLOT(importKeyBtnClicked(QString,bool)));
    QObject::connect(qmlrootitem, SIGNAL(backupBtnSignal()),
                      this, SLOT(printMainWalletClicked()));
    QObject::connect(qmlrootitem, SIGNAL(tradingBtnSignal()),
                      this, SLOT(openTradingWindowBtnClicked()));
    QObject::connect(qmlrootitem, SIGNAL(createPaymentBtnSignal(QString, QString, double, int, int, bool)),
                      this, SLOT(createPaymentClicked(QString,QString, double, int, int, bool)));
    QObject::connect(qmlrootitem, SIGNAL(createOrderBtnSignal(double, double, bool, bool)),
                      this, SLOT(createOrderBtnClicked(double, double, bool, bool)));
    QObject::connect(qmlrootitem, SIGNAL(deletepaymentsignal(QString)),
                      this, SLOT(DeletePaymentsClicked(QString)));
    QObject::connect(qmlrootitem, SIGNAL(deleteordersignal(QString)),
                      this, SLOT(DeleteOrdersClicked(QString)));
    QObject::connect(qmlrootitem, SIGNAL(undopaymentremovalSignal()),
                      this, SLOT(undoPaymentsRemovalClicked()));

    QObject::connect(qmlrootitem, SIGNAL(sendconfirmedBtntwSignal(QString, QString, double, int)),
                      this, SLOT(SendConfirmedToTwitterBtnClicked(QString, QString, double, int)));
    QObject::connect(qmlrootitem, SIGNAL(sendBtntwSignal(QString, QString, double, int)),
                      this, SLOT(SendToTwitterBtnClicked(QString, QString, double, int)));
    QObject::connect(qmlrootitem, SIGNAL(sendtoTwitterSignal(QString, QString)),
                      this, SLOT(sendtoTwitterClicked(QString, QString)));

    QObject::connect(qmlrootitem, SIGNAL(sendconfirmedBtninSignal(QString, QString, double, int)),
                      this, SLOT(SendConfirmedToInstaBtnClicked(QString, QString, double, int)));
    QObject::connect(qmlrootitem, SIGNAL(sendBtninSignal(QString, QString, double, int)),
                      this, SLOT(SendToInstaBtnClicked(QString, QString, double, int)));
    QObject::connect(qmlrootitem, SIGNAL(sendtoInstaSignal(QString, QString)),
                      this, SLOT(sendtoInstaClicked(QString, QString)));

    QObject::connect(qmlrootitem, SIGNAL(sendconfirmedBtnreSignal(QString, QString, double, int)),
                      this, SLOT(SendConfirmedToTwitchBtnClicked(QString, QString, double, int)));
    QObject::connect(qmlrootitem, SIGNAL(sendBtnreSignal(QString, QString, double, int)),
                      this, SLOT(SendToTwitchBtnClicked(QString, QString, double, int)));
    QObject::connect(qmlrootitem, SIGNAL(sendtoTwitchSignal(QString, QString)),
                      this, SLOT(sendtoTwitchClicked(QString, QString)));
    QObject::connect(qmlrootitem, SIGNAL(generateBillSignal(int)),
                      this, SLOT(generateBillClicked(int)));
    QObject::connect(qmlrootitem, SIGNAL(generateBillBitCashSignal(int)),
                      this, SLOT(generateBillBitCashClicked(int)));
    QObject::connect(qmlrootitem, SIGNAL(printfrontbillSignal()),
                      this, SLOT(printfrontbillClicked()));
    QObject::connect(qmlrootitem, SIGNAL(savebillSignal()),
                      this, SLOT(savebillSignalClicked()));
    QObject::connect(qmlrootitem, SIGNAL(savebillbackSignal()),
                      this, SLOT(savebillbackSignalClicked()));
    QObject::connect(qmlrootitem, SIGNAL(openpictureSignal()),
                      this, SLOT(openpictureClicked()));
    QObject::connect(qmlrootitem, SIGNAL(printbackbillSignal()),
                      this, SLOT(printbackbillClicked()));
    QObject::connect(qmlrootitem, SIGNAL(printstatementsignal(int, int, int)),
                      this, SLOT(printStatementsBtnClicked(int, int, int)));
    QObject::connect(qmlrootitem, SIGNAL(printTwitterBillSignal(QString, int, int)),
                      this, SLOT(printTwitterBillClicked(QString, int, int)));

    QObject::connect(qmlrootitem, SIGNAL(instaSwapCheckAmountSignal(bool, double)),
                      this, SLOT(InstaSwapCheckAmountClicked(bool, double)));
    QObject::connect(qmlrootitem, SIGNAL(instaSwapSendBtnSignal(bool, double, QString)),
                      this, SLOT(InstaSwapSendBtnClicked(bool, double, QString)));
    QObject::connect(qmlrootitem, SIGNAL(openLink(QString)),
                      this, SLOT(openLinkClicked(QString)));
    QObject::connect(qmlrootitem, SIGNAL(sendusecpusignal(bool)),
                      this, SLOT(sendusecpusignalClicked(bool)));
    QObject::connect(qmlrootitem, SIGNAL(sendcpumininfinfosignal()),
                      this, SLOT(sendcpumininfinfosignalClicked()));


    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(recurringpayments()));
    timer->start(1000*60*10);
    QTimer::singleShot(1000*60, this, SLOT(recurringpayments()));    

    QTimer *timerwelcomebonus = new QTimer(this);
    connect(timerwelcomebonus, SIGNAL(timeout()), this, SLOT(welcomebonus()));
    QTimer::singleShot(1000*70, this, SLOT(welcomebonus()));    

    this->manager = new QNetworkAccessManager(this);
    connect(this->manager, SIGNAL(finished(QNetworkReply*)), 
            this, SLOT(replyFinished(QNetworkReply*)));

    this->managerinsta = new QNetworkAccessManager(this);
    connect(this->managerinsta, SIGNAL(finished(QNetworkReply*)), 
            this, SLOT(replyFinishedInsta(QNetworkReply*)));

    this->managertwitch = new QNetworkAccessManager(this);
    connect(this->managertwitch, SIGNAL(finished(QNetworkReply*)), 
            this, SLOT(replyFinishedTwitch(QNetworkReply*)));

    this->managercheckversion = new QNetworkAccessManager(this);
    connect(this->managercheckversion, SIGNAL(finished(QNetworkReply*)), 
            this, SLOT(replyFinishedcheckversion(QNetworkReply*)));

    this->managernews = new QNetworkAccessManager(this);
    connect(this->managernews, SIGNAL(finished(QNetworkReply*)), 
            this, SLOT(replyFinishednews(QNetworkReply*)));

    this->managerinstaswap = new QNetworkAccessManager(this);
    connect(this->managerinstaswap, SIGNAL(finished(QNetworkReply*)), 
            this, SLOT(replyFinishedInstaSwap(QNetworkReply*)));

    QVariant returnedValue;
    fs::path path=GetWalletDir() / "wallet.dat";
    QVariant folder=QString::fromStdString(path.string());
    QMetaObject::invokeMethod(qmlrootitem, "setwalletfolder", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, folder));
    
    QVariant month = QDate::currentDate().month();
    QVariant year = QDate::currentDate().year();
    QMetaObject::invokeMethod(qmlrootitem, "setactualmonthandyear", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, month), Q_ARG(QVariant, year));


    miningtimer = new QTimer(this);
    connect(miningtimer, SIGNAL(timeout()), this, SLOT(updateminingstats()));    

    priceupdatetimer = new QTimer(this);
    connect(priceupdatetimer, SIGNAL(timeout()), this, SLOT(updateprice()));    
    priceupdatetimer->start(10000);
    updateprice();

    } else
#endif // ENABLE_WALLET
    {
        /* When compiled without wallet or -disablewallet is provided,
         * the central widget is the rpc console.
         */
        setCentralWidget(rpcConsole);
    }

    // Accept D&D of URIs
    setAcceptDrops(true);

    // Create actions for the toolbar, menu bar and tray/dock icon
    // Needs walletFrame to be initialized
    createActions();

    // Create application menu bar
    createMenuBar();

    // Create the toolbars
    createToolBars();

    // Create system tray icon and notification
    createTrayIcon(networkStyle);

    // Create status bar
    statusBar();

    statusBar()->hide();
    // Disable size grip because it looks ugly and nobody needs it
    statusBar()->setSizeGripEnabled(false);
    statusBar()->setStyleSheet("background-color: rgb(247, 247, 247);");

    // Status bar notification icons
    QFrame *frameBlocks = new QFrame();
    frameBlocks->setContentsMargins(0,0,0,0);
    frameBlocks->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    QHBoxLayout *frameBlocksLayout = new QHBoxLayout(frameBlocks);
    frameBlocksLayout->setContentsMargins(3,0,3,0);
    frameBlocksLayout->setSpacing(3);
    labelWalletEncryptionIcon = new QLabel();
    labelWalletHDStatusIcon = new QLabel();
    connectionsControl = new GUIUtil::ClickableLabel();
    labelBlocksIcon = new GUIUtil::ClickableLabel();
    if(enableWallet)
    {
        frameBlocksLayout->addStretch();
        frameBlocksLayout->addWidget(labelWalletEncryptionIcon);
        frameBlocksLayout->addWidget(labelWalletHDStatusIcon);
    }
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(connectionsControl);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelBlocksIcon);
    frameBlocksLayout->addStretch();

    // Progress bar and label for blocks download
    progressBarLabel = new QLabel();
    progressBarLabel->setVisible(false);
    progressBar = new GUIUtil::ProgressBar();
    progressBar->setAlignment(Qt::AlignCenter);
    progressBar->setVisible(false);

    // Override style sheet for progress bar for styles that have a segmented progress bar,
    // as they make the text unreadable (workaround for issue #1071)
    // See https://qt-project.org/doc/qt-4.8/gallery.html
    QString curStyle = QApplication::style()->metaObject()->className();
    if(curStyle == "QWindowsStyle" || curStyle == "QWindowsXPStyle")
    {
        progressBar->setStyleSheet("QProgressBar { background-color: #e8e8e8; border: 1px solid grey; border-radius: 7px; padding: 1px; text-align: center; } QProgressBar::chunk { background: QLinearGradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 #FF8000, stop: 1 orange); border-radius: 7px; margin: 0px; }");
    }

    statusBar()->addWidget(progressBarLabel);
    statusBar()->addWidget(progressBar);
    statusBar()->addPermanentWidget(frameBlocks);

    // Install event filter to be able to catch status tip events (QEvent::StatusTip)
    this->installEventFilter(this);

    // Initially wallet actions should be disabled
    setWalletActionsEnabled(false);

    // Subscribe to notifications from core
    subscribeToCoreSignals();

    connect(connectionsControl, SIGNAL(clicked(QPoint)), this, SLOT(toggleNetworkActive()));

    modalOverlay = new ModalOverlay(this->centralWidget());
#ifdef ENABLE_WALLET
    if(enableWallet) {
        connect(walletFrame, SIGNAL(requestedSyncWarningInfo()), this, SLOT(showModalOverlay()));
        connect(labelBlocksIcon, SIGNAL(clicked(QPoint)), this, SLOT(showModalOverlay()));
        connect(progressBar, SIGNAL(clicked(QPoint)), this, SLOT(showModalOverlay()));
    }
#endif    
    if (gArgs.GetBoolArg("-minimized",false))
    {
        QTimer::singleShot(2000, this, SLOT(toggleHidden()));
    }

    this->managercheckversion->get(QNetworkRequest(QUrl("https://wallet.choosebitcash.com/versioninfostable.txt")));
    this->managernews->get(QNetworkRequest(QUrl("https://wallet.choosebitcash.com/news.txt")));

}

BitcashGUI::~BitcashGUI()
{
    // Unsubscribe from notifications from core
    unsubscribeFromCoreSignals();

    QSettings settings;
    settings.setValue("MainWindowGeometry", saveGeometry());
    if(trayIcon) // Hide tray icon, as deleting will let it linger until quit (on Ubuntu)
        trayIcon->hide();
#ifdef Q_OS_MAC
    delete appMenuBar;
    MacDockIconHandler::cleanup();
#endif

    delete rpcConsole;
}


void BitcashGUI::createActions()
{
    QActionGroup *tabGroup = new QActionGroup(this);

    overviewAction = new QAction(platformStyle->SingleColorIcon(":/icons/overview"), tr("&Overview"), this);
    overviewAction->setStatusTip(tr("Show general overview of wallet"));
    overviewAction->setToolTip(overviewAction->statusTip());
    overviewAction->setCheckable(true);
    overviewAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_1));
    tabGroup->addAction(overviewAction);

    sendCoinsAction = new QAction(platformStyle->SingleColorIcon(":/icons/send"), tr("&Send"), this);
    sendCoinsAction->setStatusTip(tr("Send coins to a Bitcash address"));
    sendCoinsAction->setToolTip(sendCoinsAction->statusTip());
    sendCoinsAction->setCheckable(true);
    sendCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_2));
    tabGroup->addAction(sendCoinsAction);

    sendCoinsMenuAction = new QAction(platformStyle->TextColorIcon(":/icons/send"), sendCoinsAction->text(), this);
    sendCoinsMenuAction->setStatusTip(sendCoinsAction->statusTip());
    sendCoinsMenuAction->setToolTip(sendCoinsMenuAction->statusTip());

    receiveCoinsAction = new QAction(platformStyle->SingleColorIcon(":/icons/receiving_addresses"), tr("&Receive"), this);
    receiveCoinsAction->setStatusTip(tr("Request payments (generates QR codes and bitcash: URIs)"));
    receiveCoinsAction->setToolTip(receiveCoinsAction->statusTip());
    receiveCoinsAction->setCheckable(true);
    receiveCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_3));
    tabGroup->addAction(receiveCoinsAction);

    receiveCoinsMenuAction = new QAction(platformStyle->TextColorIcon(":/icons/receiving_addresses"), receiveCoinsAction->text(), this);
    receiveCoinsMenuAction->setStatusTip(receiveCoinsAction->statusTip());
    receiveCoinsMenuAction->setToolTip(receiveCoinsMenuAction->statusTip());

    historyAction = new QAction(platformStyle->SingleColorIcon(":/icons/history"), tr("&Transactions"), this);
    historyAction->setStatusTip(tr("Browse transaction history"));
    historyAction->setToolTip(historyAction->statusTip());
    historyAction->setCheckable(true);
    historyAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_4));
    tabGroup->addAction(historyAction);

    linksAction = new QAction(platformStyle->SingleColorIcon(":/icons/send"), tr("&BitCash Express"), this);
    linksAction->setStatusTip(tr("Send and receive BitCash via link"));
    linksAction->setToolTip(linksAction->statusTip());
    linksAction->setCheckable(true);
    linksAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_5));
    tabGroup->addAction(linksAction);


    miningAction = new QAction(platformStyle->SingleColorIcon(":/icons/tx_mined"), tr("&Mining"), this);
    miningAction->setStatusTip(tr("Mine BitCash"));
    miningAction->setToolTip(miningAction->statusTip());
    miningAction->setCheckable(true);
    miningAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_5));
    tabGroup->addAction(miningAction);

    nicknameAction = new QAction(platformStyle->SingleColorIcon(":/icons/edit"), tr("&Nicknames"), this);
    nicknameAction->setStatusTip(tr("Register a nickname"));
    nicknameAction->setToolTip(nicknameAction->statusTip());
    nicknameAction->setCheckable(true);
    nicknameAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_6));
    tabGroup->addAction(nicknameAction);

#ifdef ENABLE_WALLET
    // These showNormalIfMinimized are needed because Send Coins and Receive Coins
    // can be triggered from the tray menu, and need to show the GUI to be useful.
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(gotoOverviewPage()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(gotoSendCoinsPage()));
    connect(sendCoinsMenuAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(sendCoinsMenuAction, SIGNAL(triggered()), this, SLOT(gotoSendCoinsPage()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(gotoReceiveCoinsPage()));
    connect(receiveCoinsMenuAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(receiveCoinsMenuAction, SIGNAL(triggered()), this, SLOT(gotoReceiveCoinsPage()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(gotoHistoryPage()));
    connect(linksAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(linksAction, SIGNAL(triggered()), this, SLOT(gotoLinksPage()));
    connect(miningAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(miningAction, SIGNAL(triggered()), this, SLOT(gotoMiningPage()));
    connect(nicknameAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(nicknameAction, SIGNAL(triggered()), this, SLOT(gotoNicknamePage()));
    connect(exportAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));

#endif // ENABLE_WALLET

    quitAction = new QAction(platformStyle->TextColorIcon(":/icons/quit"), tr("E&xit"), this);
    quitAction->setStatusTip(tr("Quit application"));
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);
    aboutAction = new QAction(platformStyle->TextColorIcon(":/icons/about"), tr("&About %1").arg(tr(PACKAGE_NAME)), this);
    aboutAction->setStatusTip(tr("Show information about %1").arg(tr(PACKAGE_NAME)));
    aboutAction->setMenuRole(QAction::AboutRole);
    aboutAction->setEnabled(false);
    aboutQtAction = new QAction(platformStyle->TextColorIcon(":/icons/about_qt"), tr("About &Qt"), this);
    aboutQtAction->setStatusTip(tr("Show information about Qt"));
    aboutQtAction->setMenuRole(QAction::AboutQtRole);
    optionsAction = new QAction(platformStyle->TextColorIcon(":/icons/options"), tr("&Options..."), this);
    optionsAction->setStatusTip(tr("Modify configuration options for %1").arg(tr(PACKAGE_NAME)));
    optionsAction->setMenuRole(QAction::PreferencesRole);
    optionsAction->setEnabled(false);
    toggleHideAction = new QAction(platformStyle->TextColorIcon(":/icons/about"), tr("&Show / Hide"), this);
    toggleHideAction->setStatusTip(tr("Show or hide the main Window"));

    paperWalletAction = new QAction(platformStyle->TextColorIcon(":/res/assets/Navigation/receive-active.png"), tr("&Create a new paper wallet"), this);
    paperWalletAction->setStatusTip(tr("Create a paper wallet for save long term storage of your BitCash"));

    importKeyAction = new QAction(platformStyle->TextColorIcon(":/res/assets/Navigation/receive-active.png"), tr("&Import / Restore from paper wallet"), this);
    importKeyAction->setStatusTip(tr("Import a private key from a paper wallet to add your BitCash to the main wallet"));

    repairSyncAction = new QAction(platformStyle->TextColorIcon(":/icons/debugwindow"), tr("&Try to solve syncing issues"), this);
    repairSyncAction->setStatusTip(tr("Removes the invalid flag of all blocks. Tries to resolve syncing issues for example after a hard fork. "));

    resendWalletTxesAction = new QAction(platformStyle->TextColorIcon(":/icons/debugwindow"), tr("&Resend wallet transactions"), this);
    resendWalletTxesAction->setStatusTip(tr("This command will broadcast all transaction in the mempool to the peers immediately. "));

    rescanAction = new QAction(platformStyle->TextColorIcon(":/icons/debugwindow"), tr("&Rescan blockchain (will take very long)"), this);
    rescanAction->setStatusTip(tr("Rescan the blockchain for coins. "));

    rescanAction50 = new QAction(platformStyle->TextColorIcon(":/icons/debugwindow"), tr("Rescan last 50000 &blocks of the blockchain"), this);
    rescanAction50->setStatusTip(tr("Rescans the last 50000 blocks of the blockchain for coins. "));

    printWalletAction = new QAction(platformStyle->TextColorIcon(":/res/assets/Navigation/receive-active.png"), tr("&Backup wallet"), this);
    printWalletAction->setStatusTip(tr("Make a backup copy of your wallet."));

    exportAction = new QAction(platformStyle->TextColorIcon(":/icons/filesave"), tr("&Export transactions..."), this);
    exportAction->setStatusTip(tr("Export the transaction as CSV file"));

    encryptWalletAction = new QAction(platformStyle->TextColorIcon(":/icons/lock_closed"), tr("&Encrypt Wallet..."), this);
    encryptWalletAction->setStatusTip(tr("Encrypt the private keys that belong to your wallet"));
    encryptWalletAction->setCheckable(true);
    backupWalletAction = new QAction(platformStyle->TextColorIcon(":/icons/filesave"), tr("&Backup Wallet..."), this);
    backupWalletAction->setStatusTip(tr("Backup wallet to another location"));
    importWalletAction = new QAction(platformStyle->TextColorIcon(":/icons/open"), tr("&Import private keys from textfile..."), this);
    importWalletAction->setStatusTip(tr("Import a list of private keys from a textfile"));

    changePassphraseAction = new QAction(platformStyle->TextColorIcon(":/icons/key"), tr("&Change Passphrase..."), this);
    changePassphraseAction->setStatusTip(tr("Change the passphrase used for wallet encryption"));
    signMessageAction = new QAction(platformStyle->TextColorIcon(":/icons/edit"), tr("Sign &message..."), this);
    signMessageAction->setStatusTip(tr("Sign messages with your Bitcash addresses to prove you own them"));
    verifyMessageAction = new QAction(platformStyle->TextColorIcon(":/icons/verify"), tr("&Verify message..."), this);
    verifyMessageAction->setStatusTip(tr("Verify messages to ensure they were signed with specified Bitcash addresses"));


    websiteAction = new QAction(platformStyle->TextColorIcon(":/icons/bitcash"), tr("&BitCash website"), this);
    websiteAction->setStatusTip(tr("Open BitCash website. "));
    explorerAction = new QAction(platformStyle->TextColorIcon(":/icons/bitcash"), tr("&BitCash block explorer"), this);
    explorerAction->setStatusTip(tr("Open BitCash block explorer. "));
    supplyAction = new QAction(platformStyle->TextColorIcon(":/icons/bitcash"), tr("&BitCash supply"), this);
    supplyAction->setStatusTip(tr("Open BitCash supply information. "));

    openRPCConsoleAction = new QAction(platformStyle->TextColorIcon(":/icons/debugwindow"), tr("&Debug window"), this);
    openRPCConsoleAction->setStatusTip(tr("Open debugging and diagnostic console"));
    // initially disable the debug window menu item
    openRPCConsoleAction->setEnabled(false);

    usedSendingAddressesAction = new QAction(platformStyle->TextColorIcon(":/icons/address-book"), tr("&Sending addresses..."), this);
    usedSendingAddressesAction->setStatusTip(tr("Show the list of used sending addresses and labels"));
    usedReceivingAddressesAction = new QAction(platformStyle->TextColorIcon(":/icons/address-book"), tr("&Receiving addresses..."), this);
    usedReceivingAddressesAction->setStatusTip(tr("Show the list of used receiving addresses and labels"));

    openAction = new QAction(platformStyle->TextColorIcon(":/icons/open"), tr("Open &URI..."), this);
    openAction->setStatusTip(tr("Open a bitcash: URI or payment request"));

    showHelpMessageAction = new QAction(platformStyle->TextColorIcon(":/icons/info"), tr("&Command-line options"), this);
    showHelpMessageAction->setMenuRole(QAction::NoRole);
    showHelpMessageAction->setStatusTip(tr("Show the %1 help message to get a list with possible Bitcash command-line options").arg(tr(PACKAGE_NAME)));

    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(aboutClicked()));
    connect(aboutQtAction, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
    connect(optionsAction, SIGNAL(triggered()), this, SLOT(optionsClicked()));
    connect(toggleHideAction, SIGNAL(triggered()), this, SLOT(toggleHidden()));
    connect(showHelpMessageAction, SIGNAL(triggered()), this, SLOT(showHelpMessageClicked()));
    connect(openRPCConsoleAction, SIGNAL(triggered()), this, SLOT(showDebugWindow()));
    // prevents an open debug window from becoming stuck/unusable on client shutdown
    connect(quitAction, SIGNAL(triggered()), rpcConsole, SLOT(hide()));
    connect(paperWalletAction, SIGNAL(triggered()), this, SLOT(paperWalletClicked()));
    connect(printWalletAction, SIGNAL(triggered()), this, SLOT(printMainWalletClicked()));
    connect(importKeyAction, SIGNAL(triggered()), this, SLOT(importKeyClicked()));
    connect(repairSyncAction, SIGNAL(triggered()), this, SLOT(repairSyncIssuesClicked()));
    connect(resendWalletTxesAction, SIGNAL(triggered()), this, SLOT(resendWalletTxesClicked()));
    connect(rescanAction, SIGNAL(triggered()), this, SLOT(rescanClicked()));
    connect(rescanAction50, SIGNAL(triggered()), this, SLOT(rescan50Clicked()));

    connect(websiteAction, SIGNAL(triggered()), this, SLOT(websiteClicked()));
    connect(explorerAction, SIGNAL(triggered()), this, SLOT(explorerClicked()));
    connect(supplyAction, SIGNAL(triggered()), this, SLOT(supplyClicked()));


#ifdef ENABLE_WALLET
    if(walletFrame)
    {
        connect(encryptWalletAction, SIGNAL(triggered(bool)), walletFrame, SLOT(encryptWallet(bool)));
        connect(backupWalletAction, SIGNAL(triggered()), walletFrame, SLOT(backupWallet()));
        QObject::connect(qmlrootitem, SIGNAL(backupwalletfileSignal()), walletFrame, SLOT(backupWallet()));

        connect(importWalletAction, SIGNAL(triggered()), walletFrame, SLOT(importWallet()));
        connect(changePassphraseAction, SIGNAL(triggered()), walletFrame, SLOT(changePassphrase()));
        connect(signMessageAction, SIGNAL(triggered()), this, SLOT(gotoSignMessageTab()));
        connect(verifyMessageAction, SIGNAL(triggered()), this, SLOT(gotoVerifyMessageTab()));
        connect(usedSendingAddressesAction, SIGNAL(triggered()), walletFrame, SLOT(usedSendingAddresses()));
        connect(usedReceivingAddressesAction, SIGNAL(triggered()), walletFrame, SLOT(usedReceivingAddresses()));
        connect(openAction, SIGNAL(triggered()), this, SLOT(openClicked()));
    }
#endif // ENABLE_WALLET

    new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_C), this, SLOT(showDebugWindowActivateConsole()));
    new QShortcut(QKeySequence(Qt::CTRL + Qt::SHIFT + Qt::Key_D), this, SLOT(showDebugWindow()));

    #ifdef WIN32
        QVariant returnedValue;
        QMetaObject::invokeMethod(qmlrootitem, "makeminingpoolvisible", Q_RETURN_ARG(QVariant, returnedValue));
    #endif
}

void BitcashGUI::createMenuBar()
{
#ifdef Q_OS_MAC
    // Create a decoupled menu bar on Mac which stays even if the window is closed
    appMenuBar = new QMenuBar();
#else
    // Get the main window's menu bar on other platforms
    appMenuBar = menuBar();
#endif

//    appMenuBar->setStyleSheet("background: rgb(247, 247, 247);");
    // Configure the menus
    QMenu *file = appMenuBar->addMenu(tr("&File"));
    if(walletFrame)
    {
        file->addAction(exportAction);
        file->addAction(printWalletAction);
        file->addAction(signMessageAction);
        file->addAction(verifyMessageAction);
        file->addSeparator();
        file->addAction(usedSendingAddressesAction);
        file->addAction(usedReceivingAddressesAction);
//        file->addSeparator();
  //      file->addAction(importWalletAction);
        file->addSeparator();
    }
    file->addAction(quitAction);

    QMenu *paperwallet = appMenuBar->addMenu(tr("&Paper wallet"));
    if(walletFrame)
    {
        paperwallet->addAction(paperWalletAction);
        paperwallet->addAction(importKeyAction);

    }

    QMenu *minimizetotray = appMenuBar->addMenu(tr("&Minimize to tray"));
    if(walletFrame)
    {
        minimizetotray->addAction(toggleHideAction);

    }

    QMenu *repairwallet = appMenuBar->addMenu(tr("&Repair"));
    if(walletFrame)
    {
        repairwallet->addAction(repairSyncAction);
        repairwallet->addAction(resendWalletTxesAction);
        repairwallet->addAction(rescanAction);
        repairwallet->addAction(rescanAction50);
    }

    QMenu *settings = appMenuBar->addMenu(tr("&Settings"));
    if(walletFrame)
    {
        settings->addAction(encryptWalletAction);
        settings->addAction(changePassphraseAction);
        settings->addSeparator();
    }
    settings->addAction(optionsAction);

    QMenu *links = appMenuBar->addMenu(tr("&Links"));
    if(walletFrame)
    {
        links->addAction(websiteAction);
        links->addAction(explorerAction);
        links->addAction(supplyAction);
    }

    QMenu *help = appMenuBar->addMenu(tr("&Help"));
    if(walletFrame)
    {
        help->addAction(openRPCConsoleAction);
    }
    help->addAction(showHelpMessageAction);
    help->addSeparator();
    help->addAction(aboutAction);
    help->addAction(aboutQtAction);
}

void BitcashGUI::createToolBars()
{
    if(walletFrame)
    {
/*        QToolBar *toolbar = addToolBar(tr("Tabs toolbar"));
        appToolBar = toolbar;
        toolbar->setContextMenuPolicy(Qt::PreventContextMenu);
        toolbar->setMovable(false);
        toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        toolbar->addAction(overviewAction);
        toolbar->addAction(sendCoinsAction);
        toolbar->addAction(receiveCoinsAction);
        toolbar->addAction(linksAction);
        toolbar->addAction(historyAction);
        toolbar->addAction(miningAction);
        toolbar->addAction(nicknameAction);
        overviewAction->setChecked(true);
*/
#ifdef ENABLE_WALLET
  /*      QWidget *spacer = new QWidget();
        spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        toolbar->addWidget(spacer);*/

        m_wallet_selector = new QComboBox();
        connect(m_wallet_selector, SIGNAL(currentIndexChanged(const QString&)), this, SLOT(setCurrentWallet(const QString&)));
#endif
    }
}

void BitcashGUI::setClientModel(ClientModel *_clientModel)
{
    this->clientModel = _clientModel;
    if(_clientModel)
    {
        // Create system tray menu (or setup the dock menu) that late to prevent users from calling actions,
        // while the client has not yet fully loaded
        createTrayIconMenu();

        // Keep up to date with client
        updateNetworkState();
        connect(_clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(setNumConnections(int)));
        connect(_clientModel, SIGNAL(networkActiveChanged(bool)), this, SLOT(setNetworkActive(bool)));

        modalOverlay->setKnownBestHeight(_clientModel->getHeaderTipHeight(), QDateTime::fromTime_t(_clientModel->getHeaderTipTime()));
        setNumBlocks(m_node.getNumBlocks(), QDateTime::fromTime_t(m_node.getLastBlockTime()), m_node.getVerificationProgress(), false);
        connect(_clientModel, SIGNAL(numBlocksChanged(int,QDateTime,double,bool)), this, SLOT(setNumBlocks(int,QDateTime,double,bool)));

        // Receive and report messages from client model
        connect(_clientModel, SIGNAL(message(QString,QString,unsigned int)), this, SLOT(message(QString,QString,unsigned int)));

        // Show progress dialog
        connect(_clientModel, SIGNAL(showProgress(QString,int)), this, SLOT(showProgress(QString,int)));
        
        rpcConsole->setClientModel(_clientModel);
#ifdef ENABLE_WALLET
        if(walletFrame)
        {
            walletFrame->setClientModel(_clientModel);
        }
#endif // ENABLE_WALLET
        
        OptionsModel* optionsModel = _clientModel->getOptionsModel();
        if(optionsModel)
        {
            // be aware of the tray icon disable state change reported by the OptionsModel object.
            connect(optionsModel,SIGNAL(hideTrayIconChanged(bool)),this,SLOT(setTrayIconVisible(bool)));
        
            // initialize the disable state of the tray icon with the current value in the model.
            setTrayIconVisible(optionsModel->getHideTrayIcon());
        }
        QVariant returnedValue;
        QMetaObject::invokeMethod(qmlrootitem, "clearpaymentlistmodel", Q_RETURN_ARG(QVariant, returnedValue));//need to clear model, sometimes model already has been filled by timer
        SendLinksToModel();
        SendPaymentsToModel();
        SendOrdersToModel();
    } else {
        // Disable possibility to show main window via action
        toggleHideAction->setEnabled(false);
        if(trayIconMenu)
        {
            // Disable context menu on tray icon
            trayIconMenu->clear();
        }
        // Propagate cleared model to child objects
        rpcConsole->setClientModel(nullptr);
#ifdef ENABLE_WALLET
        if (walletFrame)
        {
            walletFrame->setClientModel(nullptr);
        }
#endif // ENABLE_WALLET
    }
}

#ifdef ENABLE_WALLET
bool BitcashGUI::addWallet(WalletModel *walletModel)
{
    if(!walletFrame)
        return false;
    const QString name = walletModel->getWalletName();
    setWalletActionsEnabled(true);
    m_wallet_selector->addItem(name);
    if (m_wallet_selector->count() == 2) {
        m_wallet_selector_label = new QLabel();
        m_wallet_selector_label->setText(tr("Wallet:") + " ");
        m_wallet_selector_label->setBuddy(m_wallet_selector);
        appToolBar->addWidget(m_wallet_selector_label);
        appToolBar->addWidget(m_wallet_selector);
    }
    rpcConsole->addWallet(walletModel);
    return walletFrame->addWallet(walletModel);
}

bool BitcashGUI::setCurrentWallet(const QString& name)
{
    if(!walletFrame)
        return false;
    return walletFrame->setCurrentWallet(name);
}

void BitcashGUI::removeAllWallets()
{
    if(!walletFrame)
        return;
    setWalletActionsEnabled(false);
    walletFrame->removeAllWallets();
}
#endif // ENABLE_WALLET

void BitcashGUI::setWalletActionsEnabled(bool enabled)
{
    overviewAction->setEnabled(enabled);
    sendCoinsAction->setEnabled(enabled);
    sendCoinsMenuAction->setEnabled(enabled);
    receiveCoinsAction->setEnabled(enabled);
    receiveCoinsMenuAction->setEnabled(enabled);
    linksAction->setEnabled(enabled);
    historyAction->setEnabled(enabled);
    miningAction->setEnabled(enabled);
    nicknameAction->setEnabled(enabled);
    encryptWalletAction->setEnabled(enabled);
    backupWalletAction->setEnabled(enabled);
    changePassphraseAction->setEnabled(enabled);
    signMessageAction->setEnabled(enabled);
    verifyMessageAction->setEnabled(enabled);
    usedSendingAddressesAction->setEnabled(enabled);
    usedReceivingAddressesAction->setEnabled(enabled);
    openAction->setEnabled(enabled);
}

void BitcashGUI::createTrayIcon(const NetworkStyle *networkStyle)
{
#ifndef Q_OS_MAC
    trayIcon = new QSystemTrayIcon(this);
    QString toolTip = tr("%1 client").arg(tr(PACKAGE_NAME)) + " " + networkStyle->getTitleAddText();
    trayIcon->setToolTip(toolTip);
    trayIcon->setIcon(networkStyle->getTrayAndWindowIcon());
    trayIcon->hide();
#endif

    notificator = new Notificator(QApplication::applicationName(), trayIcon, this);
}

void BitcashGUI::createTrayIconMenu()
{
#ifndef Q_OS_MAC
    // return if trayIcon is unset (only on non-Mac OSes)
    if (!trayIcon)
        return;

    trayIconMenu = new QMenu(this);
    trayIcon->setContextMenu(trayIconMenu);

    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
#else
    // Note: On Mac, the dock icon is used to provide the tray's functionality.
    MacDockIconHandler *dockIconHandler = MacDockIconHandler::instance();
    dockIconHandler->setMainWindow(static_cast<QMainWindow*>(this));
    trayIconMenu = dockIconHandler->dockMenu();
#endif

    // Configuration of the tray icon (or dock icon) icon menu
    trayIconMenu->addAction(toggleHideAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(optionsAction);
    trayIconMenu->addAction(openRPCConsoleAction);
#ifndef Q_OS_MAC // This is built-in on Mac
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);
#endif
}

#ifndef Q_OS_MAC
void BitcashGUI::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if(reason == QSystemTrayIcon::Trigger)
    {
        // Click on system tray icon triggers show/hide of the main window
        toggleHidden();
    }
}
#endif

extern SecureString thewalletpassword;

bool BitcashGUI::UserKnowsPassword()
{
    if(!clientModel || !clientModel->getOptionsModel())
        return false;

  if (!walletFrame) {
        return false;
    }
    WalletView * const walletView = walletFrame->currentWalletView();
    if (!walletView) {
        return false;
    }
    WalletModel * const walletModel = walletView->getWalletModel();
    return !clientModel->getOptionsModel()->getAskForPassword() || thewalletpassword == "" || walletModel->requestPassword();
}


void BitcashGUI::optionsClicked()
{
    if(!clientModel || !clientModel->getOptionsModel())
        return;

    if (UserKnowsPassword()) {
      OptionsDialog dlg(this, enableWallet);
      dlg.setModel(clientModel->getOptionsModel());
      dlg.exec();
    }
}

void BitcashGUI::aboutClicked()
{
    if(!clientModel)
        return;

    HelpMessageDialog dlg(m_node, this, true);
    dlg.exec();
}

void BitcashGUI::showDebugWindow()
{
    if (UserKnowsPassword()) {
        rpcConsole->showNormal();
        rpcConsole->show();
        rpcConsole->raise();
        rpcConsole->activateWindow();
    }
}

void BitcashGUI::showDebugWindowActivateConsole()
{
    rpcConsole->setTabFocus(RPCConsole::TAB_CONSOLE);
    showDebugWindow();
}

void BitcashGUI::showHelpMessageClicked()
{
    helpMessageDialog->show();
}

#ifdef ENABLE_WALLET
void BitcashGUI::openClicked()
{
    OpenURIDialog dlg(this);
    if(dlg.exec())
    {
        Q_EMIT receivedURI(dlg.getURI());
    }
}

void BitcashGUI::gotoOverviewPage()
{
    overviewAction->setChecked(true);
    if (walletFrame) walletFrame->gotoOverviewPage();
}

void BitcashGUI::gotoHistoryPage()
{
    historyAction->setChecked(true);
    if (walletFrame) walletFrame->gotoHistoryPage();
}

void BitcashGUI::gotoMiningPage()
{
    miningAction->setChecked(true);
    if (walletFrame) walletFrame->gotoMiningPage();
}

void BitcashGUI::gotoLinksPage()
{
    linksAction->setChecked(true);
    if (walletFrame) walletFrame->gotoLinksPage();
}


void BitcashGUI::gotoNicknamePage()
{
    nicknameAction->setChecked(true);
    if (walletFrame) walletFrame->gotoNicknamePage();
}

void BitcashGUI::gotoReceiveCoinsPage()
{
    receiveCoinsAction->setChecked(true);
    if (walletFrame) walletFrame->gotoReceiveCoinsPage();
}

void BitcashGUI::gotoSendCoinsPage(QString addr)
{
    sendCoinsAction->setChecked(true);
    if (walletFrame) walletFrame->gotoSendCoinsPage(addr);
}

void BitcashGUI::gotoSignMessageTab(QString addr)
{
    if (walletFrame) walletFrame->gotoSignMessageTab(addr);
}

void BitcashGUI::gotoVerifyMessageTab(QString addr)
{
    if (walletFrame) walletFrame->gotoVerifyMessageTab(addr);
}
#endif // ENABLE_WALLET

void BitcashGUI::updateNetworkState()
{
    int count = clientModel->getNumConnections();
    QString icon;
    switch(count)
    {
    case 0: icon = ":/icons/connect_0"; break;
    case 1: case 2: case 3: icon = ":/icons/connect_1"; break;
    case 4: case 5: case 6: icon = ":/icons/connect_2"; break;
    case 7: case 8: case 9: icon = ":/icons/connect_3"; break;
    default: icon = ":/icons/connect_4"; break;
    }

    QString tooltip;

    if (m_node.getNetworkActive()) {
        tooltip = tr("%n active connection(s) to Bitcash network", "", count) + QString(".<br>") + tr("Click to disable network activity.");
    } else {
        tooltip = tr("Network activity disabled.") + QString("<br>") + tr("Click to enable network activity again.");
        icon = ":/icons/network_disabled";
    }

    // Don't word-wrap this (fixed-width) tooltip
    tooltip = QString("<nobr>") + tooltip + QString("</nobr>");
    connectionsControl->setToolTip(tooltip);

    connectionsControl->setPixmap(platformStyle->SingleColorIcon(icon).pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
}

void BitcashGUI::setNumConnections(int count)
{
    updateNetworkState();
}

void BitcashGUI::setNetworkActive(bool networkActive)
{
    updateNetworkState();
}

void BitcashGUI::HideProgressBarLabelText()
{
        QVariant returnedValue;
        QVariant show=false;
        QVariant s="";
        QMetaObject::invokeMethod(qmlrootitem, "setprogressbarlabel", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, show), Q_ARG(QVariant, s));
}

void BitcashGUI::HideProgressBarLabelText2()
{
        QVariant returnedValue;
        QVariant show=false;
        QVariant s="";
        QMetaObject::invokeMethod(qmlrootitem, "setprogressbarlabel2", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, show), Q_ARG(QVariant, s));
}


void BitcashGUI::HideProgressBar()
{
        QVariant returnedValue;
        QVariant show=false;
        QVariant s=0;
        QMetaObject::invokeMethod(qmlrootitem, "setprogressbarpercent", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, show), Q_ARG(QVariant, s));
}

void BitcashGUI::SetProgressBarPercent(QVariant s)
{
        QVariant returnedValue;
        QVariant show=true;
//        QVariant s=tr("Synchronizing with network...");
        QMetaObject::invokeMethod(qmlrootitem, "setprogressbarpercent", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, show), Q_ARG(QVariant, s));
}


void BitcashGUI::SetProgressBarLabelText(QVariant s)
{
        QVariant returnedValue;
        QVariant show=true;
//        QVariant s=tr("Synchronizing with network...");
        QMetaObject::invokeMethod(qmlrootitem, "setprogressbarlabel", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, show), Q_ARG(QVariant, s));
}

void BitcashGUI::SetProgressBarLabelText2(QVariant s)
{
        QVariant returnedValue;
        QVariant show=true;
//        QVariant s=tr("Synchronizing with network...");
        QMetaObject::invokeMethod(qmlrootitem, "setprogressbarlabel2", Q_RETURN_ARG(QVariant, returnedValue), Q_ARG(QVariant, show), Q_ARG(QVariant, s));
}


void BitcashGUI::updateHeadersSyncProgressLabel()
{
    int64_t headersTipTime = clientModel->getHeaderTipTime();
    int headersTipHeight = clientModel->getHeaderTipHeight();
    int estHeadersLeft = (GetTime() - headersTipTime) / Params().GetConsensus().nPowTargetSpacing;
    if (estHeadersLeft > HEADER_HEIGHT_DELTA_SYNC) {
        SetProgressBarLabelText(tr("Syncing Headers (%1%)...").arg(QString::number(100.0 / (headersTipHeight+estHeadersLeft)*headersTipHeight, 'f', 1)));
    }
}

void BitcashGUI::setNumBlocks(int count, const QDateTime& blockDate, double nVerificationProgress, bool header)
{
    if (modalOverlay)
    {
        if (header)
            modalOverlay->setKnownBestHeight(count, blockDate);
        else
            modalOverlay->tipUpdate(count, blockDate, nVerificationProgress);
    }
    if (!clientModel)
        return;

    // Prevent orphan statusbar messages (e.g. hover Quit in main menu, wait until chain-sync starts -> garbled text)
    statusBar()->clearMessage();

    // Acquire current block source
    enum BlockSource blockSource = clientModel->getBlockSource();
    switch (blockSource) {
        case BlockSource::NETWORK:
            if (header) {
                updateHeadersSyncProgressLabel();
                return;
            }
            SetProgressBarLabelText(tr("Synchronizing with network..."));

            updateHeadersSyncProgressLabel();
            break;
        case BlockSource::DISK:
            if (header) {
                SetProgressBarLabelText(tr("Indexing blocks on disk..."));
            } else {
                SetProgressBarLabelText(tr("Processing blocks on disk..."));
            }
            break;
        case BlockSource::REINDEX:
            SetProgressBarLabelText(tr("Reindexing blocks on disk..."));
            break;
        case BlockSource::NONE:
            if (header) {
                return;
            }
            SetProgressBarLabelText(tr("Connecting to peers..."));
            break;
    }

    QString tooltip;

    QDateTime currentDate = QDateTime::currentDateTime();
    qint64 secs = blockDate.secsTo(currentDate);

    tooltip = tr("Processed %n block(s) of transaction history.", "", count);

    // Set icon state: spinning if catching up, tick otherwise
    if(secs < 90*60)
    {
        tooltip = tr("Up to date") + QString(".<br>") + tooltip;
        labelBlocksIcon->setPixmap(platformStyle->SingleColorIcon(":/icons/synced").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));

#ifdef ENABLE_WALLET
        if(walletFrame)
        {
            walletFrame->showOutOfSyncWarning(false);
            modalOverlay->showHide(true, true);
        }
#endif // ENABLE_WALLET

        HideProgressBarLabelText();
        HideProgressBar();
       HideProgressBarLabelText2();
    }
    else
    {
        QString timeBehindText = GUIUtil::formatNiceTimeOffset(secs);

        progressBarLabel->setVisible(true);
        SetProgressBarLabelText2(tr("%1 behind").arg(timeBehindText));
        progressBar->setFormat(tr("%1 behind").arg(timeBehindText));
        progressBar->setMaximum(1000000000);
        progressBar->setValue(nVerificationProgress * 1000000000.0 + 0.5);
        progressBar->setVisible(true);
        SetProgressBarPercent(nVerificationProgress);                                                   

        tooltip = tr("Catching up...") + QString("<br>") + tooltip;
        if(count != prevBlocks)
        {
            labelBlocksIcon->setPixmap(platformStyle->SingleColorIcon(QString(
                ":/movies/spinner-%1").arg(spinnerFrame, 3, 10, QChar('0')))
                .pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
            spinnerFrame = (spinnerFrame + 1) % SPINNER_FRAMES;
        }
        prevBlocks = count;

#ifdef ENABLE_WALLET
        if(walletFrame)
        {
            walletFrame->showOutOfSyncWarning(true);
            modalOverlay->showHide();
        }
#endif // ENABLE_WALLET

        tooltip += QString("<br>");
        tooltip += tr("Last received block was generated %1 ago.").arg(timeBehindText);
        tooltip += QString("<br>");
        tooltip += tr("Transactions after this will not yet be visible.");
    }

    // Don't word-wrap this (fixed-width) tooltip
    tooltip = QString("<nobr>") + tooltip + QString("</nobr>");

    labelBlocksIcon->setToolTip(tooltip);
    progressBarLabel->setToolTip(tooltip);
    progressBar->setToolTip(tooltip);
}

void BitcashGUI::message(const QString &title, const QString &message, unsigned int style, bool *ret)
{
    QString strTitle = tr("Bitcash"); // default title
    // Default to information icon
    int nMBoxIcon = QMessageBox::Information;
    int nNotifyIcon = Notificator::Information;

    QString msgType;

    // Prefer supplied title over style based title
    if (!title.isEmpty()) {
        msgType = title;
    }
    else {
        switch (style) {
        case CClientUIInterface::MSG_ERROR:
            msgType = tr("Error");
            break;
        case CClientUIInterface::MSG_WARNING:
            msgType = tr("Warning");
            break;
        case CClientUIInterface::MSG_INFORMATION:
            msgType = tr("Information");
            break;
        default:
            break;
        }
    }
    // Append title to "Bitcash - "
    if (!msgType.isEmpty())
        strTitle += " - " + msgType;

    // Check for error/warning icon
    if (style & CClientUIInterface::ICON_ERROR) {
        nMBoxIcon = QMessageBox::Critical;
        nNotifyIcon = Notificator::Critical;
    }
    else if (style & CClientUIInterface::ICON_WARNING) {
        nMBoxIcon = QMessageBox::Warning;
        nNotifyIcon = Notificator::Warning;
    }

    // Display message
    if (style & CClientUIInterface::MODAL) {
        // Check for buttons, use OK as default, if none was supplied
        QMessageBox::StandardButton buttons;
        if (!(buttons = (QMessageBox::StandardButton)(style & CClientUIInterface::BTN_MASK)))
            buttons = QMessageBox::Ok;

        showNormalIfMinimized();
        QMessageBox mBox(static_cast<QMessageBox::Icon>(nMBoxIcon), strTitle, message, buttons, this);
        mBox.setTextFormat(Qt::PlainText);
        int r = mBox.exec();
        if (ret != nullptr)
            *ret = r == QMessageBox::Ok;
    }
    else
        notificator->notify(static_cast<Notificator::Class>(nNotifyIcon), strTitle, message);
}

void BitcashGUI::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
#ifndef Q_OS_MAC // Ignored on Mac
    if(e->type() == QEvent::WindowStateChange)
    {
        if(clientModel && clientModel->getOptionsModel() && clientModel->getOptionsModel()->getMinimizeToTray())
        {
            QWindowStateChangeEvent *wsevt = static_cast<QWindowStateChangeEvent*>(e);
            if(!(wsevt->oldState() & Qt::WindowMinimized) && isMinimized())
            {
                QTimer::singleShot(0, this, SLOT(hide()));
                e->ignore();
            }
            else if((wsevt->oldState() & Qt::WindowMinimized) && !isMinimized())
            {
                QTimer::singleShot(0, this, SLOT(show()));
                e->ignore();
            }
        }
    }
#endif
}

void BitcashGUI::closeEvent(QCloseEvent *event)
{
#ifndef Q_OS_MAC // Ignored on Mac
    if(clientModel && clientModel->getOptionsModel())
    {
        if(!clientModel->getOptionsModel()->getMinimizeOnClose())
        {
            // close rpcConsole in case it was open to make some space for the shutdown window
            rpcConsole->close();

            QApplication::quit();
        }
        else
        {
            QMainWindow::showMinimized();
            event->ignore();
        }
    }
#else
    QMainWindow::closeEvent(event);
#endif
}

void BitcashGUI::showEvent(QShowEvent *event)
{
    // enable the debug window when the main window shows up
    openRPCConsoleAction->setEnabled(true);
    aboutAction->setEnabled(true);
    optionsAction->setEnabled(true);
}

#ifdef ENABLE_WALLET
void BitcashGUI::incomingTransaction(const QString& date, int unit, const CAmount& amount, const QString& type, const QString& address, const QString& label, const QString& walletName)
{
    // On new transaction, make an info balloon
    QString msg = tr("Date: %1\n").arg(date) +
                  tr("Amount: %1\n").arg(BitcashUnits::formatWithUnit(unit, amount, true));
    if (m_node.getWallets().size() > 1 && !walletName.isEmpty()) {
        msg += tr("Wallet: %1\n").arg(walletName);
    }
    msg += tr("Type: %1\n").arg(type);
    if (!label.isEmpty())
        msg += tr("Label: %1\n").arg(label);
    else if (!address.isEmpty())
        msg += tr("Address: %1\n").arg(address);
    message((amount)<0 ? tr("Sent transaction") : tr("Incoming transaction"),
             msg, CClientUIInterface::MSG_INFORMATION);
}
#endif // ENABLE_WALLET

void BitcashGUI::dragEnterEvent(QDragEnterEvent *event)
{
    // Accept only URIs
    if(event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void BitcashGUI::dropEvent(QDropEvent *event)
{
    if(event->mimeData()->hasUrls())
    {
        for (const QUrl &uri : event->mimeData()->urls())
        {
            Q_EMIT receivedURI(uri.toString());
        }
    }
    event->acceptProposedAction();
}

bool BitcashGUI::eventFilter(QObject *object, QEvent *event)
{
    // Catch status tip events
    if (event->type() == QEvent::StatusTip)
    {
        // Prevent adding text from setStatusTip(), if we currently use the status bar for displaying other stuff
        if (progressBarLabel->isVisible() || progressBar->isVisible())
            return true;
    }
    return QMainWindow::eventFilter(object, event);
}

#ifdef ENABLE_WALLET
bool BitcashGUI::handlePaymentRequest(const SendCoinsRecipient& recipient)
{
    // URI has to be valid
    if (walletFrame && walletFrame->handlePaymentRequest(recipient))
    {
        showNormalIfMinimized();
        gotoSendCoinsPage();
        return true;
    }
    return false;
}

void BitcashGUI::setHDStatus(int hdEnabled)
{
    labelWalletHDStatusIcon->setPixmap(platformStyle->SingleColorIcon(hdEnabled ? ":/icons/hd_enabled" : ":/icons/hd_disabled").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
    labelWalletHDStatusIcon->setToolTip(hdEnabled ? tr("HD key generation is <b>enabled</b>") : tr("HD key generation is <b>disabled</b>"));

    // eventually disable the QLabel to set its opacity to 50% 
    labelWalletHDStatusIcon->setEnabled(hdEnabled);
}

void BitcashGUI::setEncryptionStatus(int status)
{
    switch(status)
    {
    case WalletModel::Unencrypted:
        labelWalletEncryptionIcon->hide();
        encryptWalletAction->setChecked(false);
        changePassphraseAction->setEnabled(false);
        encryptWalletAction->setEnabled(true);
        break;
    case WalletModel::Unlocked:
        labelWalletEncryptionIcon->show();
        labelWalletEncryptionIcon->setPixmap(platformStyle->SingleColorIcon(":/icons/lock_open").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelWalletEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>unlocked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        break;
    case WalletModel::Locked:
        labelWalletEncryptionIcon->show();
        labelWalletEncryptionIcon->setPixmap(platformStyle->SingleColorIcon(":/icons/lock_closed").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelWalletEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>locked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        break;
    }
}

void BitcashGUI::updateWalletStatus()
{
    if (!walletFrame) {
        return;
    }
    WalletView * const walletView = walletFrame->currentWalletView();
    if (!walletView) {
        return;
    }
    WalletModel * const walletModel = walletView->getWalletModel();
    setEncryptionStatus(walletModel->getEncryptionStatus());
    setHDStatus(walletModel->wallet().hdEnabled());
}
#endif // ENABLE_WALLET

void BitcashGUI::showNormalIfMinimized(bool fToggleHidden)
{
    if(!clientModel)
        return;

    // activateWindow() (sometimes) helps with keyboard focus on Windows
    if (isHidden())
    {
        show();
        activateWindow();
    }
    else if (isMinimized())
    {
        showNormal();
        activateWindow();
    }
    else if(fToggleHidden) {
        hide();
    }
}

void BitcashGUI::toggleHidden()
{
    showNormalIfMinimized(true);
}

void BitcashGUI::detectShutdown()
{
    if (m_node.shutdownRequested())
    {
        if(rpcConsole)
            rpcConsole->hide();
        qApp->quit();
    }
}

void BitcashGUI::showProgress(const QString &title, int nProgress)
{
    if (inimporting) {
        return;
    }
    if (nProgress == 0)
    {
        progressDialog = new QProgressDialog(title, "", 0, 100);
        progressDialog->setWindowModality(Qt::ApplicationModal);
        progressDialog->setMinimumDuration(0);
        progressDialog->setCancelButton(0);
        progressDialog->setAutoClose(false);
        progressDialog->setValue(0);
    }
    else if (nProgress == 100)
    {
        if (progressDialog)
        {
            progressDialog->close();
            progressDialog->deleteLater();
        }
    }
    else if (progressDialog)
        progressDialog->setValue(nProgress);
}

void BitcashGUI::setTrayIconVisible(bool fHideTrayIcon)
{
    if (trayIcon)
    {
        trayIcon->setVisible(!fHideTrayIcon);
    }
}

void BitcashGUI::showModalOverlay()
{
    if (modalOverlay && (progressBar->isVisible() || modalOverlay->isLayerVisible()))
        modalOverlay->toggleVisibility();
}

static bool ThreadSafeMessageBox(BitcashGUI *gui, const std::string& message, const std::string& caption, unsigned int style)
{
    bool modal = (style & CClientUIInterface::MODAL);
    // The SECURE flag has no effect in the Qt GUI.
    // bool secure = (style & CClientUIInterface::SECURE);
    style &= ~CClientUIInterface::SECURE;
    bool ret = false;
    // In case of modal message, use blocking connection to wait for user to click a button
    QMetaObject::invokeMethod(gui, "message",
                               modal ? GUIUtil::blockingGUIThreadConnection() : Qt::QueuedConnection,
                               Q_ARG(QString, QString::fromStdString(caption)),
                               Q_ARG(QString, QString::fromStdString(message)),
                               Q_ARG(unsigned int, style),
                               Q_ARG(bool*, &ret));
    return ret;
}

void BitcashGUI::subscribeToCoreSignals()
{
    // Connect signals to client
    m_handler_message_box = m_node.handleMessageBox(boost::bind(ThreadSafeMessageBox, this, _1, _2, _3));
    m_handler_question = m_node.handleQuestion(boost::bind(ThreadSafeMessageBox, this, _1, _3, _4));
}

void BitcashGUI::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    m_handler_message_box->disconnect();
    m_handler_question->disconnect();
}

void BitcashGUI::toggleNetworkActive()
{
    m_node.setNetworkActive(!m_node.getNetworkActive());
}


