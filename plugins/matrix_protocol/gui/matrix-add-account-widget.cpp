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

#include "accounts/account-manager.h"
#include "accounts/account-storage.h"
#include "identities/identity-manager.h"
#include "plugin/plugin-injected-factory.h"
#include "widgets/identities-combo-box.h"
#include "widgets/simple-configuration-value-state-notifier.h"

#include "../matrix-account-data.h"
#include "../matrix-id-validator.h"

#include <QtCore/QUrl>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
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

    m_matrixId = new QLineEdit{this};
    m_matrixId->setValidator(new MatrixIdValidator{m_matrixId});
    m_matrixId->setPlaceholderText("@alice:example.org");
    connect(m_matrixId, SIGNAL(textEdited(QString)), this, SLOT(dataChanged()));
    form->addRow(tr("Matrix ID:"), m_matrixId);

    m_homeserver = new QLineEdit{this};
    m_homeserver->setPlaceholderText("https://matrix.org");
    connect(m_homeserver, SIGNAL(textEdited(QString)), this, SLOT(dataChanged()));
    form->addRow(tr("Homeserver:"), m_homeserver);

    m_password = new QLineEdit{this};
    m_password->setEchoMode(QLineEdit::Password);
    connect(m_password, SIGNAL(textEdited(QString)), this, SLOT(dataChanged()));
    form->addRow(tr("Password:"), m_password);

    m_rememberPassword = new QCheckBox{tr("Remember password"), this};
    connect(m_rememberPassword, SIGNAL(toggled(bool)), this, SLOT(dataChanged()));
    form->addRow(QString{}, m_rememberPassword);

    m_identity = m_pluginInjectedFactory->makeInjected<IdentitiesComboBox>(this);
    connect(m_identity, SIGNAL(currentIndexChanged(int)), this, SLOT(dataChanged()));
    form->addRow(tr("Account identity:"), m_identity);

    auto info = new QLabel{
        tr("The account uses password login. Message delivery and room synchronisation will be added in a later "
           "step."),
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
    m_matrixId->clear();
    m_homeserver->setText("https://matrix.org");
    m_password->clear();
    m_rememberPassword->setChecked(true);
    m_identityManager->removeUnused();
    m_identity->setCurrentIndex(0);
    dataChanged();
}

bool MatrixAddAccountWidget::validHomeserver() const
{
    const auto value = m_homeserver->text().trimmed();
    const auto url = QUrl::fromUserInput(value);
    return !value.isEmpty() && url.isValid() && !url.host().isEmpty() &&
           (url.scheme() == "https" || url.scheme() == "http");
}

void MatrixAddAccountWidget::apply()
{
    if (!m_addAccountButton->isEnabled())
        return;

    auto account = m_accountStorage->create("matrix");
    account.setId(m_matrixId->text().trimmed());
    account.setPassword(m_password->text());
    account.setHasPassword(!m_password->text().isEmpty());
    account.setRememberPassword(m_rememberPassword->isChecked());
    account.setAccountIdentity(m_identity->currentIdentity());
    MatrixAccountData{account}.setHomeserver(m_homeserver->text().trimmed());

    resetGui();
    emit accountCreated(account);
}

void MatrixAddAccountWidget::cancel()
{
    resetGui();
}

void MatrixAddAccountWidget::dataChanged()
{
    const auto valid = m_matrixId->hasAcceptableInput() && validHomeserver() && m_identity->currentIdentity() &&
                       !m_accountManager->byId("matrix", m_matrixId->text().trimmed());
    m_addAccountButton->setEnabled(valid);

    const auto untouched = m_matrixId->text().isEmpty() && m_homeserver->text() == "https://matrix.org" &&
                           m_password->text().isEmpty() && m_rememberPassword->isChecked() &&
                           m_identity->currentIndex() == 0;
    simpleStateNotifier()->setState(untouched ? StateNotChanged : valid ? StateChangedDataValid : StateChangedDataInvalid);
}
