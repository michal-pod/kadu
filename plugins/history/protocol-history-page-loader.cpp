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

#include "protocol-history-page-loader.h"
#include "protocol-history-page-loader.moc"

#include "message/message.h"
#include "protocols/services/protocol-history-service.h"
#include "widgets/webkit-messages-view/webkit-messages-view.h"

#include <QtCore/QFutureWatcher>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QSet>
#include <QtCore/QVariant>
#include <QtWebEngineCore/QWebEnginePage>

ProtocolHistoryPageLoader::ProtocolHistoryPageLoader(ProtocolHistoryService *historyService,
                                                     ProtocolHistoryRequest request,
                                                     WebkitMessagesView *messagesView, QObject *parent)
        : QObject{parent}, m_historyService{historyService}, m_messagesView{messagesView}, m_request{std::move(request)}
{
    Q_ASSERT(m_historyService);
    Q_ASSERT(m_messagesView);

    connect(m_historyService, SIGNAL(destroyed()), this, SLOT(deleteLater()));
    connect(m_messagesView, SIGNAL(destroyed()), this, SLOT(deleteLater()));
    connect(m_messagesView, SIGNAL(scrolledToTop()), this, SLOT(loadPreviousPage()));

    loadPage();
}

void ProtocolHistoryPageLoader::loadPreviousPage()
{
    if (!m_hasMore)
        return;

    loadPage();
}

void ProtocolHistoryPageLoader::loadPage()
{
    if (m_loading || !m_historyService || !m_messagesView)
        return;

    m_loading = true;
    m_page = m_historyService->requestHistory(m_request);

    auto *futureWatcher = new QFutureWatcher<ProtocolHistoryPage>{this};
    connect(futureWatcher, SIGNAL(finished()), this, SLOT(pageAvailable()));
    connect(futureWatcher, SIGNAL(finished()), futureWatcher, SLOT(deleteLater()));
    futureWatcher->setFuture(m_page);
}

void ProtocolHistoryPageLoader::pageAvailable()
{
    if (!m_messagesView)
    {
        m_loading = false;
        return;
    }

    const auto page = m_page.result();
    if (!page.error().isEmpty())
    {
        m_hasMore = false;
        m_loading = false;
        return;
    }

    m_request.setCursor(page.cursor());
    m_request.setDirection(ProtocolHistoryRequest::Direction::Older);
    m_hasMore = page.hasMore() && !page.cursor().isEmpty();

    const auto messages = m_messagesView->messages();
    const auto anchor = messages.empty() ? Message::null : messages.messages().front();
    QSet<QString> visibleMessageIds;
    for (const auto &message : messages.messages())
        if (!message.id().isEmpty())
            visibleMessageIds.insert(message.id());

    SortedMessages newMessages;
    const auto pageMessages = page.messages();
    for (const auto &message : pageMessages.messages())
    {
        if (!message.id().isEmpty() && visibleMessageIds.contains(message.id()))
            continue;

        newMessages.add(message);
    }

    m_messagesView->setForcePruneDisabled(true);
    m_messagesView->add(newMessages);
    if (!newMessages.empty() && !anchor.isNull() && !anchor.id().isEmpty())
        restoreScrollPosition(anchor.id());
    m_loading = false;
    loadPreviousPageIfAtTop();
}

void ProtocolHistoryPageLoader::loadPreviousPageIfAtTop()
{
    if (!m_hasMore || !m_messagesView)
        return;

    const QPointer<ProtocolHistoryPageLoader> loader{this};
    m_messagesView->page()->runJavaScript(
        QStringLiteral("window.scrollY <= 2"), [loader](const QVariant &atTop) {
            if (loader && atTop.toBool())
                loader->loadPreviousPage();
        });
}

void ProtocolHistoryPageLoader::restoreScrollPosition(const QString &messageId) const
{
    if (!m_messagesView)
        return;

    const auto elementId = QStringLiteral("message_%1").arg(messageId);
    const auto encodedElementId = QString::fromUtf8(
        QJsonDocument{QJsonArray{elementId}}.toJson(QJsonDocument::JsonFormat::Compact));
    m_messagesView->page()->runJavaScript(
        QStringLiteral("var anchor = document.getElementById(%1[0]);"
                       "if (anchor) window.scrollTo(0, anchor.offsetTop);")
            .arg(encodedElementId));
}
