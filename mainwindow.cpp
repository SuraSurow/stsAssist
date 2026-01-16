#include "mainwindow.h"



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
    return QDir::current().absoluteFilePath("scripts/scraper.py");
}

QString MainWindow::csvPath() const
{
    return QDir::current().absoluteFilePath("data/sts_premier_league.csv");
}

QString MainWindow::couponsDir() const
{
    return QDir::current().absoluteFilePath("coupons");
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

QString MainWindow::loadApiKey() const
{
    QFile f(apiKeyPath());
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


QString MainWindow::buildPrompt(const QString& csvText) const
{
    const int n = spMatches->value();
    const QString risk = riskLabel();
    const double budget = spBudget->value();

    return QString(
               "Jesteś analitykiem typów bukmacherskich.\n"
               "Masz dane meczowe w CSV (poniżej).\n\n"
               "PARAMETRY:\n"
               "- Liczba meczów na kuponie: %1\n"
               "- Poziom ryzyka: %2 ( Możliwe są w skali od 1 do 5 Pewniak , Bezpieczne , Normalne , Ryzykowne , Niemożliwe )\n"
               "- Budżet całkowity: %3 PLN\n\n"
               "- 1: Kurs Bukmachera na wygraną 1 drużyny\n"
               "- X: Kurs Bukmachera na remis \n"
               "- 2: Kurs Bukmachera na wygraną 2 drużyny\n"
               "WYMAGANIA:\n"
               "- Wybierz dokładnie %1 meczów z listy.\n"
               "- Podaj dla każdego: Mecz / Typ / Stawka / Szansa(%%) / Przewidywany zysk / Uzasadnienie (1-2 zdania).\n"
               "- Stawki (StakePLN) mają się sumować do %3.\n"
               "- ExpectedProfitPLN ma być liczbą (zysk netto w PLN).\n"
               "- Bez wstępu i bez zakończenia.\n"
               "- Swoją predykcje powinienieś oprzeć na uprzednim przeanalizowaniu "
               "internetu i danych Premier League,historii Premier League , obecnego stanu drużyn , ceny składu , dostępności piłkarzy\n"

               "NA KOŃCU ZAWSZE ZWRÓĆ CZYSTY BLOK CSV (bez komentarzy, bez pustych linii):\n"
               "Match,WhichTeamToBet,StakePLN,ConfidencePercent,ExpectedProfitPLN\n\n"
               "DANE CSV:\n"
               "%4\n"
               )
        .arg(n)
        .arg(risk)
        .arg(QString::number(budget, 'f', 2))
        .arg(csvText);
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

    const QString key = loadApiKey();
    if (key.isEmpty()) {
        outputEdit->setPlainText("Brak klucza API. Sprawdź APIKEY.txt ");
        return;
    }

    const QString csvText = readCsvAllLines(csvPath());
    if (csvText.isEmpty()) {
        outputEdit->setPlainText("CSV jest puste lub nie można odczytać.");
        return;
    }

    const QString prompt = buildPrompt(csvText);
    callOpenAI(prompt);
}

void MainWindow::callOpenAI(const QString& prompt)
{
    setUiBusy(true);
    setRunStatus("calling OpenAI...");
    outputEdit->setPlainText("Wysyłam do OpenAI...");

    const QString key = loadApiKey();
    if (key.isEmpty()) {
        setUiBusy(false);
        setRunStatus("missing API key");
        outputEdit->setPlainText("Brak klucza API.");
        return;
    }

    QUrl url("https://api.openai.com/v1/responses");
    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", ("Bearer " + key).toUtf8());

    const QString model = "gpt-5.1";

    QJsonObject body;
    body["model"] = model;

    QJsonArray input;
    QJsonObject msg;
    msg["role"] = "user";
    msg["content"] = prompt;
    input.append(msg);
    body["input"] = input;

    QNetworkReply* reply = net.post(req, QJsonDocument(body).toJson());

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray data = reply->readAll();
        const int http = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
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
            outputEdit->setPlainText("JSON parse error: " + pe.errorString() + "\n\n" + QString::fromUtf8(data));
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
            outputEdit->setPlainText("Brak tekstu w odpowiedzi.\n\n" + QString::fromUtf8(data));
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
                appendLog("EMAIL: brak adresu !!! dodaj poprawny adres");
            }
        }
    });
}


void MainWindow::saveCouponFiles(const QString& replyText) const
{
    ensureDir(couponsDir());

    const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    const QString base = QDir(couponsDir()).absoluteFilePath("coupon_" + ts);

    {
        QFile f(base + ".txt");
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&f);
            out << replyText << "\n";
        }
    }

    {
        QStringList lines = replyText.split('\n');
        QStringList csvLines;

        const QString header1 = "Match,WhichTeamToBet,StakePLN,ConfidencePercent,ExpectedProfitPLN";
        const QString header2 = "Match,WhichTeamToBet,StakePLN,ConfidencePercent,ExpectedProfit";

        int start = -1;
        for (int i = 0; i < lines.size(); ++i) {
            const QString t = lines[i].trimmed();
            if (t == header1 || t == header2) {
                start = i;
                csvLines << header1;
                break;
            }
        }

        if (start != -1) {
            for (int i = start + 1; i < lines.size(); ++i) {
                const QString L = lines[i].trimmed();
                if (!L.isEmpty())
                    csvLines << L;
            }
        }

        if (!csvLines.isEmpty()) {
            const int maxLines = spMatches->value() + 1;
            if (csvLines.size() > maxLines)
                csvLines = csvLines.mid(0, maxLines);

            QFile f(base + ".csv");
            if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&f);
                for (const QString& L : csvLines)
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


void MainWindow::sendEmailNotification(const QString& to, const QString& body)
{
    // U Ciebie: login to stsassist.bot@gmail.com
    const QString host = "smtp.gmail.com";
    const quint16 port = 587;

    const QString user = "stsassist.bot@gmail.com";
    const QString appPass = loadEmailPass();
    const QString from = user;

    const QString subject = "stsAssist - kupon";

    if (appPass.isEmpty()) {
        appendLog("SMTP: brak APP PASSWORD, uzupelnij EMAILPASS.txt ");
        return;
    }

    smtp->sendMail(host, port, user, appPass, from, to, subject, body);
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



