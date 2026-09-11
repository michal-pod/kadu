/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 */

#include "matrix-device-verification-dialog.h"
#include "matrix-device-verification-dialog.moc"

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QString MatrixDeviceVerificationDialog::errorMessage(Quotient::KeyVerificationSession::Error error)
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

MatrixDeviceVerificationDialog::MatrixDeviceVerificationDialog(
    Quotient::KeyVerificationSession *session, QWidget *parent)
    : QDialog{parent}, m_session{session}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Verify Matrix Device"));

    auto *layout = new QVBoxLayout{this};
    m_statusLabel = new QLabel{this};
    m_statusLabel->setTextFormat(Qt::PlainText);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    auto *emojisWidget = new QWidget{this};
    m_emojisLayout = new QVBoxLayout{emojisWidget};
    m_emojisLayout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(emojisWidget);

    m_buttons = new QDialogButtonBox{Qt::Horizontal, this};
    m_acceptButton = m_buttons->addButton(tr("Start verification"), QDialogButtonBox::AcceptRole);
    m_confirmButton = m_buttons->addButton(tr("They match"), QDialogButtonBox::YesRole);
    m_mismatchButton = m_buttons->addButton(tr("They do not match"), QDialogButtonBox::DestructiveRole);
    m_closeButton = m_buttons->addButton(QDialogButtonBox::Close);
    layout->addWidget(m_buttons);

    connect(m_acceptButton, &QPushButton::clicked, this, [this] {
        if (m_session && m_session->state() == Quotient::KeyVerificationSession::INCOMING)
            m_session->sendReady();
    });
    connect(m_confirmButton, &QPushButton::clicked, this, &MatrixDeviceVerificationDialog::finishVerification);
    connect(m_mismatchButton, &QPushButton::clicked, this, &MatrixDeviceVerificationDialog::rejectVerification);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::reject);

    if (m_session)
    {
        connect(m_session, &Quotient::KeyVerificationSession::stateChanged, this,
                &MatrixDeviceVerificationDialog::updateState);
        connect(m_session, &Quotient::KeyVerificationSession::sasEmojisChanged, this,
                &MatrixDeviceVerificationDialog::updateState);
        connect(m_session, &Quotient::KeyVerificationSession::finished, this,
                &MatrixDeviceVerificationDialog::updateState);
        connect(m_session, &QObject::destroyed, this, [this] {
            // Keep the terminal result visible after libQuotient deletes the session.
            if (!m_terminalState)
                updateState();
        });
    }

    updateState();
}

void MatrixDeviceVerificationDialog::clearSasEmojis()
{
    while (auto *item = m_emojisLayout->takeAt(0))
    {
        delete item->widget();
        delete item;
    }
}

void MatrixDeviceVerificationDialog::showSasEmojis()
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
        font.setPointSize(font.pointSize() + 10);
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

void MatrixDeviceVerificationDialog::updateState()
{
    clearSasEmojis();
    m_acceptButton->setVisible(false);
    m_confirmButton->setVisible(false);
    m_mismatchButton->setVisible(false);
    m_closeButton->setVisible(true);
    m_closeButton->setText(tr("Cancel"));

    if (!m_session)
    {
        m_statusLabel->setText(tr("The verification session is no longer available."));
        m_closeButton->setText(tr("Close"));
        m_closeButton->setVisible(true);
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
        m_terminalState = true;
        m_closeButton->setText(tr("Close"));
        m_statusLabel->setText(tr("Device %1 was verified.").arg(m_session->remoteDeviceId()));
        m_closeButton->setVisible(true);
        break;
    case Quotient::KeyVerificationSession::CANCELED:
        m_terminalState = true;
        m_closeButton->setText(tr("Close"));
        m_statusLabel->setText(errorMessage(m_session->error()));
        m_closeButton->setVisible(true);
        break;
    }
}

void MatrixDeviceVerificationDialog::finishVerification()
{
    if (m_session && m_session->state() == Quotient::KeyVerificationSession::WAITINGFORVERIFICATION)
        m_session->sendMac();
}

void MatrixDeviceVerificationDialog::rejectVerification()
{
    if (m_session)
        m_session->cancelVerification(Quotient::KeyVerificationSession::MISMATCHED_SAS);
}

void MatrixDeviceVerificationDialog::reject()
{
    if (m_session && m_session->state() != Quotient::KeyVerificationSession::DONE
        && m_session->state() != Quotient::KeyVerificationSession::CANCELED)
        m_session->cancelVerification(Quotient::KeyVerificationSession::USER);

    QDialog::reject();
}
