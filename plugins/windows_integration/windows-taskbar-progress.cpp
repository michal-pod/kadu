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

#include "windows-taskbar-progress.h"
#include "windows-taskbar-progress.moc"

#include "file-transfer/file-transfer-manager.h"

#include <QtCore/QEvent>
#include <QtWidgets/QWidget>

WindowsTaskbarProgress::WindowsTaskbarProgress(FileTransferManager *fileTransferManager, QWidget *parent)
        : QObject{parent}, m_fileTransferManager{fileTransferManager}, m_window{parent}
{
    if (m_fileTransferManager)
        connect(m_fileTransferManager, SIGNAL(totalProgressChanged(int)), this, SLOT(progressChanged(int)));

    if (m_window)
        m_window->installEventFilter(this);
    initializeTaskbarButton();
}

WindowsTaskbarProgress::~WindowsTaskbarProgress()
{
    if (m_window)
        m_window->removeEventFilter(this);
}

bool WindowsTaskbarProgress::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_window && (event->type() == QEvent::Show || event->type() == QEvent::WinIdChange))
        initializeTaskbarButton();

    return QObject::eventFilter(watched, event);
}

void WindowsTaskbarProgress::initializeTaskbarButton()
{
    if (!m_window || m_taskbarButton || !m_window->windowHandle())
        return;

    auto button = new KaWinTaskbarButton{m_window->windowHandle()};
    connect(button, &QObject::destroyed, this, [this] { m_taskbarProgress = nullptr; });
    m_taskbarButton = button;
    m_taskbarProgress = button->progress();
    m_taskbarProgress->setRange(0, 100);

    if (m_fileTransferManager)
        progressChanged(m_fileTransferManager->totalProgress());
}

void WindowsTaskbarProgress::progressChanged(int progress)
{
    if (!m_taskbarProgress)
        return;

    if (progress < 100)
    {
        m_taskbarProgress->setVisible(true);
        m_taskbarProgress->setValue(progress);
    }
    else
        m_taskbarProgress->setVisible(false);
}
