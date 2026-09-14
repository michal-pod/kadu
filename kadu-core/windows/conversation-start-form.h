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

#include "chat/chat.h"
#include "exports.h"

#include <QtWidgets/QWidget>

class KADUAPI ConversationStartForm : public QWidget
{
    Q_OBJECT

public:
    explicit ConversationStartForm(QWidget *parent = nullptr);
    ~ConversationStartForm() override;

    virtual QString primaryActionText() const = 0;
    virtual bool primaryActionEnabled() const = 0;
    virtual bool operationInProgress() const;
    virtual void performPrimaryAction() = 0;

signals:
    void stateChanged();
    void chatReady(Chat chat);
};
