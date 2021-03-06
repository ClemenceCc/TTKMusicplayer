#include "musicbdquerylearnrequest.h"
#include "musicbdqueryinterface.h"
#include "musicsemaphoreloop.h"
#include "musicurlutils.h"
#include "musiccoreutils.h"

#include "qalg/qaeswrap.h"

MusicBDQueryLearnRequest::MusicBDQueryLearnRequest(QObject *parent)
    : MusicAbstractQueryRequest(parent)
{
    m_queryServer = QUERY_BD_INTERFACE;
}

void MusicBDQueryLearnRequest::startToSearch(QueryType type, const QString &text)
{
    if(!m_manager)
    {
        return;
    }

    Q_UNUSED(type);
    TTK_LOGGER_INFO(QString("%1 startToSearch %2").arg(getClassName()).arg(text));
    deleteAll();

    const QUrl &musicUrl = MusicUtils::Algorithm::mdII(BD_LEARN_URL, false).arg(text).arg(1).arg(30);
    m_interrupt = true;

    QNetworkRequest request;
    request.setUrl(musicUrl);
    request.setRawHeader("User-Agent", MusicUtils::Algorithm::mdII(BD_UA_URL, ALG_UA_KEY, false).toUtf8());
    MusicObject::setSslConfiguration(&request);

    m_reply = m_manager->get(request);
    connect(m_reply, SIGNAL(finished()), SLOT(downLoadFinished()));
    connect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)), SLOT(replyError(QNetworkReply::NetworkError)));
}

void MusicBDQueryLearnRequest::downLoadFinished()
{
    if(!m_reply)
    {
        deleteAll();
        return;
    }

    TTK_LOGGER_INFO(QString("%1 downLoadFinished").arg(getClassName()));
    Q_EMIT clearAllItems();
    m_musicSongInfos.clear();
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
            if(value["error_code"].toInt() == 22000 && value.contains("result"))
            {
                value = value["result"].toMap();
                const QVariantList &datas = value["items"].toList();
                for(const QVariant &var : qAsConst(datas))
                {
                    if(var.isNull())
                    {
                        continue;
                    }

                    value = var.toMap();
                    MusicObject::MusicSongInformation musicInfo;
                    musicInfo.m_singerName = MusicUtils::String::illegalCharactersReplaced(value["artist_name"].toString());
                    musicInfo.m_songName = MusicUtils::String::illegalCharactersReplaced(value["song_title"].toString());
                    musicInfo.m_songId = value["song_id"].toString();
                    musicInfo.m_albumId = value["album_id"].toString();

                    musicInfo.m_year = value["publishtime"].toString();
                    musicInfo.m_discNumber = "1";
                    musicInfo.m_trackNumber = value["album_no"].toString();

                    if(m_interrupt || !m_manager || m_stateCode != MusicObject::NetworkQuery) return;
                    readFromMusicLrcAttribute(&musicInfo);
                    if(m_interrupt || !m_manager || m_stateCode != MusicObject::NetworkQuery) return;
                    readFromMusicSongAttribute(&musicInfo);
                    if(m_interrupt || !m_manager || m_stateCode != MusicObject::NetworkQuery) return;

                    if(musicInfo.m_songAttrs.isEmpty())
                    {
                        continue;
                    }

                    MusicSearchedItem item;
                    item.m_songName = musicInfo.m_songName;
                    item.m_singerName = musicInfo.m_singerName;
                    item.m_type = mapQueryServerString();
                    Q_EMIT createSearchedItem(item);
                    m_musicSongInfos << musicInfo;
                }
            }
        }
    }

    Q_EMIT downLoadDataChanged(QString());
    deleteAll();
}

void MusicBDQueryLearnRequest::readFromMusicSongAttribute(MusicObject::MusicSongInformation *info)
{
    if(!m_manager)
    {
        return;
    }

    const QString &key = MusicUtils::Algorithm::mdII(BD_LEARN_DATA_URL, false).arg(info->m_songId).arg(MusicTime::timestamp());
    QString eKey = QString(QAesWrap().encryptCBC(key.toUtf8(), "4CC20A0C44FEB6FD", "2012061402992850"));
    MusicUtils::Url::urlEncode(eKey);
    const QUrl &musicUrl = MusicUtils::Algorithm::mdII(BD_LEARN_INFO_URL, false).arg(key).arg(eKey);

    QNetworkRequest request;
    request.setUrl(musicUrl);
    request.setRawHeader("User-Agent", MusicUtils::Algorithm::mdII(BD_UA_URL, ALG_UA_KEY, false).toUtf8());
    MusicObject::setSslConfiguration(&request);

    MusicSemaphoreLoop loop;
    QNetworkReply *reply = m_manager->get(request);
    QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    QObject::connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), &loop, SLOT(quit()));
    loop.exec();

    if(!reply || reply->error() != QNetworkReply::NoError)
    {
        return;
    }

    QJson::Parser parser;
    bool ok;

    const QVariant &data = parser.parse(reply->readAll(), &ok);
    if(ok)
    {
        QVariantMap value = data.toMap();
        if(value["error_code"].toInt() == 22000 && value.contains("result"))
        {
            value = value["result"].toMap();
            MusicObject::MusicSongAttribute attr;
            attr.m_bitrate = MB_128;
            attr.m_url = value["merge_link"].toString();
            attr.m_format = MP3_FILE_PREFIX;
            if(!findUrlFileSize(&attr)) return;

            info->m_songAttrs.append(attr);
        }
    }
}

void MusicBDQueryLearnRequest::readFromMusicLrcAttribute(MusicObject::MusicSongInformation *info)
{
    if(!m_manager)
    {
        return;
    }

    const QUrl &musicUrl = MusicUtils::Algorithm::mdII(BD_SONG_PATH_URL, false).arg(info->m_songId);

    QNetworkRequest request;
    request.setUrl(musicUrl);
    request.setRawHeader("User-Agent", MusicUtils::Algorithm::mdII(BD_UA_URL, ALG_UA_KEY, false).toUtf8());
    MusicObject::setSslConfiguration(&request);

    MusicSemaphoreLoop loop;
    QNetworkReply *reply = m_manager->get(request);
    QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    QObject::connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), &loop, SLOT(quit()));
    loop.exec();

    if(!reply || reply->error() != QNetworkReply::NoError)
    {
        return;
    }

    QJson::Parser parser;
    bool ok;
    const QVariant &data = parser.parse(reply->readAll(), &ok);
    if(ok)
    {
        QVariantMap value = data.toMap();
        if(value["errorCode"].toInt() == 22000 && value.contains("data"))
        {
            value = value["data"].toMap();
            const QVariantList &datas = value["songList"].toList();
            for(const QVariant &var : qAsConst(datas))
            {
                if(var.isNull())
                {
                    continue;
                }

                value = var.toMap();
                info->m_lrcUrl = value["lrcLink"].toString();
            }
        }
    }
}
