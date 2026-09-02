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

#include "chat-timeline-preview.h"

#include "chat/timeline/chat-timeline-item.h"
#include "chat/timeline/chat-timeline-model.h"
#include "chat/timeline/chat-view-model.h"

#include <QtCore/QDateTime>
#include <QtCore/QUrl>
#include <QtQml/QQmlContext>
#include <QtQuick/QQuickItem>
#include <QtQuickWidgets/QQuickWidget>
#include <QtWidgets/QVBoxLayout>

ChatTimelinePreview::ChatTimelinePreview(QWidget *parent) : QFrame{parent}
{
    setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    setMinimumHeight(250);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    auto *layout = new QVBoxLayout{this};
    layout->setContentsMargins(0, 0, 0, 0);

    m_timelineView = new QQuickWidget{this};
    m_timelineView->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_viewModel = new ChatViewModel{Chat::null, nullptr, nullptr, nullptr, m_timelineView};
    populateTimeline();
    m_timelineView->rootContext()->setContextProperty(QStringLiteral("_chatViewModel"), m_viewModel);
    m_timelineView->setSource(QUrl{QStringLiteral("qrc:/Kadu/Chat/chat/qml/ChatPage.qml")});
    layout->addWidget(m_timelineView);
}

ChatTimelinePreview::~ChatTimelinePreview() = default;

void ChatTimelinePreview::setThemeSource(const QUrl &source)
{
    if (m_themeSource == source)
        return;

    m_themeSource = source;
    updateThemeSource();
}

void ChatTimelinePreview::setColorScheme(const QString &scheme)
{
    if (m_colorScheme == scheme)
        return;

    m_colorScheme = scheme;
    if (m_timelineView && m_timelineView->rootObject())
        m_timelineView->rootObject()->setProperty("themeColorSchemeOverride", m_colorScheme);
}

void ChatTimelinePreview::populateTimeline()
{
    const auto now = QDateTime::currentDateTime();

    ChatTimelineItem received;
    received.stableId = QStringLiteral("preview-received");
    received.sourceOrder = QByteArrayLiteral("1");
    received.timestamp = now.addSecs(-180);
    received.kind = ChatTimelineItemKind::TextMessage;
    received.sender.id = QStringLiteral("friend@example.org");
    received.sender.displayName = tr("Your friend");
    received.content.plainText = tr("This is how received messages look.");

    ChatTimelineItem sent;
    sent.stableId = QStringLiteral("preview-sent");
    sent.sourceOrder = QByteArrayLiteral("2");
    sent.timestamp = now.addSecs(-120);
    sent.kind = ChatTimelineItemKind::TextMessage;
    sent.sender.id = QStringLiteral("me@example.org");
    sent.sender.displayName = tr("You");
    sent.sender.own = true;
    sent.content.plainText = tr("And this is your reply.");
    sent.state.deliveryState = ChatTimelineDeliveryState::Delivered;

    ChatTimelineItem notice;
    notice.stableId = QStringLiteral("preview-notice");
    notice.sourceOrder = QByteArrayLiteral("3");
    notice.timestamp = now.addSecs(-60);
    notice.kind = ChatTimelineItemKind::LocalNotice;
    notice.content.plainText = tr("Timeline also shows room events.");

    m_viewModel->timeline()->reset({received, sent, notice});
}

void ChatTimelinePreview::updateThemeSource()
{
    if (!m_timelineView || !m_timelineView->rootObject())
        return;

    m_timelineView->rootObject()->setProperty("themeSourceOverride", m_themeSource);
    m_timelineView->rootObject()->setProperty("themeColorSchemeOverride", m_colorScheme);
}
