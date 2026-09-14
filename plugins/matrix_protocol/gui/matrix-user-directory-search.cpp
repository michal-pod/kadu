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

#include "matrix-user-directory-search.h"
#include "matrix-user-directory-search.moc"

#include <Quotient/connection.h>
#include <Quotient/csapi/profile.h>
#include <Quotient/csapi/users.h>

#include <QtCore/QTimer>

#include <optional>

MatrixUserDirectorySearch::MatrixUserDirectorySearch(Quotient::Connection *connection, QObject *parent)
    : QObject{parent}, m_connection{connection}
{
    m_timer = new QTimer{this};
    m_timer->setSingleShot(true);
    m_timer->setInterval(300);
    connect(m_timer, &QTimer::timeout, this, &MatrixUserDirectorySearch::startSearch);
}

QString MatrixUserDirectorySearch::completeUserId(const QString &text)
{
    const auto userId = text.trimmed();
    if (!userId.startsWith(QLatin1Char('@')))
        return {};

    const auto separator = userId.indexOf(QLatin1Char(':'), 2);
    if (separator <= 1 || separator == userId.size() - 1)
        return {};

    for (const auto character : userId)
        if (character.isSpace() || character.category() == QChar::Other_Control)
            return {};
    return userId;
}

void MatrixUserDirectorySearch::setQuery(const QString &query)
{
    const auto normalized = query.trimmed();
    if (m_query == normalized)
        return;

    m_query = normalized;
    ++m_generation;
    m_timer->stop();
    m_results.clear();
    m_error.clear();
    m_loading = false;
    m_limited = false;

    const auto incompleteUserId = m_query.startsWith(QLatin1Char('@')) && completeUserId(m_query).isEmpty();
    if (m_query.size() >= 2 && !incompleteUserId)
    {
        m_loading = true;
        m_timer->start();
    }
    emit changed();
}

QString MatrixUserDirectorySearch::query() const
{
    return m_query;
}

QVector<MatrixUserDirectoryResult> MatrixUserDirectorySearch::results() const
{
    return m_results;
}

bool MatrixUserDirectorySearch::loading() const
{
    return m_loading;
}

bool MatrixUserDirectorySearch::limited() const
{
    return m_limited;
}

QString MatrixUserDirectorySearch::error() const
{
    return m_error;
}

void MatrixUserDirectorySearch::startSearch()
{
    if (!m_connection || !m_connection->isLoggedIn() || !m_connection->isOnline() || m_query.size() < 2)
    {
        m_loading = false;
        m_error = tr("The Matrix user directory is unavailable while the account is offline.");
        emit changed();
        return;
    }

    const auto query = m_query;
    const auto generation = m_generation;
    if (!completeUserId(query).isEmpty())
    {
        m_connection->callApi<Quotient::GetUserProfileJob>(query)
            .then(this, [this, query, generation](Quotient::GetUserProfileJob *job) {
                if (generation != m_generation || query != m_query)
                    return;

                m_results = {{query, job->displayname(), job->avatarUrl()}};
                m_loading = false;
                m_limited = false;
                m_error.clear();
                emit changed();
            }, [this, query, generation](Quotient::GetUserProfileJob *job) {
                if (generation != m_generation || query != m_query)
                    return;

                m_results.clear();
                m_loading = false;
                m_limited = false;
                m_error = job ? job->errorString() : tr("The Matrix user profile lookup failed.");
                emit changed();
            });
        return;
    }

    m_connection->callApi<Quotient::SearchUserDirectoryJob>(query, std::optional<int>{30})
        .then(this, [this, query, generation](Quotient::SearchUserDirectoryJob *job) {
            if (generation != m_generation || query != m_query)
                return;

            m_results.clear();
            m_results.reserve(job->results().size());
            for (const auto &user : job->results())
                if (!user.userId.isEmpty())
                    m_results.append({user.userId, user.displayName, user.avatarUrl});
            m_loading = false;
            m_limited = job->limited();
            m_error.clear();
            emit changed();
        }, [this, query, generation](Quotient::SearchUserDirectoryJob *job) {
            if (generation != m_generation || query != m_query)
                return;

            m_results.clear();
            m_loading = false;
            m_limited = false;
            m_error = job ? job->errorString() : tr("The Matrix user directory search failed.");
            emit changed();
        });
}
