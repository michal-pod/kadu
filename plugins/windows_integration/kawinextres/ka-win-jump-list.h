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

#pragma once

#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <memory>

class KaWinJumpList;

class KaWinJumpListCategory : public QObject
{
public:
    explicit KaWinJumpListCategory(KaWinJumpList *jumpList);

    void clear();
    void addLink(const QString &title, const QString &executable, const QStringList &arguments);
    void addSeparator();
    void setVisible(bool visible);

private:
    struct Entry
    {
        QString title;
        QString executable;
        QStringList arguments;
        bool separator = false;
    };

    KaWinJumpList *m_jumpList;
    QList<Entry> m_entries;

    friend class KaWinJumpList;
};

class KaWinJumpList : public QObject
{
public:
    explicit KaWinJumpList(QObject *parent = nullptr);
    ~KaWinJumpList() override;

    KaWinJumpListCategory *tasks() const;

private:
    class Private;
    std::unique_ptr<Private> m_private;
    KaWinJumpListCategory *m_tasks;

    void synchronize(bool visible);

    friend class KaWinJumpListCategory;
};
