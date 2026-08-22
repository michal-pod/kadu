/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 */

#include "matrix-restore-recovery-key-dialog.h"
#include "matrix-restore-recovery-key-dialog.moc"

#include <Quotient/connection.h>
#include <Quotient/e2ee/sssshandler.h>

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

QString MatrixRestoreRecoveryKeyDialog::errorMessage(Quotient::SSSSHandler::Error error)
{
    switch (error)
    {
    case Quotient::SSSSHandler::WrongKeyError:
        return tr("The recovery key is invalid.");
    case Quotient::SSSSHandler::NoKeyError:
        return tr("The server does not contain a compatible encrypted key backup.");
    case Quotient::SSSSHandler::DecryptionError:
        return tr("The recovery key could not decrypt the key backup.");
    case Quotient::SSSSHandler::InvalidSignatureError:
        return tr("The key backup signature could not be verified.");
    case Quotient::SSSSHandler::UnsupportedAlgorithmError:
        return tr("This server uses an unsupported key backup algorithm.");
    }

    return tr("The recovery key could not be restored.");
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
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    auto *buttons = new QDialogButtonBox{Qt::Horizontal, this};
    m_restoreButton = buttons->addButton(tr("Restore"), QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    layout->addWidget(buttons);

    connect(m_restoreButton, &QPushButton::clicked, this, &MatrixRestoreRecoveryKeyDialog::restore);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
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
    const auto recoveryKey = m_recoveryKeyEdit->text().trimmed();
    if (!m_connection || recoveryKey.isEmpty())
        return;

    m_statusLabel->clear();
    setRestoreInProgress(true);

    m_handler = new Quotient::SSSSHandler{this};
    m_handler->setConnection(m_connection);
    connect(m_handler, &Quotient::SSSSHandler::error, this, [this](Quotient::SSSSHandler::Error error) {
        if (m_handler)
            m_handler->deleteLater();
        m_handler = nullptr;
        setRestoreInProgress(false);
        showError(errorMessage(error));
    });
    connect(m_handler, &Quotient::SSSSHandler::finished, this, [this] {
        if (m_handler)
            m_handler->deleteLater();
        m_handler = nullptr;
        accept();
    });
    m_handler->unlockSSSSFromSecurityKey(recoveryKey);
}
