#pragma once
#include <QObject>
#include <QSslSocket>

class SmtpClient : public QObject {
    Q_OBJECT
public:
    explicit SmtpClient(QObject* parent = nullptr);

    void sendMail(const QString& host,
                  quint16 port,
                  const QString& username,
                  const QString& appPassword,
                  const QString& from,
                  const QString& to,
                  const QString& subject,
                  const QString& body,
                  bool useTls);

signals:
    void log(const QString& line);
    void finished(bool ok, const QString& info);

private slots:
    void onConnected();
    void onEncrypted();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError);

private:
    enum class State {
        Idle,
        Wait220,
        Ehlo1,
        StartTls,
        WaitTls,
        Ehlo2,
        AuthLogin,
        AuthUser,
        AuthPass,
        MailFrom,
        RcptTo,
        Data,
        SendBody,
        Quit,
        Done
    };

    void sendLine(const QString& s);
    void handleLine(const QString& line);
    bool codeIs(const QString& line, int code) const;
    QString b64(const QString& s) const;

    QSslSocket* sock = nullptr;
    State state = State::Idle;

    QString host;
    quint16 port = 587;

    QString username;
    QString password;
    QString from;
    QString to;
    QString subject;
    QString body;
    bool useTls;

    QString buffer;
};
