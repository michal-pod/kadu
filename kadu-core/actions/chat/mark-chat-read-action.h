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
#include "injeqt-type-roles.h"

#include <QtCore/QPointer>
#include <injeqt/injeqt.h>

class ChatManager;
class ChatServiceRepository;
class UnreadMessageRepository;

class MarkChatReadAction final : public ActionDescription
{
    Q_OBJECT
    INJEQT_TYPE_ROLE(ACTION)

public:
    Q_INVOKABLE explicit MarkChatReadAction(QObject *parent = nullptr);
    virtual ~MarkChatReadAction() = default;

protected:
    virtual void triggered(QWidget *widget, ActionContext *context, bool toggled) override;
    virtual void updateActionState(Action *action) override;

private:
    QPointer<ChatManager> m_chatManager;
    QPointer<ChatServiceRepository> m_chatServiceRepository;
    QPointer<UnreadMessageRepository> m_unreadMessageRepository;

private slots:
    INJEQT_SET void setChatManager(ChatManager *chatManager);
    INJEQT_SET void setChatServiceRepository(ChatServiceRepository *chatServiceRepository);
    INJEQT_SET void setUnreadMessageRepository(UnreadMessageRepository *unreadMessageRepository);
};
