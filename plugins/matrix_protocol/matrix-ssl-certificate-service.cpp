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

#include "matrix-ssl-certificate-service.h"
#include "matrix-ssl-certificate-service.moc"

#include "ssl/ssl-certificate-manager.h"

#include <Quotient/networkaccessmanager.h>

#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QSslCertificate>
#include <QtNetwork/QSslError>

MatrixSslCertificateService::MatrixSslCertificateService(QObject *parent) : QObject{parent}
{
}

void MatrixSslCertificateService::setSslCertificateManager(SslCertificateManager *sslCertificateManager)
{
    m_sslCertificateManager = sslCertificateManager;
}

void MatrixSslCertificateService::init()
{
    connect(Quotient::NetworkAccessManager::instance(), &QNetworkAccessManager::sslErrors,
            this, &MatrixSslCertificateService::handleSslErrors);
}

QString MatrixSslCertificateService::certificateKey(const QString &hostName, const QSslError &error) const
{
    return hostName + QLatin1Char('\x1f') + QString::fromLatin1(error.certificate().toDer().toHex());
}

void MatrixSslCertificateService::handleSslErrors(QNetworkReply *reply, const QList<QSslError> &errors)
{
    if (!reply || errors.isEmpty() || !m_sslCertificateManager)
        return;

    const auto replyHost = reply->url().host();
    const auto requestHost = reply->request().url().host();
    const auto certificateHost = replyHost.isEmpty() ? requestHost : replyHost;
    if (certificateHost.isEmpty())
        return;

    if (m_sslCertificateManager->acceptCertificate(certificateHost, errors.constFirst()))
    {
        reply->ignoreSslErrors(errors);
        return;
    }

    const auto key = certificateKey(certificateHost, errors.constFirst());
    const auto affectedHost = requestHost.isEmpty() ? certificateHost : requestHost;
    const auto alreadyPending = m_pendingRequestHosts.contains(key);
    m_pendingRequestHosts[key].insert(affectedHost);
    emit certificateError(affectedHost);
    if (alreadyPending)
        return;

    const QPointer<MatrixSslCertificateService> service{this};
    m_sslCertificateManager->askForCertificateAcceptance(
        certificateHost, errors.constFirst().certificate(), errors,
        [service, key] {
            if (service)
                service->completeCertificateDecision(key, true);
        },
        [service, key] {
            if (service)
                service->completeCertificateDecision(key, false);
        });
}

void MatrixSslCertificateService::completeCertificateDecision(const QString &key, bool accepted)
{
    const auto requestHosts = m_pendingRequestHosts.take(key);
    for (const auto &requestHost : requestHosts)
    {
        if (accepted)
            emit certificateAccepted(requestHost);
        else
            emit certificateRejected(requestHost);
    }
}
