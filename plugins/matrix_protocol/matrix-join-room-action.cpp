/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 */

#include "matrix-join-room-action.h"
#include "matrix-join-room-action.moc"

#include "accounts/account-manager.h"
#include "accounts/account.h"
#include "actions/action.h"
#include "identities/identity.h"
#include "matrix-protocol.h"
#include "menu/menu-inventory.h"

#include <QtCore/QTimer>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMenu>

MatrixJoinRoomAction::MatrixJoinRoomAction(QObject *parent) : ActionDescription{parent}
{
    setType(ActionDescription::TypeMainMenu);
    setName("matrixJoinRoom");
    setText(tr("Join Matrix Room..."));
}

MatrixJoinRoomAction::~MatrixJoinRoomAction()
{
    if (m_menuInventory)
        m_menuInventory->menu("buddy")->removeAction(this)->update();
}

void MatrixJoinRoomAction::setAccountManager(AccountManager *accountManager)
{
    m_accountManager = accountManager;
}

void MatrixJoinRoomAction::setMenuInventory(MenuInventory *menuInventory)
{
    m_menuInventory = menuInventory;
}

void MatrixJoinRoomAction::init()
{
    connect(m_accountManager, &AccountManager::accountAdded, this, &MatrixJoinRoomAction::updateMenu);
    connect(m_accountManager, &AccountManager::accountRemoved, this, &MatrixJoinRoomAction::updateMenu);
    QTimer::singleShot(0, this, &MatrixJoinRoomAction::insertMenuActionDescription);
}

void MatrixJoinRoomAction::insertMenuActionDescription()
{
    if (m_menuInventory)
        m_menuInventory->menu("buddy")->addAction(this, KaduMenu::SectionBuddies, 29)->update();
}

void MatrixJoinRoomAction::actionInstanceCreated(Action *)
{
    updateMenu();
}

void MatrixJoinRoomAction::actionTriggered(QAction *sender, bool)
{
    joinRoom(sender->data().value<Account>());
}

void MatrixJoinRoomAction::updateMenu()
{
    if (!m_accountManager)
        return;

    const auto accounts = m_accountManager->byProtocolName("matrix");
    for (auto *action : actions())
    {
        if (accounts.isEmpty())
        {
            delete action->menu();
            action->setMenu(nullptr);
            action->setData({});
            action->setVisible(false);
            continue;
        }

        if (accounts.size() == 1)
        {
            delete action->menu();
            action->setMenu(nullptr);
            action->setData(QVariant::fromValue(accounts.constFirst()));
            action->setVisible(true);
            continue;
        }

        auto *menu = action->menu();
        if (!menu)
        {
            menu = new QMenu();
            action->setMenu(menu);
            connect(menu, &QMenu::triggered, this, &MatrixJoinRoomAction::menuActionTriggered);
        }
        else
            menu->clear();

        for (const auto &account : accounts)
        {
            auto *menuAction = menu->addAction(
                QStringLiteral("%1 (%2)").arg(account.accountIdentity().name(), account.id()));
            menuAction->setData(QVariant::fromValue(account));
        }
        action->setData({});
        action->setVisible(true);
    }
}

void MatrixJoinRoomAction::menuActionTriggered(QAction *action)
{
    joinRoom(action->data().value<Account>());
}

void MatrixJoinRoomAction::joinRoom(const Account &account)
{
    auto *protocol = account ? qobject_cast<MatrixProtocol *>(account.protocolHandler()) : nullptr;
    if (!protocol)
        return;

    const auto room = QInputDialog::getText(
        nullptr, tr("Join Matrix Room"), tr("Room ID or alias:"), QLineEdit::Normal, {}, nullptr).trimmed();
    if (!room.isEmpty())
        protocol->joinRoom(room);
}
