/*
 * %kadu copyright begin%
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

#include "ka-win-taskbar-button.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QEvent>
#include <QtCore/QPointer>
#include <QtGui/QPlatformSurfaceEvent>
#include <QtGui/QWindow>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shobjidl_core.h>
#endif

class KaWinTaskbarButton::Private
{
public:
    QPointer<QWindow> window;

#ifdef Q_OS_WIN
    ITaskbarList3 *taskbarList = nullptr;
    bool comInitialized = false;
    bool nativeEventFilterInstalled = false;
    bool taskbarButtonCreated = false;
    UINT taskbarButtonCreatedMessage = RegisterWindowMessageW(L"TaskbarButtonCreated");
#endif
};

KaWinTaskbarProgress::KaWinTaskbarProgress(KaWinTaskbarButton *button) : m_button{button}
{
}

void KaWinTaskbarProgress::setRange(int minimum, int maximum)
{
    m_minimum = minimum;
    m_maximum = maximum;
    m_button->synchronize();
}

void KaWinTaskbarProgress::setVisible(bool visible)
{
    m_visible = visible;
    m_button->synchronize();
}

void KaWinTaskbarProgress::setValue(int value)
{
    m_value = value;
    m_button->synchronize();
}

KaWinTaskbarButton::KaWinTaskbarButton(QWindow *window)
        : QObject{window}, m_private{std::make_unique<Private>()}, m_progress{this}
{
#ifdef Q_OS_WIN
    auto const result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    m_private->comInitialized = result == S_OK || result == S_FALSE;

    if (SUCCEEDED(CoCreateInstance(
            CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&m_private->taskbarList))) &&
        FAILED(m_private->taskbarList->HrInit()))
    {
        m_private->taskbarList->Release();
        m_private->taskbarList = nullptr;
    }

    if (auto application = QCoreApplication::instance())
    {
        application->installNativeEventFilter(this);
        m_private->nativeEventFilterInstalled = true;
    }
#endif

    setWindow(window);
}

KaWinTaskbarButton::~KaWinTaskbarButton()
{
#ifdef Q_OS_WIN
    if (m_private->nativeEventFilterInstalled)
        if (auto application = QCoreApplication::instance())
            application->removeNativeEventFilter(this);

    if (m_private->taskbarList)
        m_private->taskbarList->Release();
    if (m_private->comInitialized)
        CoUninitialize();
#endif
}

void KaWinTaskbarButton::setWindow(QWindow *window)
{
    if (m_private->window == window)
        return;

    if (m_private->window)
        m_private->window->removeEventFilter(this);

    m_private->window = window;
    setParent(window);

    if (m_private->window)
        m_private->window->installEventFilter(this);

#ifdef Q_OS_WIN
    m_private->taskbarButtonCreated = false;
#endif
    synchronize();
}

KaWinTaskbarProgress *KaWinTaskbarButton::progress()
{
    return &m_progress;
}

bool KaWinTaskbarButton::eventFilter(QObject *watched, QEvent *event)
{
#ifdef Q_OS_WIN
    if (watched == m_private->window && event->type() == QEvent::PlatformSurface)
    {
        auto const platformSurfaceEvent = static_cast<QPlatformSurfaceEvent *>(event);
        if (platformSurfaceEvent->surfaceEventType() == QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed)
            m_private->taskbarButtonCreated = false;
    }
#else
    Q_UNUSED(watched)
    Q_UNUSED(event)
#endif

    return QObject::eventFilter(watched, event);
}

bool KaWinTaskbarButton::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    Q_UNUSED(result)

    if (eventType != "windows_generic_MSG")
        return false;

    auto const nativeMessage = static_cast<MSG *>(message);
    if (!m_private->window || !m_private->window->handle())
        return false;

    auto const windowHandle = reinterpret_cast<HWND>(m_private->window->winId());
    if (nativeMessage->hwnd != windowHandle || nativeMessage->message != m_private->taskbarButtonCreatedMessage)
        return false;

    m_private->taskbarButtonCreated = true;
    qDebug() << "KaWinTaskbarButton: received TaskbarButtonCreated";
    synchronize();
#else
    Q_UNUSED(eventType)
    Q_UNUSED(message)
    Q_UNUSED(result)
#endif
    return false;
}

void KaWinTaskbarButton::synchronize()
{
#ifdef Q_OS_WIN
    if (!m_private->taskbarList || !m_private->window || !m_private->window->handle() ||
        !m_private->taskbarButtonCreated)
        return;

    auto const windowHandle = reinterpret_cast<HWND>(m_private->window->winId());
    HRESULT result = S_OK;
    if (!m_progress.m_visible || m_progress.m_maximum <= m_progress.m_minimum)
        result = m_private->taskbarList->SetProgressState(windowHandle, TBPF_NOPROGRESS);
    else
    {
        auto const value = qBound(m_progress.m_minimum, m_progress.m_value, m_progress.m_maximum) - m_progress.m_minimum;
        auto const range = m_progress.m_maximum - m_progress.m_minimum;
        result = m_private->taskbarList->SetProgressState(windowHandle, TBPF_NORMAL);
        if (SUCCEEDED(result))
            result = m_private->taskbarList->SetProgressValue(windowHandle, value, range);
    }

    if (FAILED(result))
        qWarning() << "KaWinTaskbarButton: taskbar operation failed" << Qt::hex << static_cast<quint32>(result);
#endif
}
