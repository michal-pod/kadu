/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 */

#include "matrix-restore-recovery-key-dialog.h"
#include "matrix-restore-recovery-key-dialog.moc"

#include <Quotient/connection.h>
#include <Quotient/database.h>
#include <Quotient/e2ee/cryptoutils.h>
#include <Quotient/events/event.h>

#include <QtConcurrent/QtConcurrentRun>
#include <QtCore/QFutureWatcher>
#include <QtCore/QJsonObject>

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

namespace
{
struct RestoredSecrets
{
    QHash<QString, QByteArray> keys;
    QString error;
};

// This worker only sees copied account data. Connection and its SQL database
// stay on the GUI thread; no full Megolm backup is downloaded during unlock.
RestoredSecrets restoreSecrets(QString encodedKey, const QJsonObject &accountData)
{
    using namespace Quotient;
    const auto invalidKey = [] {
        return RestoredSecrets{{}, MatrixRestoreRecoveryKeyDialog::tr("The recovery key is invalid.")};
    };
    encodedKey.removeIf([](QChar character) { return character.isSpace(); });
    if (encodedKey.size() > 128)
        return invalidKey();
    for (const auto character : encodedKey)
        if (!QStringLiteral("123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz").contains(character))
            return invalidKey();
    const auto decoded = base58Decode(encodedKey.toLatin1());
    if (decoded.size() != DefaultPbkdf2KeyLength + 3 || decoded[0] != 0x8B || decoded[1] != 0x01)
        return invalidKey();
    uint8_t parity = 0;
    for (auto byte : decoded)
        parity ^= byte;
    if (parity != 0)
        return invalidKey();

    const auto defaultKey = accountData.value(QStringLiteral("m.secret_storage.default_key"))
                                .toObject().value(QStringLiteral("key")).toString();
    const auto description = accountData.value(QStringLiteral("m.secret_storage.key.") + defaultKey).toObject();
    if (defaultKey.isEmpty() || description.isEmpty())
        return {{}, MatrixRestoreRecoveryKeyDialog::tr("The server does not contain a compatible encrypted key backup.")};
    if (description.value(QStringLiteral("algorithm")).toString()
        != QStringLiteral("m.secret_storage.v1.aes-hmac-sha2"))
        return {{}, MatrixRestoreRecoveryKeyDialog::tr("This server uses an unsupported key backup algorithm.")};

    const auto key = byte_view_t<>(decoded).subspan<2, DefaultPbkdf2KeyLength>();
    RestoredSecrets result;
    for (const auto &name : {QStringLiteral("m.megolm_backup.v1"), QStringLiteral("m.cross_signing.master"),
                            QStringLiteral("m.cross_signing.self_signing"), QStringLiteral("m.cross_signing.user_signing")})
    {
        const auto secret = accountData.value(name).toObject().value(QStringLiteral("encrypted"))
                                .toObject().value(defaultKey).toObject();
        // Cross-signing secrets are optional; the backup key is required.
        if (secret.isEmpty() && name != QStringLiteral("m.megolm_backup.v1"))
            continue;
        const auto decode = [&secret](const QString &part) {
            return QByteArray::fromBase64Encoding(secret.value(part).toString().toLatin1(),
                                                  QByteArray::AbortOnBase64DecodingErrors);
        };
        const auto iv = decode(QStringLiteral("iv"));
        const auto mac = decode(QStringLiteral("mac"));
        const auto cipher = decode(QStringLiteral("ciphertext"));
        if (!iv || iv.decoded.size() != AesBlockSize || !mac || mac.decoded.size() != HmacKeySize
            || !cipher || cipher.decoded.isEmpty())
            return {{}, MatrixRestoreRecoveryKeyDialog::tr("The encrypted recovery data is incomplete or invalid.")};
        const auto secretName = name.toUtf8();
        const auto keys = hkdfSha256(key, zeroes<32>(), asCBytes(secretName));
        if (!keys)
            return invalidKey();
        const auto actualMac = hmacSha256(keys->mac(), cipher.decoded);
        if (!actualMac || actualMac.value() != mac.decoded)
            return invalidKey();
        const auto plaintext = aesCtr256Decrypt(cipher.decoded, keys->aes(), asCBytes<AesBlockSize>(iv.decoded));
        if (!plaintext)
            return invalidKey();
        const auto restored = QByteArray::fromBase64Encoding(plaintext.value(), QByteArray::AbortOnBase64DecodingErrors);
        if (!restored || restored.decoded.size() != DefaultPbkdf2KeyLength)
            return {{}, MatrixRestoreRecoveryKeyDialog::tr("The decrypted recovery data is invalid.")};
        result.keys.insert(name, restored.decoded);
    }
    return result;
}
}

MatrixRestoreRecoveryKeyDialog::MatrixRestoreRecoveryKeyDialog(Quotient::Connection *connection, QWidget *parent)
    : QDialog{parent}, m_connection{connection}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Restore Matrix Recovery Key"));

    auto *layout = new QVBoxLayout{this};
    auto *description = new QLabel{
        tr("This device does not have the encryption keys stored on your Matrix account. "
           "Paste the recovery key to restore them."),
        this};
    description->setWordWrap(true);
    layout->addWidget(description);

    m_recoveryKeyEdit = new QLineEdit{this};
    m_recoveryKeyEdit->setEchoMode(QLineEdit::Password);
    m_recoveryKeyEdit->setPlaceholderText(tr("Recovery key"));
    layout->addWidget(m_recoveryKeyEdit);

    m_statusLabel = new QLabel{this};
    m_statusLabel->setTextFormat(Qt::PlainText);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    auto *buttons = new QDialogButtonBox{Qt::Horizontal, this};
    m_restoreButton = buttons->addButton(tr("Restore"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    layout->addWidget(buttons);

    connect(m_restoreButton, &QPushButton::clicked, this, &MatrixRestoreRecoveryKeyDialog::restore);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(this, &QDialog::finished, this, [this] { m_closed = true; });
    if (m_connection)
    {
        connect(m_connection, &Quotient::Connection::loggedOut, this, &QDialog::reject);
        connect(m_connection, &QObject::destroyed, this, &QDialog::reject);
    }
}

void MatrixRestoreRecoveryKeyDialog::setRestoreInProgress(bool inProgress)
{
    m_recoveryKeyEdit->setEnabled(!inProgress);
    m_restoreButton->setEnabled(!inProgress);
}

void MatrixRestoreRecoveryKeyDialog::showError(const QString &message)
{
    m_statusLabel->setText(message);
    m_recoveryKeyEdit->setFocus();
    m_recoveryKeyEdit->selectAll();
}

void MatrixRestoreRecoveryKeyDialog::restore()
{
    if (m_restored)
    {
        accept();
        return;
    }
    const auto recoveryKey = m_recoveryKeyEdit->text().trimmed();
    if (m_restoring || !m_connection || !m_connection->isLoggedIn() || !m_connection->database())
        return;
    if (recoveryKey.isEmpty())
    {
        showError(tr("Enter the recovery key."));
        return;
    }

    m_restoring = true;
    m_statusLabel->setText(tr("Restoring encryption keys..."));
    setRestoreInProgress(true);
    QJsonObject accountData;
    for (const auto &name : {QStringLiteral("m.secret_storage.default_key"), QStringLiteral("m.megolm_backup.v1"),
                            QStringLiteral("m.cross_signing.master"), QStringLiteral("m.cross_signing.self_signing"),
                            QStringLiteral("m.cross_signing.user_signing")})
        if (const auto &event = m_connection->accountData(name))
            accountData.insert(name, event->contentJson());
    const auto descriptionName = QStringLiteral("m.secret_storage.key.")
        + accountData.value(QStringLiteral("m.secret_storage.default_key")).toObject().value(QStringLiteral("key")).toString();
    if (const auto &event = m_connection->accountData(descriptionName))
        accountData.insert(descriptionName, event->contentJson());

    auto *watcher = new QFutureWatcher<RestoredSecrets>{this};
    connect(watcher, &QFutureWatcher<RestoredSecrets>::finished, this, [this, watcher] {
        const auto result = watcher->result();
        watcher->deleteLater();
        m_restoring = false;
        if (m_closed || !m_connection || !m_connection->isLoggedIn() || !m_connection->database())
            return;
        if (!result.error.isEmpty())
        {
            setRestoreInProgress(false);
            showError(result.error);
            return;
        }
        auto *database = m_connection->database();
        auto names = result.keys.keys();
        const auto backupName = QStringLiteral("m.megolm_backup.v1");
        names.removeAll(backupName);
        names.append(backupName); // Store the marker checked at startup last.
        for (const auto &name : names)
        {
            const auto secret = result.keys.value(name);
            database->storeEncrypted(name, secret);
            if (database->loadEncrypted(name) != secret)
            {
                setRestoreInProgress(false);
                showError(tr("The encryption keys could not be saved. Check that the profile directory is writable and try again."));
                return;
            }
        }
        m_recoveryKeyEdit->clear();
        m_restored = true;
        m_statusLabel->setText(tr("Recovery key restored. Message keys will be downloaded when you open encrypted messages."));
        m_restoreButton->setText(tr("Close"));
        m_restoreButton->setEnabled(true);
        emit keysRestored();
    });
    watcher->setFuture(QtConcurrent::run(restoreSecrets, recoveryKey, accountData));
}
