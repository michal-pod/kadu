/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 */

#pragma once

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

signals:
    void keysRestored();

private:
    QPointer<Quotient::Connection> m_connection;
    QLineEdit *m_recoveryKeyEdit = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_restoreButton = nullptr;

    bool m_restoring = false;
    bool m_restored = false;
    bool m_closed = false;
    void setRestoreInProgress(bool inProgress);
    void showError(const QString &message);

private slots:
    void restore();
};
