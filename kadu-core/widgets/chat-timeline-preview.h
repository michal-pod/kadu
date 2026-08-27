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

#include <QtCore/QString>
#include <QtWidgets/QFrame>

class ChatViewModel;
class QQuickWidget;
class QWidget;

/**
 * @short Non-persistent sample timeline shown by the chat-theme configuration.
 *
 * The preview deliberately uses the same ChatPage QML component as an actual
 * conversation. Its isolated ChatTimelineModel means configuration does not
 * create chats, contacts, messages or protocol connections.
 */
class ChatTimelinePreview : public QFrame
{
public:
    explicit ChatTimelinePreview(QWidget *parent = nullptr);
    ~ChatTimelinePreview() override;

    void setTheme(const QString &theme);

private:
    ChatViewModel *m_viewModel = nullptr;
    QQuickWidget *m_timelineView = nullptr;
    QString m_theme;

    void populateTimeline();
    void updateTheme();
};
