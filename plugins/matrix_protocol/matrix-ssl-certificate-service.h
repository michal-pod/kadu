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

#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QObject>
#include <QtCore/QPointer>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <injeqt/injeqt.h>

class QNetworkReply;
class QSslError;
class SslCertificateManager;

class MatrixSslCertificateService final : public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE explicit MatrixSslCertificateService(QObject *parent = nullptr);
    ~MatrixSslCertificateService() override = default;

signals:
    void certificateError(const QString &requestHost);
    void certificateAccepted(const QString &requestHost);
    void certificateRejected(const QString &requestHost);

private:
    QPointer<SslCertificateManager> m_sslCertificateManager;
    QHash<QString, QSet<QString>> m_pendingRequestHosts;

    QString certificateKey(const QString &hostName, const QSslError &error) const;
    void completeCertificateDecision(const QString &key, bool accepted);
    void handleSslErrors(QNetworkReply *reply, const QList<QSslError> &errors);

private slots:
    INJEQT_SET void setSslCertificateManager(SslCertificateManager *sslCertificateManager);
    INJEQT_INIT void init();
};
