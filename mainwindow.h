#ifndef MAINWINDOW_H
#define MAINWINDOW_H


#include "smtpclient.h"

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QProcess>
#include <QTimer>
#include <QDateTime>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSplitter>
#include <QFileInfo>
#include <QProcess>


class QPushButton;
class QPlainTextEdit;
class QTextEdit;
class QSpinBox;
class QDoubleSpinBox;
class QComboBox;
class QCheckBox;
class QLabel;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override = default;
    void sendEmailNotification(const QString& to, const QString& body);


private slots:
    void onRefreshData();
    void onGenerateCoupon();
    void onAutoToggled(bool enabled);
    void onAutoTick();



private:
    SmtpClient* smtp = nullptr;
    QString emailPassPath() const;
    QString loadEmailPass() const;

    // --- UI ---
    QPushButton* btnRefresh = nullptr;
    QPushButton* btnGenerate = nullptr;

    QSpinBox* spMatches = nullptr;
    QComboBox* cbRisk = nullptr;
    QDoubleSpinBox* spBudget = nullptr;

    QCheckBox* chkAuto = nullptr;
    QSpinBox* spAutoMinutes = nullptr;

    QLabel* lblCsvStatus = nullptr;
    QLabel* lblRunStatus = nullptr;

    QPlainTextEdit* logEdit = nullptr;
    QTextEdit* outputEdit = nullptr;

    QComboBox* cbAutoUnit = nullptr;
    QCheckBox* chkChainScraper = nullptr;

    QCheckBox* chkSendEmail = nullptr;
    QLineEdit* edEmail = nullptr;

    QNetworkAccessManager net;
    QProcess scraper;
    QTimer autoTimer;

    QString apiKeyPath() const;
    QString scraperPath() const;
    QString csvPath() const;
    QString couponsDir() const;

    void appendLog(const QString& s);
    void setCsvStatus(const QString& s);
    void setRunStatus(const QString& s);

    void setUiBusy(bool busy);

    QString loadOpenAiApiKey() const;
    QString readCsvAllLines(const QString& path) const;

    QString buildPromptFromTemplate(const QString& csvText) const;

    void callOpenAI(const QString& prompt);
    void saveCouponFiles(const QString& replyText) const;

    QString riskLabel() const;
    bool pendingAutoCoupon = false;
    QDateTime nextAutoRun;

};

#endif // MAINWINDOW_H
