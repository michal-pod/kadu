/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 */

#pragma once

#include <Quotient/e2ee/sssshandler.h>

#include <QtCore/QPointer>
#include <QtWidgets/QDialog>

class QLabel;
class QLineEdit;
class QPushButton;

namespace Quotient
{
class Connection;
}

class MatrixRestoreRecoveryKeyDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit MatrixRestoreRecoveryKeyDialog(Quotient::Connection *connection, QWidget *parent = nullptr);

private:
    QPointer<Quotient::Connection> m_connection;
    QPointer<Quotient::SSSSHandler> m_handler;
    QLineEdit *m_recoveryKeyEdit = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_restoreButton = nullptr;

    static QString errorMessage(Quotient::SSSSHandler::Error error);
    void setRestoreInProgress(bool inProgress);
    void showError(const QString &message);

private slots:
    void restore();
};
