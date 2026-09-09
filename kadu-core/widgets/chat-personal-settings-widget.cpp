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

#include "chat-personal-settings-widget.h"
#include "chat-personal-settings-widget.moc"

#include <QtCore/QSignalBlocker>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>

ChatPersonalSettingsWidget::ChatPersonalSettingsWidget(QWidget *parent) : QWidget{parent}
{
    auto form = new QFormLayout{this};
    m_notificationModeCombo = new QComboBox{this};
    m_notificationModeCombo->addItem(tr("Default"), static_cast<int>(ChatNotificationMode::Default));
    m_notificationModeCombo->addItem(tr("All messages"), static_cast<int>(ChatNotificationMode::AllMessages));
    m_notificationModeCombo->addItem(tr("Mentions only"), static_cast<int>(ChatNotificationMode::MentionsOnly));
    m_notificationModeCombo->addItem(
        tr("No notifications"), static_cast<int>(ChatNotificationMode::NoNotifications));
    form->addRow(tr("Notifications:"), m_notificationModeCombo);

    m_priorityCombo = new QComboBox{this};
    m_priorityCombo->addItem(tr("Favorite"), static_cast<qint32>(ChatPriority::Favorite));
    m_priorityCombo->addItem(tr("Default"), static_cast<qint32>(ChatPriority::Default));
    m_priorityCombo->addItem(tr("Low priority"), static_cast<qint32>(ChatPriority::LowPriority));
    form->addRow(tr("Priority:"), m_priorityCombo);
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);

    connect(m_notificationModeCombo, &QComboBox::currentIndexChanged, this,
            &ChatPersonalSettingsWidget::changed);
    connect(m_priorityCombo, &QComboBox::currentIndexChanged, this, &ChatPersonalSettingsWidget::changed);
}

QString ChatPersonalSettingsWidget::tabTitle() const
{
    return tr("My settings");
}

ChatNotificationMode ChatPersonalSettingsWidget::notificationMode() const
{
    return static_cast<ChatNotificationMode>(m_notificationModeCombo->currentData().toInt());
}

void ChatPersonalSettingsWidget::setNotificationMode(ChatNotificationMode mode)
{
    const QSignalBlocker blocker{m_notificationModeCombo};
    m_notificationModeCombo->setCurrentIndex(m_notificationModeCombo->findData(static_cast<int>(mode)));
}

ChatPriority ChatPersonalSettingsWidget::priority() const
{
    return static_cast<ChatPriority>(m_priorityCombo->currentData().toInt());
}

void ChatPersonalSettingsWidget::setPriority(ChatPriority priority)
{
    const QSignalBlocker blocker{m_priorityCombo};
    m_priorityCombo->setCurrentIndex(m_priorityCombo->findData(static_cast<qint32>(priority)));
}

void ChatPersonalSettingsWidget::setEditingEnabled(bool enabled)
{
    m_notificationModeCombo->setEnabled(enabled);
    m_priorityCombo->setEnabled(enabled);
}
