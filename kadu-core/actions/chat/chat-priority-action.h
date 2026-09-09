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
#include "chat/chat-priority.h"
#include "injeqt-type-roles.h"

#include <QtCore/QPointer>
#include <injeqt/injeqt.h>

class ChatServiceRepository;
class QMenu;

class ChatPriorityAction final : public ActionDescription
{
    Q_OBJECT
    INJEQT_TYPE_ROLE(ACTION)

public:
    Q_INVOKABLE explicit ChatPriorityAction(QObject *parent = nullptr);
    virtual ~ChatPriorityAction() = default;

protected:
    virtual QMenu *menuForAction(Action *action) override;
    virtual void updateActionState(Action *action) override;

private:
    QPointer<ChatServiceRepository> m_chatServiceRepository;

    void populateMenu(QMenu *menu, Action *action);
    void setPriority(Action *action, ChatPriority priority);

private slots:
    INJEQT_SET void setChatServiceRepository(ChatServiceRepository *chatServiceRepository);
};
