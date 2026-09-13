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

#include "matrix-add-account-widget.h"
#include "matrix-add-account-widget.moc"

#include "matrix-account-form-utils.h"

#include "accounts/account-manager.h"
#include "accounts/account-storage.h"
#include "identities/identity-manager.h"
#include "plugin/plugin-injected-factory.h"
#include "widgets/identities-combo-box.h"
#include "widgets/simple-configuration-value-state-notifier.h"

#include "../matrix-account-data.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

MatrixAddAccountWidget::MatrixAddAccountWidget(bool showButtons, QWidget *parent)
        : AccountAddWidget{parent}, m_showButtons{showButtons}
{
}

void MatrixAddAccountWidget::setAccountManager(AccountManager *accountManager)
{
    m_accountManager = accountManager;
}

void MatrixAddAccountWidget::setAccountStorage(AccountStorage *accountStorage)
{
    m_accountStorage = accountStorage;
}

void MatrixAddAccountWidget::setIdentityManager(IdentityManager *identityManager)
{
    m_identityManager = identityManager;
}

void MatrixAddAccountWidget::setPluginInjectedFactory(PluginInjectedFactory *pluginInjectedFactory)
{
    m_pluginInjectedFactory = pluginInjectedFactory;
}

void MatrixAddAccountWidget::init()
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_accountManager, SIGNAL(accountAdded(Account)), this, SLOT(dataChanged()));

    createGui();
    resetGui();
}

void MatrixAddAccountWidget::createGui()
{
    auto mainLayout = new QVBoxLayout{this};
    auto form = new QFormLayout;
    mainLayout->addLayout(form);

    m_login = new QLineEdit{this};
    m_login->setPlaceholderText(tr("alice"));
    connect(m_login, SIGNAL(textEdited(QString)), this, SLOT(dataChanged()));
    form->addRow(tr("Login:"), m_login);

    m_password = new QLineEdit{this};
    m_password->setEchoMode(QLineEdit::Password);
    connect(m_password, SIGNAL(textEdited(QString)), this, SLOT(dataChanged()));
    form->addRow(tr("Password:"), m_password);

    m_homeserver = new QComboBox{this};
    m_homeserver->setEditable(true);
    m_homeserver->setInsertPolicy(QComboBox::NoInsert);
    m_homeserver->addItems(MatrixAccountForm::suggestedHomeservers());
    m_homeserver->lineEdit()->setPlaceholderText(tr("example.org"));
    connect(m_homeserver, SIGNAL(editTextChanged(QString)), this, SLOT(dataChanged()));
    form->addRow(tr("Homeserver:"), m_homeserver);

    m_identity = m_pluginInjectedFactory->makeInjected<IdentitiesComboBox>(this);
    connect(m_identity, SIGNAL(currentIndexChanged(int)), this, SLOT(dataChanged()));
    form->addRow(tr("Account identity:"), m_identity);

    auto info = new QLabel{
        tr("The password is used only for the initial sign-in. Kadu stores the resulting access token in the "
           "system credential store instead of saving the password."),
        this};
    info->setWordWrap(true);
    form->addRow(QString{}, info);

    mainLayout->addStretch(1);

    auto buttons = new QDialogButtonBox{Qt::Horizontal, this};
    m_addAccountButton = new QPushButton{
        qApp->style()->standardIcon(QStyle::SP_DialogApplyButton), tr("Add account"), this};
    auto cancelButton = new QPushButton{
        qApp->style()->standardIcon(QStyle::SP_DialogCancelButton), tr("Cancel"), this};
    buttons->addButton(m_addAccountButton, QDialogButtonBox::AcceptRole);
    buttons->addButton(cancelButton, QDialogButtonBox::RejectRole);
    connect(m_addAccountButton, SIGNAL(clicked(bool)), this, SLOT(apply()));
    connect(cancelButton, SIGNAL(clicked(bool)), this, SLOT(cancel()));
    mainLayout->addWidget(buttons);

    if (!m_showButtons)
        buttons->hide();
}

void MatrixAddAccountWidget::resetGui()
{
    m_login->clear();
    m_homeserver->setCurrentText(QStringLiteral("matrix.org"));
    m_password->clear();
    m_identityManager->removeUnused();
    m_identity->setCurrentIndex(0);
    dataChanged();
}

bool MatrixAddAccountWidget::validHomeserver() const
{
    return MatrixAccountForm::isHomeserverHostValid(m_homeserver->currentText());
}

QString MatrixAddAccountWidget::matrixId() const
{
    return MatrixAccountForm::matrixId(m_login->text(), m_homeserver->currentText());
}

void MatrixAddAccountWidget::apply()
{
    if (!m_addAccountButton->isEnabled())
        return;

    auto account = m_accountStorage->create("matrix");
    account.setId(matrixId());
    account.setPassword(m_password->text());
    account.setHasPassword(!m_password->text().isEmpty());
    account.setRememberPassword(false);
    account.setAccountIdentity(m_identity->currentIdentity());
    MatrixAccountData{account}.setHomeserver(MatrixAccountForm::homeserverUrl(m_homeserver->currentText()));

    resetGui();
    emit accountCreated(account);
}

void MatrixAddAccountWidget::cancel()
{
    resetGui();
}

void MatrixAddAccountWidget::dataChanged()
{
    const auto valid = MatrixAccountForm::isLoginValid(m_login->text()) && validHomeserver()
                       && !m_password->text().isEmpty() && m_identity->currentIdentity()
                       && !m_accountManager->byId("matrix", matrixId());
    m_addAccountButton->setEnabled(valid);

    const auto untouched = m_login->text().isEmpty() && m_homeserver->currentText() == QStringLiteral("matrix.org") &&
                           m_password->text().isEmpty() && m_identity->currentIndex() == 0;
    simpleStateNotifier()->setState(untouched ? StateNotChanged : valid ? StateChangedDataValid : StateChangedDataInvalid);
}
