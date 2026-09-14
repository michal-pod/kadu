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

#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QUrl>
#include <QtCore/QVector>

class QTimer;

namespace Quotient
{
class Connection;
}

struct MatrixUserDirectoryResult
{
    QString userId;
    QString displayName;
    QUrl avatarUrl;
};

class MatrixUserDirectorySearch final : public QObject
{
    Q_OBJECT

public:
    explicit MatrixUserDirectorySearch(Quotient::Connection *connection, QObject *parent = nullptr);

    static QString completeUserId(const QString &text);

    void setQuery(const QString &query);
    QString query() const;
    QVector<MatrixUserDirectoryResult> results() const;
    bool loading() const;
    bool limited() const;
    QString error() const;

signals:
    void changed();

private:
    QPointer<Quotient::Connection> m_connection;
    QTimer *m_timer = nullptr;
    QString m_query;
    QVector<MatrixUserDirectoryResult> m_results;
    QString m_error;
    int m_generation = 0;
    bool m_loading = false;
    bool m_limited = false;

    void startSearch();
};
