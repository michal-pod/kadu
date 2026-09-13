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

#pragma once

#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <QtWidgets/QDialog>

class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QProgressBar;
class QPushButton;
class QRadioButton;
class QStackedWidget;
class QTimer;
class QVBoxLayout;
class MatrixDeviceVerificationWidget;

namespace Quotient
{
class Connection;
}

class MatrixRestoreRecoveryKeyDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit MatrixRestoreRecoveryKeyDialog(
        Quotient::Connection *connection, bool allowDeferral, QWidget *parent = nullptr);
    void reject() override;

signals:
    void keysRestored();

private:
    enum class Page
    {
        Choice,
        Devices,
        Verification,
        RecoveryKey,
        Waiting,
        Complete
    };

    QPointer<Quotient::Connection> m_connection;
    QStackedWidget *m_pages = nullptr;
    QRadioButton *m_deviceChoice = nullptr;
    QRadioButton *m_recoveryKeyChoice = nullptr;
    QListWidget *m_devices = nullptr;
    QLabel *m_devicesStatusLabel = nullptr;
    QVBoxLayout *m_verificationLayout = nullptr;
    MatrixDeviceVerificationWidget *m_verificationWidget = nullptr;
    QLineEdit *m_recoveryKeyEdit = nullptr;
    QCheckBox *m_showRecoveryKey = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_waitingLabel = nullptr;
    QPushButton *m_backButton = nullptr;
    QPushButton *m_primaryButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QTimer *m_recoveryPollTimer = nullptr;
    QTimer *m_recoveryTimeoutTimer = nullptr;
    QHash<QString, bool> m_verifiedDevices;

    bool m_allowDeferral = false;
    bool m_restoring = false;
    bool m_restored = false;
    bool m_closed = false;
    quint64 m_devicesRequest = 0;

    QWidget *createChoicePage();
    QWidget *createDevicesPage();
    QWidget *createVerificationPage();
    QWidget *createRecoveryKeyPage();
    QWidget *createWaitingPage();
    QWidget *createCompletePage();
    void setPage(Page page);
    void updateButtons(Page page);
    void primaryAction();
    void back();
    void loadDevices();
    void startDeviceVerification();
    void startDeviceRecovery();
    bool deviceRecoveryComplete() const;
    void finishRecovery();
    void failDeviceRecovery(const QString &message);
    void setRestoreInProgress(bool inProgress);
    void showError(const QString &message);

    void restore();
};
