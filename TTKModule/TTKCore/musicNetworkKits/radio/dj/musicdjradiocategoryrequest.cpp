#include "musicdjradiocategoryrequest.h"

MusicDJRadioCategoryRequest::MusicDJRadioCategoryRequest(QObject *parent)
    : MusicAbstractDJRadioRequest(parent)
{

}

void MusicDJRadioCategoryRequest::startToDownload()
{
    deleteAll();
    m_interrupt = true;

    QNetworkRequest request;
    if(!m_manager || m_stateCode != MusicObject::NetworkQuery) return;
    const QByteArray &parameter = makeTokenQueryUrl(&request,
                      MusicUtils::Algorithm::mdII(DJ_CATEGORY_URL, false),
                      QString("{}"));
    if(!m_manager || m_stateCode != MusicObject::NetworkQuery) return;
    MusicObject::setSslConfiguration(&request);

    m_reply = m_manager->post(request, parameter);
    connect(m_reply, SIGNAL(finished()), SLOT(downLoadFinished()));
    connect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)), SLOT(replyError(QNetworkReply::NetworkError)));
}

void MusicDJRadioCategoryRequest::downLoadFinished()
{
    if(!m_reply)
    {
        deleteAll();
        return;
    }

    m_items.clear();
    m_interrupt = false;

    if(m_reply->error() == QNetworkReply::NoError)
    {
        const QByteArray &bytes = m_reply->readAll();

        QJson::Parser parser;
        bool ok;
        const QVariant &data = parser.parse(bytes, &ok);
        if(ok)
        {
            QVariantMap value = data.toMap();
            if(value["code"].toInt() == 200 && value.contains("categories"))
            {
                const QVariantList &datas = value["categories"].toList();
                for(const QVariant &var : qAsConst(datas))
                {
                    if(m_interrupt) return;

                    if(var.isNull())
                    {
                        continue;
                    }

                    value = var.toMap();
                    MusicResultsItem item;
                    item.m_name = value["name"].toString();
                    item.m_id = value["id"].toString();
                    item.m_coverUrl = value["picMacUrl"].toString();
                    m_items << item;
                }
            }
        }
    }

    Q_EMIT downLoadDataChanged(QString());
    deleteAll();
}
