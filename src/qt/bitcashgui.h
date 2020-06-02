// Copyright (c) 2011-2017 The Bitcash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCASH_QT_BITCASHGUI_H
#define BITCASH_QT_BITCASHGUI_H

#if defined(HAVE_CONFIG_H)
#include <config/bitcash-config.h>
#endif

#include <amount.h>

#include <QLabel>
#include <QMainWindow>
#include <QMap>
#include <QMenu>
#include <QPoint>
#include <QSystemTrayIcon>
#include <qt/walletmodel.h>

#include <memory>

#include <QDialog>
#include <QMessageBox>
#include <QString>
#include <QTimer>
#include <QQmlContext>
#include <QNetworkReply>
#include <QProcess>

class ClientModel;
class NetworkStyle;
class Notificator;
class OptionsModel;
class PlatformStyle;
class RPCConsole;
class SendCoinsRecipient;
class WalletFrame;
class WalletModel;
class HelpMessageDialog;
class ModalOverlay;
extern QObject *qmlrootitem;
extern QQmlContext *qmlrootctxt;

namespace interfaces {
class Handler;
class Node;
}

QT_BEGIN_NAMESPACE
class QAction;
class QComboBox;
class QProgressBar;
class QProgressDialog;
QT_END_NAMESPACE

/**
  Bitcash GUI main class. This class represents the main window of the Bitcash UI. It communicates with both the client and
  wallet models to give the user an up-to-date view of the current core state.
*/
class BitcashGUI : public QMainWindow
{
    Q_OBJECT

public:
    static const std::string DEFAULT_UIPLATFORM;

    explicit BitcashGUI(interfaces::Node& node, const PlatformStyle *platformStyle, const NetworkStyle *networkStyle, QWidget *parent = 0);
    ~BitcashGUI();

    /** Set the client model.
        The client model represents the part of the core that communicates with the P2P network, and is wallet-agnostic.
    */
    void setClientModel(ClientModel *clientModel);

#ifdef ENABLE_WALLET
    /** Set the wallet model.
        The wallet model represents a bitcash wallet, and offers access to the list of transactions, address book and sending
        functionality.
    */
    WalletModel* getCurrentWalletModel();
    bool addWallet(WalletModel *walletModel);
    void removeAllWallets();
#endif // ENABLE_WALLET
    bool enableWallet;
    QNetworkAccessManager *manager;
    QNetworkAccessManager *managerinsta;
    QNetworkAccessManager *managertwitch;
    QNetworkAccessManager *managercheckversion;
    QNetworkAccessManager *managernews;
    QNetworkAccessManager *managerinstaswap;

protected:
    bool nativeEvent(const QByteArray& eventType, void* message, long* result);
    void changeEvent(QEvent *e);
    void closeEvent(QCloseEvent *event);
    void showEvent(QShowEvent *event);
    void dragEnterEvent(QDragEnterEvent *event);
    void dropEvent(QDropEvent *event);
    bool eventFilter(QObject *object, QEvent *event);

private:
    interfaces::Node& m_node;
    std::unique_ptr<interfaces::Handler> m_handler_message_box;
    std::unique_ptr<interfaces::Handler> m_handler_question;
    ClientModel *clientModel;
    WalletFrame *walletFrame;
    QProcess miningprocess;

    QLabel *labelWalletEncryptionIcon;
    QLabel *labelWalletHDStatusIcon;
    QLabel *connectionsControl;
    QLabel *labelBlocksIcon;
    QLabel *progressBarLabel;
    QProgressBar *progressBar;
    QProgressDialog *progressDialog;
    int currentbilldenomination;
    int currentbillcurrency;
    std::string currentlinkforbill;
    QImage currentfaceforbill;
    bool usecpumininginsteadofgpumining = false;

    QMenuBar *appMenuBar;
    QToolBar *appToolBar;
    QAction *paperWalletAction;
    QAction *printWalletAction;
    QAction *importKeyAction;
    QAction *repairSyncAction;
    QAction *resendWalletTxesAction;
    QAction *rescanAction;
    QAction *rescanAction50;
    QAction *overviewAction;
    QAction *miningAction;
    QAction *nicknameAction;
    QAction *linksAction;
    QAction *historyAction;
    QAction *quitAction;
    QAction *sendCoinsAction;
    QAction *sendCoinsMenuAction;
    QAction *usedSendingAddressesAction;
    QAction *usedReceivingAddressesAction;
    QAction *signMessageAction;
    QAction *verifyMessageAction;
    QAction *aboutAction;
    QAction *receiveCoinsAction;
    QAction *receiveCoinsMenuAction;
    QAction *optionsAction;
    QAction *toggleHideAction;
    QAction *encryptWalletAction;
    QAction *backupWalletAction;
    QAction *importWalletAction;
    QAction *changePassphraseAction;
    QAction *aboutQtAction;
    QAction *openRPCConsoleAction;
    QAction *openAction;
    QAction *showHelpMessageAction;
    QAction *websiteAction;
    QAction *explorerAction;
    QAction *supplyAction;

    QLabel *m_wallet_selector_label;
    QComboBox *m_wallet_selector;

    QSystemTrayIcon *trayIcon;
    QMenu *trayIconMenu;
    Notificator *notificator;
    RPCConsole *rpcConsole;
    HelpMessageDialog *helpMessageDialog;
    ModalOverlay *modalOverlay;

    /** Keep track of previous number of blocks, to detect progress */
    int prevBlocks;
    int spinnerFrame;

    bool hasbeenmining = false;
    QString lastamountforinstaswap;

    const PlatformStyle *platformStyle;

    void processSendCoinsReturn(WalletModel * const model, const WalletModel::SendCoinsReturn &sendCoinsReturn, const QString &msgArg = QString());

    /** Create the main UI actions. */
    void createActions();
    /** Create the menu bar and sub-menus. */
    void createMenuBar();
    /** Create the toolbars */
    void createToolBars();
    /** Create system tray icon and notification */
    void createTrayIcon(const NetworkStyle *networkStyle);
    /** Create system tray menu (or setup the dock menu) */
    void createTrayIconMenu();

    void HideProgressBarLabelText();
    void HideProgressBarLabelText2();
    void HideProgressBar();
    void SetProgressBarPercent(QVariant s);
    void SetProgressBarLabelText(QVariant s);
    void SetProgressBarLabelText2(QVariant s);

    /** Enable or disable all wallet-related actions */
    void setWalletActionsEnabled(bool enabled);

    /** Connect core signals to GUI client */
    void subscribeToCoreSignals();
    /** Disconnect core signals from GUI client */
    void unsubscribeFromCoreSignals();

    /** Update UI with latest network info from model. */
    void updateNetworkState();

    void updateHeadersSyncProgressLabel();

Q_SIGNALS:
    /** Signal raised when a URI was entered or dragged to the GUI */
    void receivedURI(const QString &uri);

public Q_SLOTS:
    void printMainWalletClicked();
    void openTradingWindowBtnClicked();
    void paperWalletClicked();
    void repairSyncIssuesClicked();
    void importKeyClicked();
    bool UserKnowsPassword();
    void importKeyBtnClicked(QString keystr, bool fromwebwallet);
    void resendWalletTxesClicked();
    void rescanClicked(); 
    QImage GetbillbackImage();
    QImage GetbillfrontImage();
    void rescan50Clicked();
    void welcomebonus();
    void termsBtnClicked();
    void InstaSwapCheckAmountClicked(bool buybitcash, double amount);
    void InstaSwapSendBtnClicked(bool buybitcash, double amount, QString bitcoinaddress);
    void printTwitterBillClicked(QString strlink, int denomination, int curr);
    void RegisterNickBtnClicked(const QString &nickname, const QString &address);
    void SendLinksBtnClicked(const QString &description, double amount, int curr);
    void createPaymentClicked(const QString &recipient, const QString &description, double amount, int day, int month, bool senddollar);
    void createOrderBtnClicked(double amounttosend, double targetPrice, bool senddollar, bool whenpricegoesabove);
    void SendBtnClicked(const QString &destination, const QString &label, const QString &description, double amount, bool substractfee);
    void SendBtnDoClicked(const QString &destination, const QString &label, const QString &description, double amount, bool substractfee);
    void SendBtnGoClicked(const QString &destination, const QString &label, const QString &description, double amount, bool substractfee);
    bool SendBtnClickedIntern(const QString &destination, const QString &label, const QString &description, double amount, bool substractfee, bool dialog, unsigned char currency, bool nomessages);
    void SendToTwitterBtnClicked(const QString &destination, const QString &description, double amount, int currency);
    void SendToInstaBtnClicked(const QString &destination, const QString &description, double amount, int currency);
    void SendToTwitchBtnClicked(const QString &destination, const QString &description, double amount, int currency);
    void SendConfirmedToTwitterBtnClicked(const QString &destination, const QString &description, double amount, int currency);
    void SendConfirmedToInstaBtnClicked(const QString &destination, const QString &description, double amount, int currency);
    void SendConfirmedToTwitchBtnClicked(const QString &destination, const QString &description, double amount, int currency);
    void sendtoTwitterClicked(QString twitteruser, QString coinlink);
    void sendtoInstaClicked(QString instauser, QString coinlink);
    void generateBillClicked(int denomination);
    void generateBillBitCashClicked(int denomination);
    void printfrontbillClicked();
    void printbackbillClicked();
    void sendcpumininfinfosignalClicked();
    void sendusecpusignalClicked(bool usecpumining);
    void openLinkClicked(QString link) ;
    void sendtoTwitchClicked(QString twitchuser, QString coinlink);
    void replyFinished(QNetworkReply *reply);
    void replyFinishedInsta(QNetworkReply *reply);
    void replyFinishedTwitch(QNetworkReply *reply);
    void replyFinishedcheckversion(QNetworkReply *reply);
    void replyFinishednews(QNetworkReply *reply);
    void replyFinishedInstaSwap(QNetworkReply *reply);
    void ClaimLinksBtnClicked(const QString &strlinkqt);
    void recurringpayments();
    void StartMiningBtnClicked(); 
    void undoPaymentsRemovalClicked();
    void createnewpaperwallet();
    void savebillSignalClicked();
    void savebillbackSignalClicked();
    void openpictureClicked();
    void DeletePaymentsClicked(const QString &strlink);
    void DeleteOrdersClicked(const QString &strlink);
    void PrintPaperWalletClicked();
    void printStatementsBtnClicked(int month, int year, int currency);
    void HelpBtnClicked();
    void websiteClicked();
    void explorerClicked();
    void supplyClicked();
    void SendLinksToModel();
    void SendPaymentsToModel();
    void SendOrdersToModel();
    void DeleteLinksClicked(const QString &strlink);
    void undoLinksRemovalClicked();
    void updateminingstats();
    void updateprice();
    void ExecuteOrders(double price, double price2);
    void StopMiningBtnClicked(); 
    /** Set number of connections shown in the UI */
    void setNumConnections(int count);
    /** Set network state shown in the UI */
    void setNetworkActive(bool networkActive);
    /** Set number of blocks and last block date shown in the UI */
    void setNumBlocks(int count, const QDateTime& blockDate, double nVerificationProgress, bool headers);

    /** Notify the user of an event from the core network or transaction handling code.
       @param[in] title     the message box / notification title
       @param[in] message   the displayed text
       @param[in] style     modality and style definitions (icon and used buttons - buttons only for message boxes)
                            @see CClientUIInterface::MessageBoxFlags
       @param[in] ret       pointer to a bool that will be modified to whether Ok was clicked (modal only)
    */
    void message(const QString &title, const QString &message, unsigned int style, bool *ret = nullptr);

#ifdef ENABLE_WALLET
    bool setCurrentWallet(const QString& name);
    /** Set the UI status indicators based on the currently selected wallet.
    */
    void updateWalletStatus();

private:
    /** Set the encryption status as shown in the UI.
       @param[in] status            current encryption status
       @see WalletModel::EncryptionStatus
    */
    void setEncryptionStatus(int status);

    /** Set the hd-enabled status as shown in the UI.
     @param[in] status            current hd enabled status
     @see WalletModel::EncryptionStatus
     */
    void setHDStatus(int hdEnabled);

public Q_SLOTS:
    bool handlePaymentRequest(const SendCoinsRecipient& recipient);

    /** Show incoming transaction notification for new transactions. */
    void incomingTransaction(const QString& date, int unit, const CAmount& amount, const QString& type, const QString& address, const QString& label, const QString& walletName);
#endif // ENABLE_WALLET

private Q_SLOTS:
#ifdef ENABLE_WALLET
    /** Switch to overview (home) page */
    void gotoOverviewPage();
    /** Switch to send/receive via link page */
    void gotoLinksPage();
    /** Switch to history (transactions) page */
    void gotoHistoryPage();
    /** Switch to mining page */
    void gotoMiningPage();
    /** Switch to nickname page */
    void gotoNicknamePage();
    /** Switch to receive coins page */
    void gotoReceiveCoinsPage();
    /** Switch to send coins page */
    void gotoSendCoinsPage(QString addr = "");

    /** Show Sign/Verify Message dialog and switch to sign message tab */
    void gotoSignMessageTab(QString addr = "");
    /** Show Sign/Verify Message dialog and switch to verify message tab */
    void gotoVerifyMessageTab(QString addr = "");

    /** Show open dialog */
    void openClicked();
#endif // ENABLE_WALLET
    /** Show configuration dialog */
    void optionsClicked();
    /** Show about dialog */
    void aboutClicked();
    /** Show debug window */
    void showDebugWindow();
    /** Show debug window and set focus to the console */
    void showDebugWindowActivateConsole();
    /** Show help message dialog */
    void showHelpMessageClicked();
#ifndef Q_OS_MAC
    /** Handle tray icon clicked */
    void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
#endif

    /** Show window if hidden, unminimize when minimized, rise when obscured or show if hidden and fToggleHidden is true */
    void showNormalIfMinimized(bool fToggleHidden = false);
    /** Simply calls showNormalIfMinimized(true) for use in SLOT() macro */
    void toggleHidden();

    /** called by a timer to check if fRequestShutdown has been set **/
    void detectShutdown();

    /** Show progress dialog e.g. for verifychain */
    void showProgress(const QString &title, int nProgress);
    
    /** When hideTrayIcon setting is changed in OptionsModel hide or show the icon accordingly. */
    void setTrayIconVisible(bool);

    /** Toggle networking */
    void toggleNetworkActive();

    void showModalOverlay();
};

#define SEND_CONFIRM_DELAY   3

class SendConfirmationDialog2 : public QMessageBox
{
    Q_OBJECT

public:
    SendConfirmationDialog2(const QString &title, const QString &text, int secDelay = SEND_CONFIRM_DELAY, QWidget *parent = 0);
    int exec();

private Q_SLOTS:
    void countDown();
    void updateYesButton();

private:
    QAbstractButton *yesButton;
    QTimer countDownTimer;
    int secDelay;
};

#endif // BITCASH_QT_BITCASHGUI_H
