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

#include "remote-history-search-dialog.h"
#include "remote-history-search-dialog.moc"

#include "protocol-history-page-loader.h"

#include "protocols/services/protocol-history-request.h"
#include "protocols/services/protocol-history-service.h"
#include "widgets/webkit-messages-view/webkit-messages-view-factory.h"
#include "widgets/webkit-messages-view/webkit-messages-view.h"

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>

#include <utility>

RemoteHistorySearchDialog::RemoteHistorySearchDialog(ProtocolHistoryService *historyService, Chat chat,
                                                     QWidget *parent)
        : QDialog{parent}, m_historyService{historyService}, m_chat{std::move(chat)}
{
}

RemoteHistorySearchDialog::~RemoteHistorySearchDialog() = default;

void RemoteHistorySearchDialog::setWebkitMessagesViewFactory(
    WebkitMessagesViewFactory *webkitMessagesViewFactory)
{
    m_webkitMessagesViewFactory = webkitMessagesViewFactory;
}

void RemoteHistorySearchDialog::init()
{
    if (!m_historyService || !m_webkitMessagesViewFactory || !m_chat)
    {
        deleteLater();
        return;
    }

    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Search Remote History"));
    resize(750, 500);

    auto *layout = new QVBoxLayout{this};
    auto *searchLayout = new QHBoxLayout;
    m_query = new QLineEdit{this};
    m_query->setPlaceholderText(tr("Search messages"));
    m_search = new QPushButton{tr("Search"), this};
    m_search->setEnabled(false);
    searchLayout->addWidget(m_query);
    searchLayout->addWidget(m_search);
    layout->addLayout(searchLayout);

    m_error = new QLabel{this};
    m_error->setWordWrap(true);
    layout->addWidget(m_error);

    m_messagesView = m_webkitMessagesViewFactory->createWebkitMessagesView(m_chat, false, this);
    m_messagesView->setForcePruneDisabled(true);
    layout->addWidget(m_messagesView.get());

    connect(m_query, &QLineEdit::returnPressed, this, &RemoteHistorySearchDialog::search);
    connect(m_query, &QLineEdit::textChanged, this, &RemoteHistorySearchDialog::updateSearchEnabled);
    connect(m_search, &QPushButton::clicked, this, &RemoteHistorySearchDialog::search);
    m_query->setFocus();
}

void RemoteHistorySearchDialog::search()
{
    if (!m_historyService || !m_messagesView)
        return;

    const auto text = m_query->text().trimmed();
    if (text.isEmpty())
        return;

    delete m_pageLoader.data();
    m_error->clear();
    m_messagesView->clearMessages();

    ProtocolHistoryRequest request;
    request.setChat(m_chat);
    request.setText(text);
    request.setLimit(50);
    m_pageLoader = new ProtocolHistoryPageLoader{m_historyService, request, m_messagesView.get(), m_messagesView.get()};
    connect(m_pageLoader, &ProtocolHistoryPageLoader::errorOccurred, this, &RemoteHistorySearchDialog::showError);
}

void RemoteHistorySearchDialog::updateSearchEnabled(const QString &text)
{
    m_search->setEnabled(!text.trimmed().isEmpty());
}

void RemoteHistorySearchDialog::showError(const QString &error)
{
    m_error->setText(error);
}
