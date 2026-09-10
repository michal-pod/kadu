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

#pragma once

#include "widgets/account-add-widget.h"

#include <QtCore/QPointer>
#include <injeqt/injeqt.h>

class AccountManager;
class AccountStorage;
class IdentitiesComboBox;
class IdentityManager;
class PluginInjectedFactory;
class QLineEdit;
class QPushButton;

class MatrixAddAccountWidget final : public AccountAddWidget
{
    Q_OBJECT

public:
    explicit MatrixAddAccountWidget(bool showButtons, QWidget *parent = nullptr);
    virtual ~MatrixAddAccountWidget() = default;

public slots:
    virtual void apply() override;
    virtual void cancel() override;

private:
    QPointer<AccountManager> m_accountManager;
    QPointer<AccountStorage> m_accountStorage;
    QPointer<IdentityManager> m_identityManager;
    QPointer<PluginInjectedFactory> m_pluginInjectedFactory;
    bool m_showButtons;

    QLineEdit *m_matrixId = nullptr;
    QLineEdit *m_homeserver = nullptr;
    QLineEdit *m_password = nullptr;
    IdentitiesComboBox *m_identity = nullptr;
    QPushButton *m_addAccountButton = nullptr;

    void createGui();
    void resetGui();
    bool validHomeserver() const;

private slots:
    INJEQT_SET void setAccountManager(AccountManager *accountManager);
    INJEQT_SET void setAccountStorage(AccountStorage *accountStorage);
    INJEQT_SET void setIdentityManager(IdentityManager *identityManager);
    INJEQT_SET void setPluginInjectedFactory(PluginInjectedFactory *pluginInjectedFactory);
    INJEQT_INIT void init();

    void dataChanged();
};
