#include "mainwindow.h"


static QJsonObject loadAppConfig()
{
    QFile f(QDir::current().absoluteFilePath("config/app.json"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    return QJsonDocument::fromJson(f.readAll()).object();
}

static QString cfgPath(const QJsonObject& cfg, const QString& key)
{
    return QDir::current().absoluteFilePath(
        cfg["paths"].toObject()[key].toString()
        );
}



static QString ensureDir(const QString& path)
{
    QDir d(path);
    if (!d.exists()) d.mkpath(".");
    return d.absolutePath();
}


static QDateTime addByUnit(const QDateTime& base, int value, const QString& unitText)
{
    if (unitText == "Dni") return base.addDays(value);
    return base.addSecs(value * 60);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("stsAssist");
    resize(1100, 720);

    auto* central = new QWidget(this);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(10);

    auto* statusRow = new QHBoxLayout();
    lblCsvStatus = new QLabel("CSV: (brak)", central);
    lblRunStatus = new QLabel("Status: idle", central);
    lblCsvStatus->setMinimumWidth(420);
    statusRow->addWidget(lblCsvStatus);
    statusRow->addWidget(lblRunStatus, 1);
    root->addLayout(statusRow);

    auto* gbControls = new QGroupBox("Sterowanie", central);
    auto* controlsGrid = new QGridLayout(gbControls);
    controlsGrid->setHorizontalSpacing(12);
    controlsGrid->setVerticalSpacing(8);

    btnRefresh = new QPushButton("Refresh data (scraper)", gbControls);
    btnGenerate = new QPushButton("Generate coupon (OpenAI)", gbControls);

    spMatches = new QSpinBox(gbControls);
    spMatches->setRange(1, 20);
    spMatches->setValue(2);

    cbRisk = new QComboBox(gbControls);
    cbRisk->addItems({"Pewniak", "Bezpieczne", "Normalne", "Ryzykowne", "Niemożliwe"});
    cbRisk->setCurrentText("Normalne");

    spBudget = new QDoubleSpinBox(gbControls);
    spBudget->setRange(1.0, 100000.0);
    spBudget->setValue(100.0);
    spBudget->setSuffix(" zł");
    spBudget->setDecimals(2);

    chkAuto = new QCheckBox("Auto-generuj", gbControls);

    spAutoMinutes = new QSpinBox(gbControls);
    spAutoMinutes->setRange(1, 365);
    spAutoMinutes->setValue(30);

    cbAutoUnit = new QComboBox(gbControls);
    cbAutoUnit->addItems({"Minuty", "Dni"});
    cbAutoUnit->setCurrentText("Minuty");


    chkChainScraper = new QCheckBox("Sprzęż auto: refresh → coupon", gbControls);
    chkChainScraper->setChecked(false);


    chkSendEmail = new QCheckBox("Wyślij email z kuponem", gbControls);
    chkSendEmail->setChecked(false);

    edEmail = new QLineEdit(gbControls);
    edEmail->setPlaceholderText("adres@email.com");

    controlsGrid->addWidget(btnRefresh, 0, 0);
    controlsGrid->addWidget(btnGenerate, 0, 1);

    controlsGrid->addWidget(new QLabel("Mecze:", gbControls), 1, 0);
    controlsGrid->addWidget(spMatches, 1, 1);

    controlsGrid->addWidget(new QLabel("Ryzyko:", gbControls), 2, 0);
    controlsGrid->addWidget(cbRisk, 2, 1);

    controlsGrid->addWidget(new QLabel("Budżet:", gbControls), 3, 0);
    controlsGrid->addWidget(spBudget, 3, 1);

    auto* autoRow = new QHBoxLayout();
    autoRow->addWidget(chkAuto);
    autoRow->addSpacing(10);
    autoRow->addWidget(new QLabel("co:", gbControls));
    autoRow->addWidget(spAutoMinutes);
    autoRow->addWidget(cbAutoUnit);
    autoRow->addStretch(1);
    controlsGrid->addLayout(autoRow, 4, 0, 1, 2);

    controlsGrid->addWidget(chkChainScraper, 5, 0, 1, 2);

    auto* emailRow = new QHBoxLayout();
    emailRow->addWidget(chkSendEmail);
    emailRow->addSpacing(10);
    emailRow->addWidget(new QLabel("Do:", gbControls));
    emailRow->addWidget(edEmail, 1);
    controlsGrid->addLayout(emailRow, 6, 0, 1, 2);

    root->addWidget(gbControls);

    auto* splitter = new QSplitter(Qt::Horizontal, central);

    auto* logBox = new QGroupBox("Log scrapera / statusy", splitter);
    auto* logLay = new QVBoxLayout(logBox);
    logEdit = new QPlainTextEdit(logBox);
    logEdit->setReadOnly(true);
    logEdit->setPlaceholderText("Tutaj pojawi się log z Pythona + komunikaty aplikacji...");
    logLay->addWidget(logEdit);

    auto* outBox = new QGroupBox("Wynik kuponu (OpenAI)", splitter);
    auto* outLay = new QVBoxLayout(outBox);
    outputEdit = new QTextEdit(outBox);
    outputEdit->setReadOnly(true);
    outputEdit->setPlaceholderText("Tu pojawi się odpowiedź z OpenAI. Zostanie też zapisana do pliku w folderze coupons/.");
    outLay->addWidget(outputEdit);

    splitter->addWidget(logBox);
    splitter->addWidget(outBox);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);

    root->addWidget(splitter, 1);

    setCentralWidget(central);

    connect(btnRefresh, &QPushButton::clicked, this, &MainWindow::onRefreshData);
    connect(btnGenerate, &QPushButton::clicked, this, &MainWindow::onGenerateCoupon);
    connect(chkAuto, &QCheckBox::toggled, this, &MainWindow::onAutoToggled);

    connect(spAutoMinutes, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int){
        if (chkAuto->isChecked()) {
            appendLog("AUTO: zmiana interwału -> przeliczam harmonogram");
            nextAutoRun = QDateTime::currentDateTime();
        }
    });
    connect(cbAutoUnit, &QComboBox::currentTextChanged, this, [this](const QString&){
        if (chkAuto->isChecked()) {
            appendLog("AUTO: zmiana jednostki -> przeliczam harmonogram");
            nextAutoRun = QDateTime::currentDateTime();
        }
    });

    // QProcess streaming
    connect(&scraper, &QProcess::readyReadStandardOutput, this, [this]() {
        appendLog(QString::fromUtf8(scraper.readAllStandardOutput()));
    });
    connect(&scraper, &QProcess::readyReadStandardError, this, [this]() {
        appendLog(QString::fromUtf8(scraper.readAllStandardError()));
    });
    connect(&scraper, &QProcess::started, this, [this]() {
        setRunStatus("scraper running...");
        setUiBusy(true);
    });
    connect(&scraper, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus st) {
        Q_UNUSED(st);
        setUiBusy(false);
        if (exitCode == 0) {
            setCsvStatus("CSV: OK -> " + csvPath());
            setRunStatus("scraper OK");
            appendLog("Scraper finished OK.");
            if (pendingAutoCoupon) {
                pendingAutoCoupon = false;
                appendLog("AUTO: refresh OK -> generuję kupon");
                onGenerateCoupon();
            }

        } else {
            setCsvStatus("CSV: ERROR (exitCode=" + QString::number(exitCode) + ")");
            setRunStatus("scraper FAILED");
            appendLog("Scraper FAILED.");

            if (pendingAutoCoupon) {
                pendingAutoCoupon = false;
                appendLog("AUTO: refresh FAILED -> pomijam kupon");
            }
        }
    });

    // auto timer
    autoTimer.setSingleShot(false);
    connect(&autoTimer, &QTimer::timeout, this, &MainWindow::onAutoTick);

    // ensure dirs
    ensureDir(QFileInfo(csvPath()).absolutePath());
    ensureDir(couponsDir());

    // initial status
    if (QFile::exists(csvPath())) {
        setCsvStatus("CSV: OK -> " + csvPath());
    } else {
        setCsvStatus("CSV: (brak) -> " + csvPath());
    }
    setRunStatus("idle");

    nextAutoRun = QDateTime();
    smtp = new SmtpClient(this);
    connect(smtp, &SmtpClient::log, this, [this](const QString& s){ appendLog(s); });
    connect(smtp, &SmtpClient::finished, this, [this](bool ok, const QString& info){
        appendLog(QString("SMTP finished: %1 | %2").arg(ok ? "OK" : "FAIL").arg(info));
    });

}

QString MainWindow::apiKeyPath() const
{
    return QDir::current().absoluteFilePath("APIKEY.txt");
}

QString MainWindow::scraperPath() const
{
    static QJsonObject cfg = loadAppConfig();
    return cfgPath(cfg, "scraper_script");
}

QString MainWindow::csvPath() const
{
    static QJsonObject cfg = loadAppConfig();
    return cfgPath(cfg, "csv");
}


QString MainWindow::couponsDir() const
{
    static QJsonObject cfg = loadAppConfig();
    return cfgPath(cfg, "coupons_dir");
}

void MainWindow::appendLog(const QString& s)
{
    const QString trimmed = s.trimmed();
    if (!trimmed.isEmpty())
        logEdit->appendPlainText(trimmed);
}

void MainWindow::setCsvStatus(const QString& s)
{
    lblCsvStatus->setText(s);
}

void MainWindow::setRunStatus(const QString& s)
{
    lblRunStatus->setText("Status: " + s);
}

void MainWindow::setUiBusy(bool busy)
{
    btnRefresh->setEnabled(!busy);
    btnGenerate->setEnabled(!busy);
    chkAuto->setEnabled(!busy);
    spAutoMinutes->setEnabled(!busy);
    cbAutoUnit->setEnabled(!busy);
    chkChainScraper->setEnabled(!busy);
    chkSendEmail->setEnabled(!busy);
    edEmail->setEnabled(!busy);
    spMatches->setEnabled(!busy);
    cbRisk->setEnabled(!busy);
    spBudget->setEnabled(!busy);
}

QString MainWindow::loadOpenAiApiKey() const
{
    QJsonObject cfg = loadAppConfig();
    QJsonObject openai = cfg["openai"].toObject();

    const QString relPath = openai["api_key_file"].toString();
    if (relPath.isEmpty())
        return {};

    QFile f(QDir::current().absoluteFilePath(relPath));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QTextStream in(&f);
    return in.readLine().trimmed();
}



QString MainWindow::readCsvAllLines(const QString& path) const
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QTextStream in(&f);
    QString out;
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (!line.trimmed().isEmpty())
            out += line + "\n";
    }
    return out.trimmed();
}

QString MainWindow::riskLabel() const
{
    return cbRisk->currentText();
}


QString MainWindow::buildPromptFromTemplate(const QString& csvText) const
{
    QJsonObject cfg = loadAppConfig();

    QFile f(cfgPath(cfg, "prompt_file"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QString tpl = QTextStream(&f).readAll();

    return tpl
        .replace("{{MATCHES}}", QString::number(spMatches->value()))
        .replace("{{RISK}}", cbRisk->currentText())
        .replace("{{BUDGET}}", QString::number(spBudget->value(), 'f', 2))
        .replace("{{CSV_DATA}}", csvText);
}

void MainWindow::onRefreshData()
{
    if (scraper.state() != QProcess::NotRunning) {
        appendLog("Scraper already running.");
        return;
    }

    if (!QFile::exists(scraperPath())) {
        appendLog("ERROR: scraper.py not found: " + scraperPath());
        setRunStatus("scraper.py missing");
        return;
    }

    ensureDir(QFileInfo(csvPath()).absolutePath());

    appendLog("Running scraper...");
    setRunStatus("starting scraper...");

    scraper.start("python3", { scraperPath(), csvPath() });
}

void MainWindow::onGenerateCoupon()
{
    if (!QFile::exists(csvPath())) {
        outputEdit->setPlainText("Brak CSV. Kliknij najpierw Refresh data");
        return;
    }

    const QString key = loadOpenAiApiKey();
    if (key.isEmpty()) {
        outputEdit->setPlainText("Brak klucza API. Sprawdź APIKEY.txt ");
        return;
    }

    const QString csvText = readCsvAllLines(csvPath());
    if (csvText.isEmpty()) {
        outputEdit->setPlainText("CSV jest puste lub nie można odczytać.");
        return;
    }

    const QString prompt = buildPromptFromTemplate(csvText);
    callOpenAI(prompt);
}

void MainWindow::callOpenAI(const QString& prompt)
{
    setUiBusy(true);
    setRunStatus("calling OpenAI...");
    outputEdit->setPlainText("Wysyłam do OpenAI...");

    // ===== API KEY =====
    const QString key = loadOpenAiApiKey();
    if (key.isEmpty()) {
        setUiBusy(false);
        setRunStatus("missing API key");
        outputEdit->setPlainText("Brak klucza API (sprawdź config/openai.key).");
        return;
    }

    QJsonObject cfg = loadAppConfig();
    QJsonObject openai = cfg["openai"].toObject();

    const QString model = openai["model"].toString("gpt-5.1");
    const int maxTokens = openai["max_tokens"].toInt(1200);

    QUrl url("https://api.openai.com/v1/responses");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", ("Bearer " + key).toUtf8());

    QJsonObject body;
    body["model"] = model;
    body["max_output_tokens"] = maxTokens;

    QJsonArray input;
    QJsonObject msg;
    msg["role"] = "user";
    msg["content"] = prompt;
    input.append(msg);

    body["input"] = input;

    QNetworkReply* reply = net.post(req, QJsonDocument(body).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {

        const QByteArray data = reply->readAll();
        const int http =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QString err = reply->errorString();
        reply->deleteLater();

        setUiBusy(false);

        if (http < 200 || http >= 300) {
            setRunStatus("OpenAI FAILED");
            outputEdit->setPlainText(
                "Błąd HTTP: " + QString::number(http) + "\n" +
                "Qt error: " + err + "\n\n" +
                "Response:\n" + QString::fromUtf8(data)
                );
            return;
        }

        QJsonParseError pe;
        QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
        if (pe.error != QJsonParseError::NoError) {
            setRunStatus("OpenAI JSON parse error");
            outputEdit->setPlainText(
                "JSON parse error: " + pe.errorString() + "\n\n" +
                QString::fromUtf8(data)
                );
            return;
        }

        QString text;
        const QJsonObject root = doc.object();
        const QJsonArray output = root.value("output").toArray();

        for (const QJsonValue& v : output) {
            const QJsonObject o = v.toObject();
            const QJsonArray content = o.value("content").toArray();
            for (const QJsonValue& c : content) {
                const QJsonObject co = c.toObject();
                if (co.value("type").toString() == "output_text") {
                    text += co.value("text").toString();
                }
            }
        }

        text = text.trimmed();
        if (text.isEmpty()) {
            setRunStatus("OpenAI empty reply");
            outputEdit->setPlainText(
                "Brak tekstu w odpowiedzi.\n\n" +
                QString::fromUtf8(data)
                );
            return;
        }

        setRunStatus("OpenAI OK");
        outputEdit->setPlainText(text);

        saveCouponFiles(text);

        if (chkSendEmail && chkSendEmail->isChecked()) {
            const QString to = edEmail ? edEmail->text().trimmed() : QString();
            if (!to.isEmpty()) {
                sendEmailNotification(to, text);
            } else {
                appendLog("EMAIL: brak adresu — podaj poprawny adres");
            }
        }
    });
}



void MainWindow::saveCouponFiles(const QString& replyText) const
{
    ensureDir(couponsDir());

    const QString ts = QDateTime::currentDateTime()
                           .toString("yyyy-MM-dd_HH-mm-ss");
    const QString base =
        QDir(couponsDir()).absoluteFilePath("coupon_" + ts);

    // ===== TXT =====
    {
        QFile f(base + ".txt");
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&f);
            out << replyText << "\n";
        }
    }

    // ===== CSV =====
    {
        QStringList lines = replyText.split('\n');
        QStringList dataLines;

        const QString header =
            "Match,WhichTeamToBet,StakePLN,ConfidencePercent,ExpectedProfitPLN";

        for (const QString& line : lines) {
            const QString t = line.trimmed();
            if (t.isEmpty())
                continue;

            // pomijamy nagłówek, gdziekolwiek by był
            if (t.startsWith("Match,") &&
                t.contains("StakePLN") &&
                t.contains("ConfidencePercent")) {
                continue;
            }

            // heurystyka: linia danych CSV
            if (t.count(',') >= 4) {
                dataLines << t;
            }
        }

        if (!dataLines.isEmpty()) {

            // limit do liczby meczów
            const int maxData = spMatches->value();
            if (dataLines.size() > maxData)
                dataLines = dataLines.mid(0, maxData);

            QFile f(base + ".csv");
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&f);

                // NAGŁÓWEK ZAWSZE PIERWSZY
                out << header << "\n";

                for (const QString& L : dataLines)
                    out << L << "\n";
            }
        }
    }
}



void MainWindow::onAutoToggled(bool enabled)
{
    if (enabled) {

        nextAutoRun = QDateTime::currentDateTime();
        autoTimer.start(30 * 1000);

        appendLog(QString("AUTO ON (%1 %2)")
                      .arg(spAutoMinutes->value())
                      .arg(cbAutoUnit->currentText()));

        onAutoTick();
    } else {
        autoTimer.stop();
        appendLog("AUTO OFF");
        setRunStatus("idle");
        nextAutoRun = QDateTime();
        pendingAutoCoupon = false;
    }
}

void MainWindow::onAutoTick()
{
    if (!chkAuto->isChecked())
        return;

    if (!nextAutoRun.isValid())
        nextAutoRun = QDateTime::currentDateTime();

    const QDateTime now = QDateTime::currentDateTime();
    if (now < nextAutoRun)
        return;

    nextAutoRun = addByUnit(now, spAutoMinutes->value(), cbAutoUnit->currentText());

    if (chkChainScraper->isChecked()) {
        pendingAutoCoupon = true;
        appendLog("AUTO: start refresh (chain ON)");
        onRefreshData();
    } else {
        appendLog("AUTO: generuję kupon (chain OFF)");
        onGenerateCoupon();
    }
}


void MainWindow::sendEmailNotification(const QString& toOverride, const QString& body)
{
    QJsonObject cfg = loadAppConfig();

    // --- EMAIL CONFIG ---
    QJsonObject emailCfg = cfg["email"].toObject();

    const QString host = emailCfg["smtp_host"].toString();
    const quint16 port = static_cast<quint16>(emailCfg["smtp_port"].toInt());
    const bool useTls = emailCfg["use_tls"].toBool(true);

    const QString user = emailCfg["from"].toString();
    const QString from = user;

    // jeśli pole "to" w UI puste → bierz z configa
    QString to = toOverride.trimmed();
    if (to.isEmpty()) {
        to = emailCfg["to"].toString();
    }

    const QString subject = "stsAssist - kupon";
    const QString appPass = loadEmailPass();

    // --- WALIDACJA ---
    if (host.isEmpty() || port == 0) {
        appendLog("SMTP: brak hosta lub portu w app_config.json");
        return;
    }

    if (user.isEmpty()) {
        appendLog("SMTP: brak pola email.from w app_config.json");
        return;
    }

    if (to.isEmpty()) {
        appendLog("SMTP: brak adresu odbiorcy");
        return;
    }

    if (appPass.isEmpty()) {
        appendLog("SMTP: brak APP PASSWORD (EMAILPASS.txt)");
        return;
    }

    appendLog(QString("SMTP: wysyłam maila do %1 (%2:%3)")
                  .arg(to)
                  .arg(host)
                  .arg(port));

    smtp->sendMail(
        host,
        port,
        user,
        appPass,
        from,
        to,
        subject,
        body,
        useTls
        );
}


QString MainWindow::emailPassPath() const
{
    return QDir::current().absoluteFilePath("EMAILPASS.txt");
}

QString MainWindow::loadEmailPass() const
{
    QFile f(emailPassPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};

    QTextStream in(&f);
    return in.readLine().trimmed();
}



