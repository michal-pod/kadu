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

#include "ka-win-jump-list.h"

#include <QtCore/QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <propkey.h>
#include <propvarutil.h>
#include <shobjidl_core.h>

namespace
{
HRESULT createLink(const QString &title, const QString &executable, const QStringList &arguments, IShellLink **link)
{
    auto result = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(link));
    if (FAILED(result))
        return result;

    auto const nativeExecutable = executable.toStdWString();
    auto const nativeArguments = arguments.join(' ').toStdWString();
    auto const nativeTitle = title.toStdWString();
    if (FAILED(result = (*link)->SetPath(nativeExecutable.c_str())) ||
        FAILED(result = (*link)->SetArguments(nativeArguments.c_str())) ||
        FAILED(result = (*link)->SetIconLocation(nativeExecutable.c_str(), 0)) ||
        FAILED(result = (*link)->SetDescription(nativeTitle.c_str())))
    {
        (*link)->Release();
        *link = nullptr;
        return result;
    }

    IPropertyStore *propertyStore = nullptr;
    result = (*link)->QueryInterface(IID_PPV_ARGS(&propertyStore));
    if (SUCCEEDED(result))
    {
        PROPVARIANT value;
        InitPropVariantFromString(nativeTitle.c_str(), &value);
        result = propertyStore->SetValue(PKEY_Title, value);
        if (SUCCEEDED(result))
            result = propertyStore->Commit();
        PropVariantClear(&value);
        propertyStore->Release();
    }

    if (FAILED(result))
    {
        (*link)->Release();
        *link = nullptr;
    }

    return result;
}

HRESULT createSeparator(IShellLink **link)
{
    auto result = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(link));
    if (FAILED(result))
        return result;

    IPropertyStore *propertyStore = nullptr;
    result = (*link)->QueryInterface(IID_PPV_ARGS(&propertyStore));
    if (SUCCEEDED(result))
    {
        PROPVARIANT value;
        InitPropVariantFromBoolean(TRUE, &value);
        result = propertyStore->SetValue(PKEY_AppUserModel_IsDestListSeparator, value);
        if (SUCCEEDED(result))
            result = propertyStore->Commit();
        PropVariantClear(&value);
        propertyStore->Release();
    }

    if (FAILED(result))
    {
        (*link)->Release();
        *link = nullptr;
    }

    return result;
}
}
#endif

class KaWinJumpList::Private
{
public:
#ifdef Q_OS_WIN
    bool comInitialized = false;
#endif
};

KaWinJumpListCategory::KaWinJumpListCategory(KaWinJumpList *jumpList) : QObject{jumpList}, m_jumpList{jumpList}
{
}

void KaWinJumpListCategory::clear()
{
    m_entries.clear();
}

void KaWinJumpListCategory::addLink(const QString &title, const QString &executable, const QStringList &arguments)
{
    m_entries.append(Entry{title, executable, arguments});
}

void KaWinJumpListCategory::addSeparator()
{
    m_entries.append(Entry{.separator = true});
}

void KaWinJumpListCategory::setVisible(bool visible)
{
    m_jumpList->synchronize(visible);
}

KaWinJumpList::KaWinJumpList(QObject *parent)
        : QObject{parent}, m_private{std::make_unique<Private>()}, m_tasks{new KaWinJumpListCategory{this}}
{
#ifdef Q_OS_WIN
    auto const result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    m_private->comInitialized = result == S_OK || result == S_FALSE;
#endif
}

KaWinJumpList::~KaWinJumpList()
{
#ifdef Q_OS_WIN
    if (m_private->comInitialized)
        CoUninitialize();
#endif
}

KaWinJumpListCategory *KaWinJumpList::tasks() const
{
    return m_tasks;
}

void KaWinJumpList::synchronize(bool visible)
{
#ifdef Q_OS_WIN
    ICustomDestinationList *destinationList = nullptr;
    auto result = CoCreateInstance(CLSID_DestinationList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&destinationList));
    if (FAILED(result))
    {
        qWarning() << "KaWinJumpList: creating destination list failed" << Qt::hex << static_cast<quint32>(result);
        return;
    }

    if (!visible)
    {
        result = destinationList->DeleteList(nullptr);
        destinationList->Release();
        if (FAILED(result))
            qWarning() << "KaWinJumpList: clearing destination list failed" << Qt::hex << static_cast<quint32>(result);
        return;
    }

    UINT maximumSlots = 0;
    IObjectArray *removedItems = nullptr;
    result = destinationList->BeginList(&maximumSlots, IID_PPV_ARGS(&removedItems));
    if (removedItems)
        removedItems->Release();
    if (FAILED(result))
    {
        qWarning() << "KaWinJumpList: beginning destination list update failed" << Qt::hex << static_cast<quint32>(result);
        destinationList->Release();
        return;
    }

    IObjectCollection *collection = nullptr;
    result = CoCreateInstance(CLSID_EnumerableObjectCollection, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&collection));
    if (SUCCEEDED(result))
    {
        for (auto const &entry : m_tasks->m_entries)
        {
            IShellLink *link = nullptr;
            result = entry.separator ? createSeparator(&link) : createLink(entry.title, entry.executable, entry.arguments, &link);
            if (FAILED(result))
                break;

            result = collection->AddObject(link);
            link->Release();
            if (FAILED(result))
                break;
        }
    }

    IObjectArray *items = nullptr;
    if (SUCCEEDED(result))
        result = collection->QueryInterface(IID_PPV_ARGS(&items));
    if (SUCCEEDED(result))
        result = destinationList->AddUserTasks(items);
    if (SUCCEEDED(result))
        result = destinationList->CommitList();
    else
        destinationList->AbortList();

    if (items)
        items->Release();
    if (collection)
        collection->Release();
    destinationList->Release();

    if (FAILED(result))
        qWarning() << "KaWinJumpList: updating destination list failed" << Qt::hex << static_cast<quint32>(result);
#else
    Q_UNUSED(visible)
#endif
}
