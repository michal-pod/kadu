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

#include "ka-win-thumbnail-toolbar.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QList>
#include <QtCore/QMetaObject>
#include <QtGui/QImage>
#include <QtGui/QPixmap>
#include <QtGui/QWindow>

#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shobjidl_core.h>

#include <array>
#include <cwchar>
#endif

namespace
{
constexpr int MaximumButtons = 7;
constexpr int FirstButtonId = 1;
}

class KaWinThumbnailToolBar::Private
{
public:
    QPointer<QWindow> window;
    QList<QPointer<KaWinThumbnailToolButton>> buttons;

#ifdef Q_OS_WIN
    ITaskbarList3 *taskbarList = nullptr;
    bool comInitialized = false;
    bool buttonsAdded = false;
    bool taskbarButtonCreated = false;
    UINT taskbarButtonCreatedMessage = RegisterWindowMessageW(L"TaskbarButtonCreated");
#endif
};

KaWinThumbnailToolBar::KaWinThumbnailToolBar(QWindow *window) : QObject{window}, m_private{std::make_unique<Private>()}
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

    QCoreApplication::instance()->installNativeEventFilter(this);
#endif

    setWindow(window);
}

KaWinThumbnailToolBar::~KaWinThumbnailToolBar()
{
#ifdef Q_OS_WIN
    QCoreApplication::instance()->removeNativeEventFilter(this);

    if (m_private->taskbarList)
        m_private->taskbarList->Release();
    if (m_private->comInitialized)
        CoUninitialize();
#endif
}

void KaWinThumbnailToolBar::setWindow(QWindow *window)
{
    if (m_private->window == window)
        return;

    setParent(window);
    m_private->window = window;

#ifdef Q_OS_WIN
    m_private->buttonsAdded = false;
    m_private->taskbarButtonCreated = false;
    synchronize();
#endif
}

void KaWinThumbnailToolBar::clear()
{
    for (auto const &button : m_private->buttons)
        delete button.data();
    m_private->buttons.clear();
    synchronize();
}

void KaWinThumbnailToolBar::addButton(KaWinThumbnailToolButton *button)
{
    if (!button)
        return;

    if (m_private->buttons.size() == MaximumButtons)
    {
        delete button;
        return;
    }

    button->setParent(this);
    m_private->buttons.append(button);
    synchronize();
}

bool KaWinThumbnailToolBar::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
#ifdef Q_OS_WIN
    if (eventType != "windows_generic_MSG")
        return false;

    auto const nativeMessage = static_cast<MSG *>(message);
    auto const windowHandle = m_private->window ? reinterpret_cast<HWND>(m_private->window->winId()) : nullptr;
    if (nativeMessage->hwnd != windowHandle)
        return false;

    if (nativeMessage->message == m_private->taskbarButtonCreatedMessage)
    {
        m_private->taskbarButtonCreated = true;
        qDebug() << "KaWinThumbnailToolBar: received TaskbarButtonCreated";
        synchronize();
        return false;
    }

    if (nativeMessage->message != WM_COMMAND || HIWORD(nativeMessage->wParam) != THBN_CLICKED)
        return false;

    auto const index = static_cast<int>(LOWORD(nativeMessage->wParam)) - FirstButtonId;
    if (index < 0 || index >= m_private->buttons.size())
        return false;

    auto const button = m_private->buttons.at(index);
    if (!button)
        return false;

    QMetaObject::invokeMethod(this, [button] {
        if (button)
            emit button->clicked();
    }, Qt::QueuedConnection);
    if (result)
        *result = 0;
    return true;
#else
    Q_UNUSED(eventType)
    Q_UNUSED(message)
    Q_UNUSED(result)
    return false;
#endif
}

void KaWinThumbnailToolBar::synchronize()
{
#ifdef Q_OS_WIN
    if (!m_private->taskbarList || !m_private->window || !m_private->taskbarButtonCreated)
        return;

    auto const windowHandle = reinterpret_cast<HWND>(m_private->window->winId());
    std::array<THUMBBUTTON, MaximumButtons> nativeButtons{};
    std::array<HICON, MaximumButtons> icons{};

    for (auto index = 0; index < MaximumButtons; ++index)
    {
        auto &nativeButton = nativeButtons.at(index);
        nativeButton.dwMask = THB_FLAGS;
        nativeButton.iId = FirstButtonId + index;
        nativeButton.dwFlags = THBF_HIDDEN;

        if (index >= m_private->buttons.size() || !m_private->buttons.at(index))
            continue;

        auto const button = m_private->buttons.at(index);
        nativeButton.dwMask |= THB_ICON | THB_TOOLTIP;
        nativeButton.dwFlags = button->m_dismissOnClick ? THBF_DISMISSONCLICK : THBF_ENABLED;

        auto const pixmap = button->m_icon.pixmap(QSize{16, 16});
        if (!pixmap.isNull())
        {
            icons.at(index) = pixmap.toImage().toHICON();
            nativeButton.hIcon = icons.at(index);
        }

        auto const toolTip = button->m_toolTip.toStdWString();
        wcsncpy_s(nativeButton.szTip, _countof(nativeButton.szTip), toolTip.c_str(), _TRUNCATE);
    }

    auto const result = m_private->buttonsAdded
        ? m_private->taskbarList->ThumbBarUpdateButtons(windowHandle, MaximumButtons, nativeButtons.data())
        : m_private->taskbarList->ThumbBarAddButtons(windowHandle, MaximumButtons, nativeButtons.data());
    if (SUCCEEDED(result))
        m_private->buttonsAdded = true;
    else
        qWarning() << "KaWinThumbnailToolBar: taskbar operation failed" << Qt::hex << static_cast<quint32>(result);

    for (auto icon : icons)
        if (icon)
            DestroyIcon(icon);
#endif
}

KaWinThumbnailToolButton::KaWinThumbnailToolButton(KaWinThumbnailToolBar *parent) : QObject{parent}
{
}

void KaWinThumbnailToolButton::setToolTip(const QString &toolTip)
{
    m_toolTip = toolTip;
}

void KaWinThumbnailToolButton::setIcon(const QIcon &icon)
{
    m_icon = icon;
}

void KaWinThumbnailToolButton::setDismissOnClick(bool dismissOnClick)
{
    m_dismissOnClick = dismissOnClick;
}
