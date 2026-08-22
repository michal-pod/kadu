/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 */

#pragma once

#include "actions/action-description.h"

#include <QtCore/QPointer>
#include <injeqt/injeqt.h>

class Account;
class AccountManager;
class MenuInventory;

class MatrixJoinRoomAction final : public ActionDescription
{
    Q_OBJECT

public:
    Q_INVOKABLE explicit MatrixJoinRoomAction(QObject *parent = nullptr);
    ~MatrixJoinRoomAction() override;

protected:
    void actionInstanceCreated(Action *action) override;
    void actionTriggered(QAction *sender, bool toggled) override;

private:
    QPointer<AccountManager> m_accountManager;
    QPointer<MenuInventory> m_menuInventory;

    void joinRoom(const Account &account);

private slots:
    INJEQT_SET void setAccountManager(AccountManager *accountManager);
    INJEQT_SET void setMenuInventory(MenuInventory *menuInventory);
    INJEQT_INIT void init();

    void insertMenuActionDescription();
    void updateMenu();
    void menuActionTriggered(QAction *action);
};
