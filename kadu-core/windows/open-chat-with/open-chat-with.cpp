/*
 * %kadu copyright begin%
 * Copyright 2016 Rafał Przemysław Malinowski (rafal.przemyslaw.malinowski@gmail.com)
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

#include "open-chat-with.h"
#include "open-chat-with.moc"

#include "accounts/account.h"
#include "accounts/filter/abstract-account-filter.h"
#include "configuration/configuration.h"
#include "configuration/config-file-variant-wrapper.h"
#include "core/injected-factory.h"
#include "icons/icons-manager.h"
#include "icons/kadu-icon.h"
#include "os/generic/window-geometry-manager.h"
#include "protocols/protocol.h"
#include "widgets/accounts-combo-box.h"
#include "widgets/chat-widget/chat-widget-manager.h"
#include "windows/conversation-start-form.h"

#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

class ConversationStartAccountFilter final : public AbstractAccountFilter
{
public:
    explicit ConversationStartAccountFilter(QObject *parent = nullptr) : AbstractAccountFilter{parent}
    {
    }

    bool acceptAccount(Account account) override
    {
        auto *protocol = account ? account.protocolHandler() : nullptr;
        return protocol && protocol->supportsConversationStart();
    }
};

OpenChatWith::OpenChatWith(QWidget *parent) : QDialog{parent}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowRole(QStringLiteral("kadu-start-conversation"));
    setWindowTitle(tr("Start Conversation"));
}

OpenChatWith::~OpenChatWith() = default;

void OpenChatWith::setChatWidgetManager(ChatWidgetManager *chatWidgetManager)
{
    m_chatWidgetManager = chatWidgetManager;
}

void OpenChatWith::setConfiguration(Configuration *configuration)
{
    m_configuration = configuration;
}

void OpenChatWith::setIconsManager(IconsManager *iconsManager)
{
    m_iconsManager = iconsManager;
}

void OpenChatWith::setInjectedFactory(InjectedFactory *injectedFactory)
{
    m_injectedFactory = injectedFactory;
}

void OpenChatWith::init()
{
    createGui();
    new WindowGeometryManager(
        new ConfigFileVariantWrapper(m_configuration, "General", "StartConversationWindowGeometry"),
        QRect{0, 50, 620, 640}, this);
}

void OpenChatWith::createGui()
{
    auto *mainLayout = new QVBoxLayout{this};
    auto *accountLayout = new QFormLayout;
    mainLayout->addLayout(accountLayout);

    m_accountCombo = m_injectedFactory->makeInjected<AccountsComboBox>(
        true, AccountsComboBox::NotVisibleWithOneRowSourceModel, this);
    m_accountCombo->setIncludeIdInDisplay(true);
    m_accountCombo->addFilter(new ConversationStartAccountFilter{m_accountCombo});
    accountLayout->addRow(tr("Account:"), m_accountCombo);

    m_formHost = new QWidget{this};
    m_formLayout = new QVBoxLayout{m_formHost};
    m_formLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(m_formHost, 1);

    auto *buttons = new QDialogButtonBox{this};
    m_primaryButton = new QPushButton{
        m_iconsManager->iconByPath(KaduIcon{QStringLiteral("internet-group-chat")}), tr("Open"), this};
    m_primaryButton->setDefault(true);
    buttons->addButton(m_primaryButton, QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttons);

    connect(m_accountCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(rebuildForm()));
    connect(m_primaryButton, &QPushButton::clicked, this, [this] {
        if (m_form)
            m_form->performPrimaryAction();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    rebuildForm();
}

void OpenChatWith::rebuildForm()
{
    if (m_placeholder)
    {
        m_formLayout->removeWidget(m_placeholder);
        delete m_placeholder;
        m_placeholder = nullptr;
    }
    if (m_form)
    {
        m_formLayout->removeWidget(m_form);
        delete m_form;
        m_form = nullptr;
    }

    const auto account = m_accountCombo->currentAccount();
    auto *protocol = account ? account.protocolHandler() : nullptr;
    if (protocol && protocol->supportsConversationStart())
        m_form = protocol->createConversationStartForm(m_formHost);

    if (m_form)
    {
        m_formLayout->addWidget(m_form);
        connect(m_form, &ConversationStartForm::stateChanged, this, &OpenChatWith::refreshState);
        connect(m_form, &ConversationStartForm::chatReady, this, &OpenChatWith::openChat);
    }
    else
    {
        m_placeholder = new QLabel{tr("No account currently supports starting conversations here."), m_formHost};
        m_placeholder->setWordWrap(true);
        m_formLayout->addWidget(m_placeholder);
    }
    refreshState();
}

void OpenChatWith::refreshState()
{
    m_primaryButton->setText(m_form ? m_form->primaryActionText() : tr("Open"));
    m_primaryButton->setEnabled(m_form && m_form->primaryActionEnabled());
    m_accountCombo->setEnabled(!m_form || !m_form->operationInProgress());
}

void OpenChatWith::openChat(const Chat &chat)
{
    if (!chat || !m_chatWidgetManager)
        return;

    m_chatWidgetManager->openChat(chat, OpenChatActivation::Activate);
    accept();
}

void OpenChatWith::show()
{
    if (!isVisible())
        QDialog::show();
    else
    {
        raise();
        activateWindow();
    }
}
