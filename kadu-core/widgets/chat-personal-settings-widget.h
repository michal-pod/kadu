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

#include "chat/chat-notification-mode.h"
#include "exports.h"

#include <QtWidgets/QWidget>

class QComboBox;

class KADUAPI ChatPersonalSettingsWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit ChatPersonalSettingsWidget(QWidget *parent = nullptr);
    virtual ~ChatPersonalSettingsWidget() = default;

    QString tabTitle() const;
    ChatNotificationMode notificationMode() const;
    void setNotificationMode(ChatNotificationMode mode);
    void setEditingEnabled(bool enabled);

signals:
    void changed();

private:
    QComboBox *m_notificationModeCombo = nullptr;
};
