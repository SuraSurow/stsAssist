#include "smtpclient.h"
#include <QDateTime>
#include <QRegularExpression>

SmtpClient::SmtpClient(QObject* parent) : QObject(parent)
{
    sock = new QSslSocket(this);

    connect(sock, &QSslSocket::connected, this, &SmtpClient::onConnected);
    connect(sock, &QSslSocket::encrypted, this, &SmtpClient::onEncrypted);
    connect(sock, &QSslSocket::readyRead, this, &SmtpClient::onReadyRead);
    connect(sock, &QSslSocket::errorOccurred, this, &SmtpClient::onError);
}

void SmtpClient::sendMail(const QString& host_,
                          quint16 port_,
                          const QString& username_,
                          const QString& appPassword_,
                          const QString& from_,
                          const QString& to_,
                          const QString& subject_,
                          const QString& bodyText_,
                          bool useTls_)
{
    host = host_;
    port = port_;
    username = username_;
    password = appPassword_;
    from = from_;
    to = to_;
    subject = subject_;
    body = bodyText_;
    useTls = useTls_;

    buffer.clear();
    state = State::Wait220;

    emit log(QString("SMTP connect %1:%2 (TLS=%3)")
                 .arg(host)
                 .arg(port)
                 .arg(useTls ? "ON" : "OFF"));

    // ZAWSZE zwykłe połączenie TCP
    // STARTTLS robimy dopiero po EHLO
    sock->connectToHost(host, port);
}

void SmtpClient::onConnected()
{
    emit log("SMTP connected, waiting for 220...");
}

void SmtpClient::onEncrypted()
{
    emit log("SMTP: TLS encrypted OK");

    // Po STARTTLS MUSIMY wysłać EHLO jeszcze raz
    state = State::Ehlo2;
    sendLine("EHLO localhost");
}

void SmtpClient::onReadyRead()
{
    buffer += QString::fromUtf8(sock->readAll());

    while (true) {
        int idx = buffer.indexOf("\r\n");
        if (idx < 0) break;

        QString line = buffer.left(idx);
        buffer.remove(0, idx + 2);

        if (!line.isEmpty())
            handleLine(line);
    }
}

void SmtpClient::onError(QAbstractSocket::SocketError)
{
    emit finished(false, "Socket error: " + sock->errorString());
    state = State::Done;
}

void SmtpClient::sendLine(const QString& s)
{
    emit log("C: " + s);
    sock->write((s + "\r\n").toUtf8());
}

bool SmtpClient::codeIs(const QString& line, int code) const
{
    return line.startsWith(QString::number(code));
}

QString SmtpClient::b64(const QString& s) const
{
    return QString::fromUtf8(s.toUtf8().toBase64());
}

void SmtpClient::handleLine(const QString& line)
{
    emit log("S: " + line);

    static QRegularExpression lastLineRe(R"(^(\d{3})\s)");
    auto m = lastLineRe.match(line);
    if (!m.hasMatch())
        return;

    const int code = m.captured(1).toInt();

    switch (state) {

    case State::Wait220:
        if (code == 220) {
            state = State::Ehlo1;
            sendLine("EHLO localhost");
        } else {
            emit finished(false, "Expected 220, got: " + line);
            state = State::Done;
        }
        break;

    case State::Ehlo1:
        if (code == 250) {
            if (useTls) {
                state = State::StartTls;
                sendLine("STARTTLS");
            } else {
                state = State::AuthLogin;
                sendLine("AUTH LOGIN");
            }
        } else {
            emit finished(false, "EHLO failed: " + line);
            state = State::Done;
        }
        break;

    case State::StartTls:
        if (code == 220) {
            state = State::WaitTls;
            emit log("SMTP: starting TLS");
            sock->startClientEncryption();
        } else {
            emit finished(false, "STARTTLS failed: " + line);
            state = State::Done;
        }
        break;

    case State::Ehlo2:
        if (code == 250) {
            state = State::AuthLogin;
            sendLine("AUTH LOGIN");
        } else {
            emit finished(false, "EHLO after TLS failed: " + line);
            state = State::Done;
        }
        break;

    case State::AuthLogin:
        if (code == 334) {
            state = State::AuthUser;
            sendLine(b64(username));
        } else {
            emit finished(false, "AUTH LOGIN failed: " + line);
            state = State::Done;
        }
        break;

    case State::AuthUser:
        if (code == 334) {
            state = State::AuthPass;
            sendLine(b64(password));
        } else {
            emit finished(false, "AUTH USER rejected: " + line);
            state = State::Done;
        }
        break;

    case State::AuthPass:
        if (code == 235) {
            state = State::MailFrom;
            sendLine(QString("MAIL FROM:<%1>").arg(from));
        } else {
            emit finished(false, "AUTH PASS rejected: " + line);
            state = State::Done;
        }
        break;

    case State::MailFrom:
        if (code == 250) {
            state = State::RcptTo;
            sendLine(QString("RCPT TO:<%1>").arg(to));
        } else {
            emit finished(false, "MAIL FROM failed: " + line);
            state = State::Done;
        }
        break;

    case State::RcptTo:
        if (code == 250) {
            state = State::Data;
            sendLine("DATA");
        } else {
            emit finished(false, "RCPT TO failed: " + line);
            state = State::Done;
        }
        break;

    case State::Data:
        if (code == 354) {
            state = State::SendBody;

            QString msg;
            msg += "From: <" + from + ">\r\n";
            msg += "To: <" + to + ">\r\n";
            msg += "Subject: " + subject + "\r\n";
            msg += "MIME-Version: 1.0\r\n";
            msg += "Content-Type: text/plain; charset=UTF-8\r\n";
            msg += "Content-Transfer-Encoding: 8bit\r\n";
            msg += "\r\n";
            msg += body;
            msg += "\r\n.\r\n";

            emit log("C: [message body]");
            sock->write(msg.toUtf8());
        } else {
            emit finished(false, "DATA failed: " + line);
            state = State::Done;
        }
        break;

    case State::SendBody:
        if (code == 250) {
            state = State::Quit;
            sendLine("QUIT");
        } else {
            emit finished(false, "Sending body failed: " + line);
            state = State::Done;
        }
        break;

    case State::Quit:
        state = State::Done;
        emit finished(true, "Email sent OK");
        sock->disconnectFromHost();
        break;

    default:
        break;
    }
}
