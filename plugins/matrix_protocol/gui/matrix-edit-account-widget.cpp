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

#include "accounts/account-manager.h"
#include "accounts/account-shared.h"
#include "identities/identity-manager.h"
#include "plugin/plugin-injected-factory.h"
#include "widgets/account-configuration-widget-tab-adapter.h"
#include "widgets/account-avatar-widget.h"
#include "widgets/identities-combo-box.h"
#include "widgets/simple-configuration-value-state-notifier.h"

#include "../matrix-account-data.h"
#include "../matrix-id-validator.h"

#include <QtCore/QUrl>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
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

    m_matrixId = new QLineEdit{general};
    m_matrixId->setValidator(new MatrixIdValidator{m_matrixId});
    connect(m_matrixId, SIGNAL(textEdited(QString)), this, SLOT(dataChanged()));
    form->addRow(tr("Matrix ID:"), m_matrixId);

    m_homeserver = new QLineEdit{general};
    connect(m_homeserver, SIGNAL(textEdited(QString)), this, SLOT(dataChanged()));
    form->addRow(tr("Homeserver:"), m_homeserver);

    m_password = new QLineEdit{general};
    m_password->setEchoMode(QLineEdit::Password);
    connect(m_password, SIGNAL(textEdited(QString)), this, SLOT(dataChanged()));
    form->addRow(tr("Password:"), m_password);

    m_rememberPassword = new QCheckBox{tr("Remember password"), general};
    connect(m_rememberPassword, SIGNAL(toggled(bool)), this, SLOT(dataChanged()));
    form->addRow(QString{}, m_rememberPassword);

    m_identity = m_pluginInjectedFactory->makeInjected<IdentitiesComboBox>(general);
    connect(m_identity, SIGNAL(currentIndexChanged(int)), this, SLOT(dataChanged()));
    form->addRow(tr("Account identity:"), m_identity);

    auto info = new QLabel{
        tr("The account uses password login. Message delivery and room synchronisation will be added in a later "
           "step."),
        general};
    info->setWordWrap(true);
    form->addRow(QString{}, info);

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
    m_matrixId->setText(account().id());
    m_homeserver->setText(MatrixAccountData{account()}.homeserver());
    m_password->setText(account().password());
    m_rememberPassword->setChecked(account().rememberPassword());
    simpleStateNotifier()->setState(StateNotChanged);
}

bool MatrixEditAccountWidget::validHomeserver() const
{
    const auto value = m_homeserver->text().trimmed();
    const auto url = QUrl::fromUserInput(value);
    return !value.isEmpty() && url.isValid() && !url.host().isEmpty() &&
           (url.scheme() == "https" || url.scheme() == "http");
}

void MatrixEditAccountWidget::apply()
{
    if (!m_applyButton->isEnabled())
        return;

    applyAccountConfigurationWidgets();
    account().setId(m_matrixId->text().trimmed());
    account().setPassword(m_password->text());
    account().setHasPassword(!m_password->text().isEmpty());
    account().setRememberPassword(m_rememberPassword->isChecked());
    account().setAccountIdentity(m_identity->currentIdentity());
    MatrixAccountData{account()}.setHomeserver(m_homeserver->text().trimmed());
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
    const auto accountData = MatrixAccountData{account()};
    const auto sameIdExists = m_accountManager->byId(account().protocolName(), m_matrixId->text().trimmed()) &&
                              m_accountManager->byId(account().protocolName(), m_matrixId->text().trimmed()) != account();
    const auto unchanged = account().accountIdentity() == m_identity->currentIdentity() &&
                           account().id() == m_matrixId->text().trimmed() &&
                           accountData.homeserver() == m_homeserver->text().trimmed() &&
                           account().password() == m_password->text() &&
                           account().rememberPassword() == m_rememberPassword->isChecked();

    if (unchanged)
        simpleStateNotifier()->setState(StateNotChanged);
    else if (!m_matrixId->hasAcceptableInput() || !validHomeserver() || !m_identity->currentIdentity() || sameIdExists)
        simpleStateNotifier()->setState(StateChangedDataInvalid);
    else
        simpleStateNotifier()->setState(StateChangedDataValid);
}

void MatrixEditAccountWidget::stateChangedSlot(ConfigurationValueState state)
{
    m_applyButton->setEnabled(state == StateChangedDataValid);
    m_cancelButton->setEnabled(state != StateNotChanged);
}
