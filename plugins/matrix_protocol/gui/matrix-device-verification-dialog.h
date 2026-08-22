/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 */

#pragma once

#include <Quotient/keyverificationsession.h>

#include <QtCore/QPointer>
#include <QtWidgets/QDialog>

class QDialogButtonBox;
class QLabel;
class QPushButton;
class QVBoxLayout;

class MatrixDeviceVerificationDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit MatrixDeviceVerificationDialog(Quotient::KeyVerificationSession *session, QWidget *parent = nullptr);

private:
    QPointer<Quotient::KeyVerificationSession> m_session;
    QLabel *m_statusLabel = nullptr;
    QVBoxLayout *m_emojisLayout = nullptr;
    QDialogButtonBox *m_buttons = nullptr;
    QPushButton *m_acceptButton = nullptr;
    QPushButton *m_confirmButton = nullptr;
    QPushButton *m_mismatchButton = nullptr;
    QPushButton *m_closeButton = nullptr;

    static QString errorMessage(Quotient::KeyVerificationSession::Error error);
    void updateState();
    void showSasEmojis();
    void clearSasEmojis();
    void finishVerification();
    void rejectVerification();

protected:
    void reject() override;
};
