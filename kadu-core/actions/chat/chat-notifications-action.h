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

#include "actions/action-description.h"
#include "chat/chat.h"
#include "injeqt-type-roles.h"

#include <QtCore/QHash>
#include <QtCore/QPointer>
#include <injeqt/injeqt.h>

class ChatService;
class ChatServiceRepository;
class QMenu;
class QWidget;

class ChatNotificationsAction final : public ActionDescription
{
    Q_OBJECT
    INJEQT_TYPE_ROLE(ACTION)

public:
    Q_INVOKABLE explicit ChatNotificationsAction(QObject *parent = nullptr);
    virtual ~ChatNotificationsAction() = default;

protected:
    virtual QMenu *menuForAction(Action *action) override;
    virtual void updateActionState(Action *action) override;

private:
    struct PendingChange
    {
        ChatNotificationMode mode;
        QPointer<QWidget> parent;
    };

    QPointer<ChatServiceRepository> m_chatServiceRepository;
    QHash<QUuid, PendingChange> m_pendingChanges;

    void populateMenu(QMenu *menu, Action *action);
    void setMode(Action *action, ChatNotificationMode mode);
    void watchChatService(ChatService *chatService);

private slots:
    INJEQT_SET void setChatServiceRepository(ChatServiceRepository *chatServiceRepository);
    void chatNotificationModeChanged(const Chat &chat, ChatNotificationMode mode);
    void chatNotificationModeChangeFailed(const Chat &chat, const QString &error);
};
