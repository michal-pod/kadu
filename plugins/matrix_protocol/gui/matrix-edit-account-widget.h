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

#include "widgets/account-edit-widget.h"

#include <QtCore/QPointer>
#include <injeqt/injeqt.h>

class AccountManager;
class IdentitiesComboBox;
class IdentityManager;
class PluginInjectedFactory;
class QCheckBox;
class QLineEdit;
class QPushButton;

class MatrixEditAccountWidget final : public AccountEditWidget
{
    Q_OBJECT

public:
    explicit MatrixEditAccountWidget(Account account, QWidget *parent = nullptr);
    virtual ~MatrixEditAccountWidget() = default;

public slots:
    virtual void apply() override;
    virtual void cancel() override;

private:
    QPointer<AccountManager> m_accountManager;
    QPointer<IdentityManager> m_identityManager;
    QPointer<PluginInjectedFactory> m_pluginInjectedFactory;

    QLineEdit *m_matrixId = nullptr;
    QLineEdit *m_homeserver = nullptr;
    QLineEdit *m_password = nullptr;
    QCheckBox *m_rememberPassword = nullptr;
    IdentitiesComboBox *m_identity = nullptr;
    QPushButton *m_applyButton = nullptr;
    QPushButton *m_cancelButton = nullptr;

    void createGui();
    void loadAccountData();
    bool validHomeserver() const;

private slots:
    INJEQT_SET void setAccountManager(AccountManager *accountManager);
    INJEQT_SET void setIdentityManager(IdentityManager *identityManager);
    INJEQT_SET void setPluginInjectedFactory(PluginInjectedFactory *pluginInjectedFactory);
    INJEQT_INIT void init();

    virtual void removeAccount();
    void dataChanged();
    void stateChangedSlot(ConfigurationValueState state);
};
