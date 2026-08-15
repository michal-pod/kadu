/*
 * %kadu copyright begin%
 * Copyright 2015 Rafał Przemysław Malinowski (rafal.przemyslaw.malinowski@gmail.com)
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

#include "windows-thumbnail-toolbar.h"

#include "status/status-actions.h"
#include "status/status-container.h"
#include "status/status-setter.h"

#include <QtCore/QEvent>
#include <QtGui/QAction>
#include <QtWidgets/QWidget>

WindowsThumbnailToolbar::WindowsThumbnailToolbar(not_owned_qptr<StatusActions> statusActions, QWidget *parent)
        : QObject{parent}, m_window{parent}, m_statusActions{std::move(statusActions)}
{
    connect(
        m_statusActions, &StatusActions::statusActionsRecreated, this,
        &WindowsThumbnailToolbar::statusActionsRecreated);
    connect(m_statusActions, &StatusActions::statusActionTriggered, this, &WindowsThumbnailToolbar::changeStatus);

    if (m_window)
        m_window->installEventFilter(this);
    initializeToolbar();
}

WindowsThumbnailToolbar::~WindowsThumbnailToolbar()
{
    if (m_window)
        m_window->removeEventFilter(this);
}

bool WindowsThumbnailToolbar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_window && (event->type() == QEvent::Show || event->type() == QEvent::WinIdChange))
        initializeToolbar();

    return QObject::eventFilter(watched, event);
}

void WindowsThumbnailToolbar::initializeToolbar()
{
    if (!m_window || m_toolbar || !m_window->windowHandle())
        return;

    m_toolbar = new KaWinThumbnailToolBar{m_window->windowHandle()};
    statusActionsRecreated();
}

void WindowsThumbnailToolbar::setStatusSetter(StatusSetter *statusSetter)
{
    m_statusSetter = statusSetter;
}

void WindowsThumbnailToolbar::statusActionsRecreated()
{
    if (!m_toolbar || !m_statusActions)
        return;

    m_toolbar->clear();

    for (auto action : m_statusActions->actions())
    {
        auto button = make_owned<KaWinThumbnailToolButton>(m_toolbar.data());
        button->setToolTip(action->text());
        button->setIcon(action->icon());
        button->setDismissOnClick(true);
        connect(button.get(), &KaWinThumbnailToolButton::clicked, action, &QAction::trigger);
        m_toolbar->addButton(button.get());
    }
}

void WindowsThumbnailToolbar::changeStatus(QAction *action)
{
    if (!action || !m_statusActions || !m_statusSetter)
        return;

    auto statusType = action->data().value<StatusType>();

    for (auto &&container : m_statusActions->statusContainer()->subStatusContainers())
    {
        auto status = Status{m_statusSetter->manuallySetStatus(container)};
        status.setType(statusType);

        m_statusSetter->setStatusManually(container, status);
        container->storeStatus(status);
    }
}

#include "windows-thumbnail-toolbar.moc"
