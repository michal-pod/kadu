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

#include "matrix-edit-account-widget.h"
#include "matrix-edit-account-widget.moc"

#include "matrix-account-form-utils.h"

#include "accounts/account-manager.h"
#include "accounts/account-shared.h"
#include "identities/identity-manager.h"
#include "plugin/plugin-injected-factory.h"
#include "widgets/account-configuration-widget-tab-adapter.h"
#include "widgets/account-avatar-widget.h"
#include "widgets/identities-combo-box.h"
#include "widgets/simple-configuration-value-state-notifier.h"

#include "../matrix-account-data.h"
#include "../matrix-protocol.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QVBoxLayout>

MatrixEditAccountWidget::MatrixEditAccountWidget(Account account, QWidget *parent) : AccountEditWidget{account, parent}
{
}

void MatrixEditAccountWidget::setAccountManager(AccountManager *accountManager)
{
    m_accountManager = accountManager;
}

void MatrixEditAccountWidget::setIdentityManager(IdentityManager *identityManager)
{
    m_identityManager = identityManager;
}

void MatrixEditAccountWidget::setPluginInjectedFactory(PluginInjectedFactory *pluginInjectedFactory)
{
    m_pluginInjectedFactory = pluginInjectedFactory;
}

void MatrixEditAccountWidget::init()
{
    createGui();
    loadAccountData();
    stateChangedSlot(stateNotifier()->state());
}

void MatrixEditAccountWidget::createGui()
{
    auto mainLayout = new QVBoxLayout{this};
    auto tabs = new QTabWidget{this};
    mainLayout->addWidget(tabs);

    auto general = new QWidget{tabs};
    auto generalLayout = new QHBoxLayout{general};
    auto form = new QFormLayout;
    generalLayout->addLayout(form, 1);

    m_login = new QLineEdit{general};
    m_login->setReadOnly(true);
    m_login->setToolTip(tr("The login of an existing Matrix account cannot be changed."));
    form->addRow(tr("Login:"), m_login);

    m_password = new QLineEdit{general};
    m_password->setEchoMode(QLineEdit::Password);
    m_password->setPlaceholderText(tr("Leave empty to keep using the current session"));
    connect(m_password, SIGNAL(textEdited(QString)), this, SLOT(dataChanged()));
    form->addRow(tr("Password:"), m_password);

    m_homeserver = new QLineEdit{general};
    m_homeserver->setReadOnly(true);
    m_homeserver->setToolTip(tr("The homeserver of an existing Matrix account cannot be changed."));
    form->addRow(tr("Homeserver:"), m_homeserver);

    m_identity = m_pluginInjectedFactory->makeInjected<IdentitiesComboBox>(general);
    connect(m_identity, SIGNAL(currentIndexChanged(int)), this, SLOT(dataChanged()));
    form->addRow(tr("Account identity:"), m_identity);

    auto info = new QLabel{
        tr("Kadu does not save the account password. A password entered here is kept only until it is used for "
           "authentication; the Matrix access token remains in the system credential store."),
        general};
    info->setWordWrap(true);
    form->addRow(QString{}, info);

    m_recoverKeysButton = new QPushButton{tr("Recover encryption keys..."), general};
    form->addRow(m_recoverKeysButton);
    connect(m_recoverKeysButton, &QPushButton::clicked, this, [this] {
        if (auto *protocol = qobject_cast<MatrixProtocol *>(account().protocolHandler()))
            protocol->restoreRecoveryKey();
    });
    if (auto *protocol = qobject_cast<MatrixProtocol *>(account().protocolHandler()))
        connect(protocol, &MatrixProtocol::recoveryKeyRestoreStateChanged,
                this, &MatrixEditAccountWidget::updateRecoveryButton);
    updateRecoveryButton();

    auto *avatarWidget = m_pluginInjectedFactory->makeInjected<AccountAvatarWidget>(account(), general);
    generalLayout->addWidget(avatarWidget, 0, Qt::AlignTop);
    tabs->addTab(general, tr("General"));

    new AccountConfigurationWidgetTabAdapter{this, tabs, this};

    auto buttons = new QDialogButtonBox{Qt::Horizontal, this};
    m_applyButton = new QPushButton{
        qApp->style()->standardIcon(QStyle::SP_DialogApplyButton), tr("Apply"), this};
    m_cancelButton = new QPushButton{
        qApp->style()->standardIcon(QStyle::SP_DialogCancelButton), tr("Cancel"), this};
    auto removeButton = new QPushButton{
        qApp->style()->standardIcon(QStyle::SP_DialogCancelButton), tr("Delete account"), this};
    buttons->addButton(m_applyButton, QDialogButtonBox::ApplyRole);
    buttons->addButton(m_cancelButton, QDialogButtonBox::RejectRole);
    buttons->addButton(removeButton, QDialogButtonBox::DestructiveRole);
    connect(m_applyButton, SIGNAL(clicked(bool)), this, SLOT(apply()));
    connect(m_cancelButton, SIGNAL(clicked(bool)), this, SLOT(cancel()));
    connect(removeButton, SIGNAL(clicked(bool)), this, SLOT(removeAccount()));
    mainLayout->addWidget(buttons);

    connect(
        stateNotifier(), SIGNAL(stateChanged(ConfigurationValueState)), this,
        SLOT(stateChangedSlot(ConfigurationValueState)));
}

void MatrixEditAccountWidget::loadAccountData()
{
    m_identity->setCurrentIdentity(account().accountIdentity());
    m_login->setText(MatrixAccountForm::loginFromMatrixId(account().id()));
    m_homeserver->setText(MatrixAccountForm::homeserverHost(MatrixAccountData{account()}.homeserver()));
    m_password->clear();
    simpleStateNotifier()->setState(StateNotChanged);
}

void MatrixEditAccountWidget::updateRecoveryButton()
{
    if (!m_recoverKeysButton)
        return;

    const auto *protocol = qobject_cast<MatrixProtocol *>(account().protocolHandler());
    m_recoverKeysButton->setVisible(protocol && protocol->recoveryKeyRestoreRequired());
}

void MatrixEditAccountWidget::apply()
{
    if (!m_applyButton->isEnabled())
        return;

    applyAccountConfigurationWidgets();
    account().setPassword(m_password->text());
    account().setHasPassword(true);
    account().setRememberPassword(false);
    account().setAccountIdentity(m_identity->currentIdentity());
    account().data()->forceEmitUpdated();

    m_identityManager->removeUnused();
    simpleStateNotifier()->setState(StateNotChanged);
}

void MatrixEditAccountWidget::cancel()
{
    cancelAccountConfigurationWidgets();
    loadAccountData();
    m_identityManager->removeUnused();
}

void MatrixEditAccountWidget::removeAccount()
{
    const auto answer = QMessageBox::question(
        this, tr("Confirm account removal"),
        tr("Do you want to remove the Matrix account %1?").arg(account().id()), QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);
    if (answer == QMessageBox::Yes)
    {
        m_accountManager->removeAccountAndBuddies(account());
        deleteLater();
    }
}

void MatrixEditAccountWidget::dataChanged()
{
    const auto unchanged = account().accountIdentity() == m_identity->currentIdentity() &&
                           m_password->text().isEmpty();

    if (unchanged)
        simpleStateNotifier()->setState(StateNotChanged);
    else if (!m_identity->currentIdentity())
        simpleStateNotifier()->setState(StateChangedDataInvalid);
    else
        simpleStateNotifier()->setState(StateChangedDataValid);
}

void MatrixEditAccountWidget::stateChangedSlot(ConfigurationValueState state)
{
    m_applyButton->setEnabled(state == StateChangedDataValid);
    m_cancelButton->setEnabled(state != StateNotChanged);
}
