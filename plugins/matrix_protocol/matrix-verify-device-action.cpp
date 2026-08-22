/*
 * %kadu copyright begin%
 * Copyright 2026 Kadu Qt6 port
 * %kadu copyright end%
 */

#include "matrix-verify-device-action.h"
#include "matrix-verify-device-action.moc"

#include "accounts/account-manager.h"
#include "accounts/account.h"
#include "actions/action.h"
#include "identities/identity.h"
#include "matrix-protocol.h"
#include "menu/menu-inventory.h"

#include <QtCore/QTimer>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>

MatrixVerifyDeviceAction::MatrixVerifyDeviceAction(QObject *parent) : ActionDescription{parent}
{
    setType(ActionDescription::TypeMainMenu);
    setName("matrixVerifyDevice");
    setText(tr("Verify Matrix Device..."));
}

MatrixVerifyDeviceAction::~MatrixVerifyDeviceAction()
{
    if (m_menuInventory)
        m_menuInventory->menu("buddy")->removeAction(this)->update();
}

void MatrixVerifyDeviceAction::setAccountManager(AccountManager *accountManager)
{
    m_accountManager = accountManager;
}

void MatrixVerifyDeviceAction::setMenuInventory(MenuInventory *menuInventory)
{
    m_menuInventory = menuInventory;
}

void MatrixVerifyDeviceAction::init()
{
    connect(m_accountManager, &AccountManager::accountAdded, this, &MatrixVerifyDeviceAction::updateMenu);
    connect(m_accountManager, &AccountManager::accountRemoved, this, &MatrixVerifyDeviceAction::updateMenu);
    QTimer::singleShot(0, this, &MatrixVerifyDeviceAction::insertMenuActionDescription);
}

void MatrixVerifyDeviceAction::insertMenuActionDescription()
{
    if (m_menuInventory)
        m_menuInventory->menu("buddy")->addAction(this, KaduMenu::SectionBuddies, 30)->update();
}

void MatrixVerifyDeviceAction::actionInstanceCreated(Action *)
{
    updateMenu();
}

void MatrixVerifyDeviceAction::actionTriggered(QAction *sender, bool)
{
    verifyDevice(sender->data().value<Account>());
}

void MatrixVerifyDeviceAction::updateMenu()
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
            menu = new QMenu;
            action->setMenu(menu);
            connect(menu, &QMenu::triggered, this, &MatrixVerifyDeviceAction::menuActionTriggered);
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

void MatrixVerifyDeviceAction::menuActionTriggered(QAction *action)
{
    verifyDevice(action->data().value<Account>());
}

void MatrixVerifyDeviceAction::verifyDevice(const Account &account)
{
    auto *protocol = account ? qobject_cast<MatrixProtocol *>(account.protocolHandler()) : nullptr;
    if (!protocol)
        return;

    const auto devices = protocol->availableVerificationDevices();
    if (devices.isEmpty())
    {
        QMessageBox::information(
            nullptr, tr("Verify Matrix Device"),
            tr("No other Matrix devices are available for verification. Make sure the other device is online and has "
               "synchronised its encryption keys."));
        return;
    }

    bool accepted = false;
    const auto deviceId = QInputDialog::getItem(
        nullptr, tr("Verify Matrix Device"), tr("Device:"), devices, 0, false, &accepted);
    if (accepted && !deviceId.isEmpty())
        protocol->verifyDevice(deviceId);
}
