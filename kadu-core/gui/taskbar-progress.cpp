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

#include "taskbar-progress.h"
#include "taskbar-progress.moc"

#include "file-transfer/file-transfer-manager.h"

#include <QtWidgets/QWidget>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shobjidl_core.h>
#endif

TaskbarProgress::TaskbarProgress(FileTransferManager *fileTransferManager, QWidget *parent) : QObject{parent}
{
#ifdef Q_OS_WIN
    if (!parent)
        return;

    parent->window()->winId();   // force a native window handle

    auto const comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    m_comInitialized = comResult == S_OK || comResult == S_FALSE;
    if (SUCCEEDED(CoCreateInstance(
            CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_taskbarList))) &&
        FAILED(m_taskbarList->HrInit()))
    {
        m_taskbarList->Release();
        m_taskbarList = nullptr;
    }

    connect(fileTransferManager, SIGNAL(totalProgressChanged(int)), this, SLOT(progressChanged(int)));
    progressChanged(fileTransferManager->totalProgress());
#else
    Q_UNUSED(fileTransferManager);
#endif
}

TaskbarProgress::~TaskbarProgress()
{
#ifdef Q_OS_WIN
    if (m_taskbarList)
        m_taskbarList->Release();
    if (m_comInitialized)
        CoUninitialize();
#endif
}

void TaskbarProgress::progressChanged(int progress)
{
#ifdef Q_OS_WIN
    if (!m_taskbarList)
        return;

    auto window = static_cast<QWidget *>(parent())->window();
    auto const windowHandle = reinterpret_cast<HWND>(window->winId());
    if (progress < 100)
    {
        m_taskbarList->SetProgressState(windowHandle, TBPF_NORMAL);
        m_taskbarList->SetProgressValue(windowHandle, qBound(0, progress, 100), 100);
    }
    else
        m_taskbarList->SetProgressState(windowHandle, TBPF_NOPROGRESS);
#else
    Q_UNUSED(progress);
#endif
}
