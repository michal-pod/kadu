/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 */

#pragma once

#include <QtWidgets/QDialog>

class MatrixRoomInvitationDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit MatrixRoomInvitationDialog(const QString &roomName, QWidget *parent = nullptr);

signals:
    void joinRequested();
    void rejectRequested();
};
