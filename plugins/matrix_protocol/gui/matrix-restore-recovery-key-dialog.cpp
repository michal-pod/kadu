/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "matrix-restore-recovery-key-dialog.h"
#include "matrix-restore-recovery-key-dialog.moc"

#include "matrix-device-verification-widget.h"

#include "icons/icons-manager.h"
#include "icons/kadu-icon.h"

#include <Quotient/connection.h>
#include <Quotient/csapi/device_management.h>
#include <Quotient/database.h>
#include <Quotient/e2ee/cryptoutils.h>
#include <Quotient/events/event.h>

#include <QtConcurrent/QtConcurrentRun>
#include <QtCore/QDateTime>
#include <QtCore/QFutureWatcher>
#include <QtCore/QJsonObject>
#include <QtCore/QLocale>
#include <QtCore/QTimer>

#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QVBoxLayout>

namespace MatrixRecoveryDetails
{
struct RestoredSecrets
{
    QHash<QString, QByteArray> keys;
    QString error;
};

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

MatrixRestoreRecoveryKeyDialog::MatrixRestoreRecoveryKeyDialog(
    Quotient::Connection *connection, IconsManager *iconsManager, bool allowDeferral, QWidget *parent)
    : QDialog{parent}, m_connection{connection}, m_iconsManager{iconsManager}, m_allowDeferral{allowDeferral}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Recover Matrix Encryption Keys"));
    setMinimumWidth(560);
    if (m_iconsManager)
        setWindowIcon(m_iconsManager->iconByPath(KaduIcon{QStringLiteral("dialog-password")}));

    auto *layout = new QVBoxLayout{this};
    m_pages = new QStackedWidget{this};
    m_pages->addWidget(createChoicePage());
    m_pages->addWidget(createDevicesPage());
    m_pages->addWidget(createVerificationPage());
    m_pages->addWidget(createRecoveryKeyPage());
    m_pages->addWidget(createWaitingPage());
    m_pages->addWidget(createCompletePage());
    layout->addWidget(m_pages);

    auto *buttons = new QDialogButtonBox{Qt::Horizontal, this};
    m_backButton = buttons->addButton(tr("Back"), QDialogButtonBox::ResetRole);
    m_primaryButton = buttons->addButton(tr("Continue"), QDialogButtonBox::AcceptRole);
    m_cancelButton = buttons->addButton(
        m_allowDeferral ? tr("Do this later") : tr("Cancel"), QDialogButtonBox::RejectRole);
    setButtonIcon(m_backButton, QStringLiteral("go-previous"));
    setButtonIcon(m_primaryButton, QStringLiteral("go-next"));
    setButtonIcon(m_cancelButton, QStringLiteral("dialog-cancel"));
    layout->addWidget(buttons);

    connect(m_backButton, &QPushButton::clicked, this, &MatrixRestoreRecoveryKeyDialog::back);
    connect(m_primaryButton, &QPushButton::clicked, this, &MatrixRestoreRecoveryKeyDialog::primaryAction);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(this, &QDialog::finished, this, [this] { m_closed = true; });
    if (m_connection)
    {
        connect(m_connection, &Quotient::Connection::loggedOut, this, &QDialog::reject);
        connect(m_connection, &QObject::destroyed, this, &QDialog::reject);
    }

    m_recoveryPollTimer = new QTimer{this};
    m_recoveryPollTimer->setInterval(250);
    connect(m_recoveryPollTimer, &QTimer::timeout, this, [this] {
        if (deviceRecoveryComplete())
            finishRecovery();
    });
    m_recoveryTimeoutTimer = new QTimer{this};
    m_recoveryTimeoutTimer->setSingleShot(true);
    connect(m_recoveryTimeoutTimer, &QTimer::timeout, this, [this] {
        failDeviceRecovery(
            tr("The verified device did not send the recovery key. Make sure it is online and approve the request, "
               "then try again or use your recovery key."));
    });

    setPage(Page::Choice);
}

QWidget *MatrixRestoreRecoveryKeyDialog::createChoicePage()
{
    auto *page = new QWidget{m_pages};
    auto *layout = new QVBoxLayout{page};
    auto *heading = new QLabel{tr("Recover encrypted messages"), page};
    auto font = heading->font();
    font.setBold(true);
    font.setPointSize(font.pointSize() + 2);
    heading->setFont(font);
    layout->addWidget(heading);

    auto *description = new QLabel{
        tr("This device does not have the encryption keys stored on your Matrix account. Choose how to recover them."),
        page};
    description->setWordWrap(true);
    layout->addWidget(description);

    auto *deviceGroup = new QGroupBox{page};
    auto *deviceLayout = new QVBoxLayout{deviceGroup};
    m_deviceChoice = new QRadioButton{tr("Verify with another device (recommended)"), deviceGroup};
    m_deviceChoice->setChecked(true);
    deviceLayout->addWidget(m_deviceChoice);
    auto *deviceDescription = new QLabel{
        tr("Compare security emoji on a device where this account is already signed in, then request the keys from it."),
        deviceGroup};
    deviceDescription->setWordWrap(true);
    deviceDescription->setIndent(24);
    deviceLayout->addWidget(deviceDescription);
    layout->addWidget(deviceGroup);

    auto *keyGroup = new QGroupBox{page};
    auto *keyLayout = new QVBoxLayout{keyGroup};
    m_recoveryKeyChoice = new QRadioButton{tr("Use a recovery key"), keyGroup};
    keyLayout->addWidget(m_recoveryKeyChoice);
    auto *keyDescription = new QLabel{
        tr("Enter the recovery key created by another Matrix client when encrypted backup was enabled."), keyGroup};
    keyDescription->setWordWrap(true);
    keyDescription->setIndent(24);
    keyLayout->addWidget(keyDescription);
    layout->addWidget(keyGroup);
    auto *methods = new QButtonGroup{page};
    methods->addButton(m_deviceChoice);
    methods->addButton(m_recoveryKeyChoice);
    layout->addStretch();
    return page;
}

QWidget *MatrixRestoreRecoveryKeyDialog::createDevicesPage()
{
    auto *page = new QWidget{m_pages};
    auto *layout = new QVBoxLayout{page};
    auto *heading = new QLabel{tr("Choose another device"), page};
    auto font = heading->font();
    font.setBold(true);
    heading->setFont(font);
    layout->addWidget(heading);
    auto *description = new QLabel{
        tr("Choose a device you can access now. Kadu will ask it to verify this session and provide the recovery key."),
        page};
    description->setWordWrap(true);
    layout->addWidget(description);
    m_devices = new QListWidget{page};
    m_devices->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_devices, 1);
    m_devicesStatusLabel = new QLabel{page};
    m_devicesStatusLabel->setTextFormat(Qt::PlainText);
    m_devicesStatusLabel->setWordWrap(true);
    layout->addWidget(m_devicesStatusLabel);
    auto *refreshButton = new QPushButton{tr("Refresh devices"), page};
    setButtonIcon(refreshButton, QStringLiteral("view-refresh"));
    layout->addWidget(refreshButton, 0, Qt::AlignLeft);
    connect(refreshButton, &QPushButton::clicked, this, &MatrixRestoreRecoveryKeyDialog::loadDevices);
    connect(m_devices, &QListWidget::itemSelectionChanged, this, [this] {
        const auto *item = m_devices->currentItem();
        const auto usable = item && item->flags().testFlag(Qt::ItemIsEnabled);
        m_primaryButton->setEnabled(usable);
        m_primaryButton->setText(
            usable && m_verifiedDevices.value(item->data(Qt::UserRole).toString())
                ? tr("Request keys") : tr("Verify device"));
        setButtonIcon(
            m_primaryButton,
            usable && m_verifiedDevices.value(item->data(Qt::UserRole).toString())
                ? QStringLiteral("dialog-password") : QStringLiteral("security-high"));
    });
    connect(m_devices, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        if (item && item->flags().testFlag(Qt::ItemIsEnabled))
            startDeviceVerification();
    });
    return page;
}

QWidget *MatrixRestoreRecoveryKeyDialog::createVerificationPage()
{
    auto *page = new QWidget{m_pages};
    m_verificationLayout = new QVBoxLayout{page};
    return page;
}

QWidget *MatrixRestoreRecoveryKeyDialog::createRecoveryKeyPage()
{
    auto *page = new QWidget{m_pages};
    auto *layout = new QVBoxLayout{page};
    auto *heading = new QLabel{tr("Use a recovery key"), page};
    auto font = heading->font();
    font.setBold(true);
    heading->setFont(font);
    layout->addWidget(heading);
    auto *description = new QLabel{
        tr("Paste the recovery key to restore the encryption secrets stored on your Matrix account."), page};
    description->setWordWrap(true);
    layout->addWidget(description);
    m_recoveryKeyEdit = new QLineEdit{page};
    m_recoveryKeyEdit->setEchoMode(QLineEdit::Password);
    m_recoveryKeyEdit->setPlaceholderText(tr("Recovery key"));
    if (m_iconsManager)
        m_recoveryKeyEdit->addAction(
            m_iconsManager->iconByPath(KaduIcon{QStringLiteral("dialog-password")}), QLineEdit::LeadingPosition);
    layout->addWidget(m_recoveryKeyEdit);
    m_showRecoveryKey = new QCheckBox{tr("Show recovery key"), page};
    layout->addWidget(m_showRecoveryKey);
    connect(m_showRecoveryKey, &QCheckBox::toggled, this, [this](bool checked) {
        m_recoveryKeyEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
    });
    connect(m_recoveryKeyEdit, &QLineEdit::returnPressed, this, &MatrixRestoreRecoveryKeyDialog::restore);
    m_statusLabel = new QLabel{page};
    m_statusLabel->setTextFormat(Qt::PlainText);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);
    layout->addStretch();
    return page;
}

QWidget *MatrixRestoreRecoveryKeyDialog::createWaitingPage()
{
    auto *page = new QWidget{m_pages};
    auto *layout = new QVBoxLayout{page};
    auto *heading = new QLabel{tr("Recovering encryption keys"), page};
    auto font = heading->font();
    font.setBold(true);
    heading->setFont(font);
    layout->addWidget(heading);
    m_waitingLabel = new QLabel{
        tr("The device was verified. Waiting for it to send the recovery key…"), page};
    m_waitingLabel->setWordWrap(true);
    layout->addWidget(m_waitingLabel);
    auto *progress = new QProgressBar{page};
    progress->setRange(0, 0);
    layout->addWidget(progress);
    layout->addStretch();
    return page;
}

QWidget *MatrixRestoreRecoveryKeyDialog::createCompletePage()
{
    auto *page = new QWidget{m_pages};
    auto *layout = new QVBoxLayout{page};
    if (m_iconsManager)
    {
        auto *resultIcon = new QLabel{page};
        resultIcon->setAlignment(Qt::AlignCenter);
        resultIcon->setPixmap(
            m_iconsManager->iconByPath(KaduIcon{QStringLiteral("dialog-ok")}).pixmap(32, 32));
        layout->addWidget(resultIcon);
    }
    auto *heading = new QLabel{tr("Encryption keys recovered"), page};
    auto font = heading->font();
    font.setBold(true);
    font.setPointSize(font.pointSize() + 2);
    heading->setFont(font);
    layout->addWidget(heading);
    auto *description = new QLabel{
        tr("The recovery key was saved on this device. Message keys will be downloaded when encrypted messages are opened."),
        page};
    description->setWordWrap(true);
    layout->addWidget(description);
    layout->addStretch();
    return page;
}

void MatrixRestoreRecoveryKeyDialog::setPage(Page page)
{
    m_pages->setCurrentIndex(static_cast<int>(page));
    updateButtons(page);
    if (page == Page::Devices && m_devices->count() == 0)
        loadDevices();
    else if (page == Page::RecoveryKey)
        m_recoveryKeyEdit->setFocus();
}

void MatrixRestoreRecoveryKeyDialog::updateButtons(Page page)
{
    m_backButton->setVisible(page == Page::Devices || page == Page::RecoveryKey);
    m_primaryButton->setVisible(page == Page::Choice || page == Page::Devices
                                || page == Page::RecoveryKey || page == Page::Complete);
    m_cancelButton->setVisible(page != Page::Complete);
    m_cancelButton->setEnabled(true);
    setButtonIcon(m_primaryButton, QStringLiteral("go-next"));

    switch (page)
    {
    case Page::Choice:
        m_primaryButton->setText(tr("Continue"));
        m_primaryButton->setEnabled(true);
        break;
    case Page::Devices:
        m_primaryButton->setText(tr("Verify device"));
        m_primaryButton->setEnabled(
            m_devices->currentItem() && m_devices->currentItem()->flags().testFlag(Qt::ItemIsEnabled));
        setButtonIcon(
            m_primaryButton,
            m_devices->currentItem()
                    && m_verifiedDevices.value(m_devices->currentItem()->data(Qt::UserRole).toString())
                ? QStringLiteral("dialog-password") : QStringLiteral("security-high"));
        break;
    case Page::RecoveryKey:
        m_primaryButton->setText(tr("Restore"));
        m_primaryButton->setEnabled(!m_restoring);
        setButtonIcon(m_primaryButton, QStringLiteral("dialog-password"));
        break;
    case Page::Complete:
        m_primaryButton->setText(tr("Close"));
        m_primaryButton->setEnabled(true);
        setButtonIcon(m_primaryButton, QStringLiteral("dialog-ok"));
        break;
    case Page::Verification:
    case Page::Waiting:
        break;
    }
}

void MatrixRestoreRecoveryKeyDialog::primaryAction()
{
    const auto page = static_cast<Page>(m_pages->currentIndex());
    switch (page)
    {
    case Page::Choice:
        setPage(m_deviceChoice->isChecked() ? Page::Devices : Page::RecoveryKey);
        break;
    case Page::Devices:
        startDeviceVerification();
        break;
    case Page::RecoveryKey:
        restore();
        break;
    case Page::Complete:
        accept();
        break;
    case Page::Verification:
    case Page::Waiting:
        break;
    }
}

void MatrixRestoreRecoveryKeyDialog::back()
{
    if (m_restoring)
        return;

    const auto page = static_cast<Page>(m_pages->currentIndex());
    setPage(page == Page::Verification ? Page::Devices : Page::Choice);
}

void MatrixRestoreRecoveryKeyDialog::loadDevices()
{
    if (!m_connection || !m_connection->isLoggedIn())
        return;

    const auto request = ++m_devicesRequest;
    m_devices->clear();
    m_verifiedDevices.clear();
    m_devicesStatusLabel->setText(tr("Loading devices…"));
    m_primaryButton->setEnabled(false);
    m_connection->reloadDevices();
    const QPointer<Quotient::Connection> requestedConnection{m_connection};
    auto job = requestedConnection->callApi<Quotient::GetDevicesJob>();
    connect(job, &Quotient::BaseJob::success, this, [this, job, request, requestedConnection] {
        if (!requestedConnection || requestedConnection.data() != m_connection.data() || request != m_devicesRequest)
            return;

        const auto knownDevices = requestedConnection->devicesForUser(requestedConnection->userId());
        const auto currentDeviceId = requestedConnection->deviceId();
        for (const auto &device : job->devices())
        {
            if (device.deviceId == currentDeviceId)
                continue;
            const auto knownForEncryption = knownDevices.contains(device.deviceId)
                                           && requestedConnection->isKnownE2eeCapableDevice(
                                               requestedConnection->userId(), device.deviceId);
            const auto verified = knownForEncryption && requestedConnection->isVerifiedDevice(
                                                        requestedConnection->userId(), device.deviceId);
            const auto name = device.displayName.isEmpty() ? tr("Unnamed Matrix device") : device.displayName;
            QString details = device.deviceId;
            if (device.lastSeenTs)
            {
                const auto activity = QDateTime::fromMSecsSinceEpoch(*device.lastSeenTs, Qt::UTC).toLocalTime();
                details += tr(" — last active %1").arg(QLocale{}.toString(activity, QLocale::ShortFormat));
            }
            if (verified)
                details += tr(" — verified");
            else if (!knownForEncryption)
                details += tr(" — encryption keys unavailable");

            auto *item = new QListWidgetItem{QStringLiteral("%1\n%2").arg(name, details), m_devices};
            if (m_iconsManager)
            {
                const auto iconName = verified
                                          ? QStringLiteral("security-high")
                                          : knownForEncryption ? QStringLiteral("computer")
                                                               : QStringLiteral("security-low");
                item->setIcon(m_iconsManager->iconByPath(KaduIcon{iconName}));
            }
            item->setData(Qt::UserRole, device.deviceId);
            if (!knownForEncryption)
            {
                auto flags = item->flags();
                flags.setFlag(Qt::ItemIsEnabled, false);
                flags.setFlag(Qt::ItemIsSelectable, false);
                item->setFlags(flags);
                item->setToolTip(tr("This session has not published encryption keys and cannot be used for recovery."));
            }
            m_verifiedDevices.insert(device.deviceId, verified);
        }

        if (m_devices->count() == 0)
            m_devicesStatusLabel->setText(
                tr("No other devices are available. Sign in on another device or go back and use a recovery key."));
        else
        {
            m_devicesStatusLabel->setText(
                tr("Open the selected Matrix client before continuing. Devices without encryption keys cannot be used."));
            for (int row = 0; row < m_devices->count(); ++row)
                if (m_devices->item(row)->flags().testFlag(Qt::ItemIsEnabled))
                {
                    m_devices->setCurrentRow(row);
                    break;
                }
        }
    });
    connect(job, &Quotient::BaseJob::failure, this, [this, job, request, requestedConnection] {
        if (!requestedConnection || requestedConnection.data() != m_connection.data() || request != m_devicesRequest)
            return;
        m_devicesStatusLabel->setText(tr("The device list could not be loaded: %1").arg(job->errorString()));
    });
}

void MatrixRestoreRecoveryKeyDialog::startDeviceVerification()
{
    auto *item = m_devices->currentItem();
    if (!item || !item->flags().testFlag(Qt::ItemIsEnabled) || !m_connection)
        return;
    const auto deviceId = item->data(Qt::UserRole).toString();
    if (deviceId.isEmpty())
        return;

    if (m_verifiedDevices.value(deviceId))
    {
        startDeviceRecovery();
        return;
    }
    if (m_connection->hasConflictingDeviceIdsAndCrossSigningKeys(m_connection->userId()))
    {
        m_devicesStatusLabel->setText(
            tr("The account has conflicting device and cross-signing keys. Verification cannot continue."));
        return;
    }

    auto *session = m_connection->startKeyVerificationSession(m_connection->userId(), deviceId);
    if (!session)
    {
        m_devicesStatusLabel->setText(tr("The verification request could not be started."));
        return;
    }

    if (m_verificationWidget)
    {
        m_verificationLayout->removeWidget(m_verificationWidget);
        m_verificationWidget->deleteLater();
    }
    m_verificationWidget = new MatrixDeviceVerificationWidget{
        session, m_iconsManager.data(), m_pages->widget(static_cast<int>(Page::Verification))};
    m_verificationLayout->addWidget(m_verificationWidget);
    connect(m_verificationWidget, &MatrixDeviceVerificationWidget::verificationSucceeded,
            this, &MatrixRestoreRecoveryKeyDialog::startDeviceRecovery);
    connect(m_verificationWidget, &MatrixDeviceVerificationWidget::verificationFailed, this,
            [this](const QString &message) {
                m_devicesStatusLabel->setText(message);
                m_backButton->setVisible(true);
                m_backButton->setEnabled(true);
            });
    setPage(Page::Verification);
}

void MatrixRestoreRecoveryKeyDialog::startDeviceRecovery()
{
    if (!m_connection || !m_connection->database())
        return;
    if (deviceRecoveryComplete())
    {
        finishRecovery();
        return;
    }

    m_restoring = true;
    setPage(Page::Waiting);
    m_recoveryPollTimer->start();
    m_recoveryTimeoutTimer->start(30000);

    const QPointer<Quotient::Connection> requestedConnection{m_connection};
    QTimer::singleShot(1500, this, [this, requestedConnection] {
        if (!m_restoring || !requestedConnection || requestedConnection.data() != m_connection.data()
            || deviceRecoveryComplete())
            return;
        for (const Quotient::event_type_t name : {
                 QLatin1String{"m.megolm_backup.v1"}, QLatin1String{"m.cross_signing.master"},
                 QLatin1String{"m.cross_signing.self_signing"}, QLatin1String{"m.cross_signing.user_signing"}})
        {
            auto *watcher = new QFutureWatcher<QByteArray>{this};
            connect(watcher, &QFutureWatcher<QByteArray>::finished, this, [this, watcher] {
                watcher->deleteLater();
                if (!m_closed && deviceRecoveryComplete())
                    finishRecovery();
            });
            watcher->setFuture(requestedConnection->requestKeyFromDevices(name));
        }
    });
}

bool MatrixRestoreRecoveryKeyDialog::deviceRecoveryComplete() const
{
    return m_connection && m_connection->database()
           && !m_connection->database()->loadEncrypted(QStringLiteral("m.megolm_backup.v1")).isEmpty();
}

void MatrixRestoreRecoveryKeyDialog::finishRecovery()
{
    if (m_restored)
        return;
    m_restoring = false;
    m_restored = true;
    m_recoveryPollTimer->stop();
    m_recoveryTimeoutTimer->stop();
    if (m_recoveryKeyEdit)
        m_recoveryKeyEdit->clear();
    setPage(Page::Complete);
    emit keysRestored();
}

void MatrixRestoreRecoveryKeyDialog::failDeviceRecovery(const QString &message)
{
    m_restoring = false;
    m_recoveryPollTimer->stop();
    m_recoveryTimeoutTimer->stop();
    setPage(Page::Devices);
    m_devicesStatusLabel->setText(message);
}

void MatrixRestoreRecoveryKeyDialog::setRestoreInProgress(bool inProgress)
{
    m_recoveryKeyEdit->setEnabled(!inProgress);
    m_showRecoveryKey->setEnabled(!inProgress);
    m_backButton->setEnabled(!inProgress);
    m_primaryButton->setEnabled(!inProgress);
}

void MatrixRestoreRecoveryKeyDialog::showError(const QString &message)
{
    m_restoring = false;
    setRestoreInProgress(false);
    m_statusLabel->setText(message);
    m_recoveryKeyEdit->clear();
    m_recoveryKeyEdit->setFocus();
}

void MatrixRestoreRecoveryKeyDialog::setButtonIcon(QPushButton *button, const QString &name) const
{
    if (button && m_iconsManager)
        button->setIcon(m_iconsManager->iconByPath(KaduIcon{name}));
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
    m_statusLabel->setText(tr("Restoring encryption keys…"));
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

    auto *watcher = new QFutureWatcher<MatrixRecoveryDetails::RestoredSecrets>{this};
    connect(watcher, &QFutureWatcher<MatrixRecoveryDetails::RestoredSecrets>::finished, this, [this, watcher] {
        const auto result = watcher->result();
        watcher->deleteLater();
        m_restoring = false;
        if (m_closed || !m_connection || !m_connection->isLoggedIn() || !m_connection->database())
            return;
        if (!result.error.isEmpty())
        {
            showError(result.error);
            return;
        }
        auto *database = m_connection->database();
        auto names = result.keys.keys();
        const auto backupName = QStringLiteral("m.megolm_backup.v1");
        names.removeAll(backupName);
        names.append(backupName);
        for (const auto &name : names)
        {
            const auto secret = result.keys.value(name);
            database->storeEncrypted(name, secret);
            if (database->loadEncrypted(name) != secret)
            {
                showError(tr("The encryption keys could not be saved. Check that the profile directory is writable and try again."));
                return;
            }
        }
        finishRecovery();
    });
    watcher->setFuture(QtConcurrent::run(MatrixRecoveryDetails::restoreSecrets, recoveryKey, accountData));
}

void MatrixRestoreRecoveryKeyDialog::reject()
{
    if (m_verificationWidget)
        m_verificationWidget->cancelVerification();
    QDialog::reject();
}
