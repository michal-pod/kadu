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

#include <Quotient/keyverificationsession.h>

#include <QtCore/QPointer>
#include <QtWidgets/QWidget>

class QLabel;
class QPushButton;
class QVBoxLayout;
class IconsManager;

class MatrixDeviceVerificationWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit MatrixDeviceVerificationWidget(
        Quotient::KeyVerificationSession *session, IconsManager *iconsManager, QWidget *parent = nullptr);

    void cancelVerification();

signals:
    void verificationSucceeded();
    void verificationFailed(const QString &message);

private:
    QPointer<Quotient::KeyVerificationSession> m_session;
    QPointer<IconsManager> m_iconsManager;
    QLabel *m_resultIconLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QVBoxLayout *m_emojisLayout = nullptr;
    QPushButton *m_acceptButton = nullptr;
    QPushButton *m_confirmButton = nullptr;
    QPushButton *m_mismatchButton = nullptr;
    bool m_terminalState = false;

    static QString errorMessage(Quotient::KeyVerificationSession::Error error);
    void updateState();
    void showSasEmojis();
    void clearSasEmojis();
    void showResultIcon(bool success);
    void finishVerification();
    void rejectVerification();
};
