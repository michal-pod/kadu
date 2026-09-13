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

#include "matrix-device-verification-widget.h"
#include "matrix-device-verification-widget.moc"

#include "icons/icons-manager.h"
#include "icons/kadu-icon.h"

#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

QString MatrixDeviceVerificationWidget::errorMessage(Quotient::KeyVerificationSession::Error error)
{
    switch (error)
    {
    case Quotient::KeyVerificationSession::TIMEOUT:
    case Quotient::KeyVerificationSession::REMOTE_TIMEOUT:
        return tr("The verification timed out.");
    case Quotient::KeyVerificationSession::USER:
    case Quotient::KeyVerificationSession::REMOTE_USER:
        return tr("The verification was cancelled.");
    case Quotient::KeyVerificationSession::KEY_MISMATCH:
    case Quotient::KeyVerificationSession::REMOTE_KEY_MISMATCH:
    case Quotient::KeyVerificationSession::MISMATCHED_SAS:
    case Quotient::KeyVerificationSession::REMOTE_MISMATCHED_SAS:
        return tr("The security emoji did not match.");
    case Quotient::KeyVerificationSession::NONE:
        return tr("The verification could not be completed.");
    default:
        return tr("The verification was cancelled because the other device reported an error.");
    }
}

MatrixDeviceVerificationWidget::MatrixDeviceVerificationWidget(
    Quotient::KeyVerificationSession *session, IconsManager *iconsManager, QWidget *parent)
    : QWidget{parent}, m_session{session}, m_iconsManager{iconsManager}
{
    auto *layout = new QVBoxLayout{this};
    layout->setContentsMargins(0, 0, 0, 0);

    m_resultIconLabel = new QLabel{this};
    m_resultIconLabel->setAlignment(Qt::AlignCenter);
    m_resultIconLabel->setVisible(false);
    layout->addWidget(m_resultIconLabel);

    m_statusLabel = new QLabel{this};
    m_statusLabel->setTextFormat(Qt::PlainText);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    auto *emojisWidget = new QWidget{this};
    m_emojisLayout = new QVBoxLayout{emojisWidget};
    m_emojisLayout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(emojisWidget);

    auto *buttonsLayout = new QHBoxLayout;
    buttonsLayout->addStretch();
    m_acceptButton = new QPushButton{tr("Start verification"), this};
    m_confirmButton = new QPushButton{tr("They match"), this};
    m_mismatchButton = new QPushButton{tr("They do not match"), this};
    if (m_iconsManager)
    {
        m_acceptButton->setIcon(m_iconsManager->iconByPath(KaduIcon{QStringLiteral("security-high")}));
        m_confirmButton->setIcon(m_iconsManager->iconByPath(KaduIcon{QStringLiteral("dialog-ok")}));
        m_mismatchButton->setIcon(m_iconsManager->iconByPath(KaduIcon{QStringLiteral("dialog-error")}));
    }
    buttonsLayout->addWidget(m_acceptButton);
    buttonsLayout->addWidget(m_confirmButton);
    buttonsLayout->addWidget(m_mismatchButton);
    layout->addLayout(buttonsLayout);

    connect(m_acceptButton, &QPushButton::clicked, this, [this] {
        if (m_session && m_session->state() == Quotient::KeyVerificationSession::INCOMING)
            m_session->sendReady();
    });
    connect(m_confirmButton, &QPushButton::clicked, this, &MatrixDeviceVerificationWidget::finishVerification);
    connect(m_mismatchButton, &QPushButton::clicked, this, &MatrixDeviceVerificationWidget::rejectVerification);

    if (m_session)
    {
        connect(m_session, &Quotient::KeyVerificationSession::stateChanged, this,
                &MatrixDeviceVerificationWidget::updateState);
        connect(m_session, &Quotient::KeyVerificationSession::sasEmojisChanged, this,
                &MatrixDeviceVerificationWidget::updateState);
        connect(m_session, &Quotient::KeyVerificationSession::finished, this,
                &MatrixDeviceVerificationWidget::updateState);
        connect(m_session, &QObject::destroyed, this, [this] {
            if (!m_terminalState)
            {
                const auto message = tr("The verification session is no longer available.");
                showResultIcon(false);
                m_statusLabel->setAlignment(Qt::AlignCenter);
                m_statusLabel->setText(message);
                emit verificationFailed(message);
            }
        });
    }

    updateState();
}

void MatrixDeviceVerificationWidget::clearSasEmojis()
{
    while (auto *item = m_emojisLayout->takeAt(0))
    {
        delete item->widget();
        delete item;
    }
}

void MatrixDeviceVerificationWidget::showSasEmojis()
{
    clearSasEmojis();
    if (!m_session)
        return;

    auto *emojisWidget = new QWidget{this};
    auto *emojisGrid = new QGridLayout{emojisWidget};
    const auto emojis = m_session->sasEmojis();
    for (qsizetype index = 0; index < emojis.size(); ++index)
    {
        const auto &emoji = emojis.at(index);
        auto *emojiLabel = new QLabel{emoji.emoji};
        auto font = emojiLabel->font();
        font.setPointSize(font.pointSize() + 14);
        emojiLabel->setFont(font);
        emojiLabel->setAlignment(Qt::AlignCenter);

        auto *descriptionLabel = new QLabel{emoji.description};
        descriptionLabel->setAlignment(Qt::AlignCenter);

        const auto row = (index / 3) * 2;
        const auto column = index % 3;
        emojisGrid->addWidget(emojiLabel, row, column);
        emojisGrid->addWidget(descriptionLabel, row + 1, column);
    }
    m_emojisLayout->addWidget(emojisWidget);
}

void MatrixDeviceVerificationWidget::showResultIcon(bool success)
{
    if (!m_iconsManager)
    {
        m_resultIconLabel->setVisible(false);
        return;
    }

    const auto iconName = success ? QStringLiteral("dialog-ok") : QStringLiteral("dialog-error");
    m_resultIconLabel->setPixmap(m_iconsManager->iconByPath(KaduIcon{iconName}).pixmap(32, 32));
    m_resultIconLabel->setVisible(true);
}

void MatrixDeviceVerificationWidget::updateState()
{
    clearSasEmojis();
    m_resultIconLabel->setVisible(false);
    m_statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_acceptButton->setVisible(false);
    m_confirmButton->setVisible(false);
    m_mismatchButton->setVisible(false);

    if (!m_session)
    {
        showResultIcon(false);
        m_statusLabel->setAlignment(Qt::AlignCenter);
        m_statusLabel->setText(tr("The verification session is no longer available."));
        return;
    }

    switch (m_session->state())
    {
    case Quotient::KeyVerificationSession::INCOMING:
        m_statusLabel->setText(
            tr("Device %1 requests verification. Continue only if you started this verification from that device.")
                .arg(m_session->remoteDeviceId()));
        m_acceptButton->setVisible(true);
        break;
    case Quotient::KeyVerificationSession::WAITINGFORREADY:
        m_statusLabel->setText(tr("Waiting for device %1 to accept the verification request.")
                                   .arg(m_session->remoteDeviceId()));
        break;
    case Quotient::KeyVerificationSession::READY:
    case Quotient::KeyVerificationSession::WAITINGFORACCEPT:
    case Quotient::KeyVerificationSession::ACCEPTED:
    case Quotient::KeyVerificationSession::WAITINGFORKEY:
        m_statusLabel->setText(tr("Exchanging verification data with device %1.")
                                   .arg(m_session->remoteDeviceId()));
        break;
    case Quotient::KeyVerificationSession::WAITINGFORVERIFICATION:
        m_statusLabel->setText(
            tr("Compare these emoji with the other device. Confirm only when all of them match."));
        showSasEmojis();
        m_confirmButton->setVisible(true);
        m_mismatchButton->setVisible(true);
        break;
    case Quotient::KeyVerificationSession::WAITINGFORMAC:
        m_statusLabel->setText(tr("The emoji were confirmed. Waiting for device %1 to finish verification.")
                                   .arg(m_session->remoteDeviceId()));
        break;
    case Quotient::KeyVerificationSession::DONE:
        showResultIcon(true);
        m_statusLabel->setAlignment(Qt::AlignCenter);
        m_statusLabel->setText(tr("Device %1 was verified.").arg(m_session->remoteDeviceId()));
        if (!m_terminalState)
        {
            m_terminalState = true;
            emit verificationSucceeded();
        }
        break;
    case Quotient::KeyVerificationSession::CANCELED:
        showResultIcon(false);
        m_statusLabel->setAlignment(Qt::AlignCenter);
        m_statusLabel->setText(errorMessage(m_session->error()));
        if (!m_terminalState)
        {
            m_terminalState = true;
            const auto message = errorMessage(m_session->error());
            emit verificationFailed(message);
        }
        break;
    }
}

void MatrixDeviceVerificationWidget::finishVerification()
{
    if (m_session && m_session->state() == Quotient::KeyVerificationSession::WAITINGFORVERIFICATION)
        m_session->sendMac();
}

void MatrixDeviceVerificationWidget::rejectVerification()
{
    if (m_session)
        m_session->cancelVerification(Quotient::KeyVerificationSession::MISMATCHED_SAS);
}

void MatrixDeviceVerificationWidget::cancelVerification()
{
    if (m_session && m_session->state() != Quotient::KeyVerificationSession::DONE
        && m_session->state() != Quotient::KeyVerificationSession::CANCELED)
        m_session->cancelVerification(Quotient::KeyVerificationSession::USER);
}
