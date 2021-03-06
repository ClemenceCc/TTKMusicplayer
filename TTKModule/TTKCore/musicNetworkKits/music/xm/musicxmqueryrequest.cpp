#include "musicxmqueryrequest.h"

MusicXMQueryRequest::MusicXMQueryRequest(QObject *parent)
    : MusicAbstractQueryRequest(parent)
{
    m_queryServer = QUERY_XM_INTERFACE;
    m_pageSize = 30;
}

void MusicXMQueryRequest::startToSearch(QueryType type, const QString &text)
{
    if(!m_manager)
    {
        return;
    }

    TTK_LOGGER_INFO(QString("%1 startToSearch %2").arg(getClassName()).arg(text));
    m_currentType = type;
    m_searchText = text.trimmed();

    Q_EMIT clearAllItems();
    m_musicSongInfos.clear();

    startToPage(0);
}

void MusicXMQueryRequest::startToPage(int offset)
{
    if(!m_manager)
    {
        return;
    }

    TTK_LOGGER_INFO(QString("%1 startToPage %2").arg(getClassName()).arg(offset));
    deleteAll();

    m_interrupt = true;
    m_pageTotal = 0;
    m_pageIndex = offset;

    QNetworkRequest request;
    if(!m_manager || m_stateCode != MusicObject::NetworkQuery) return;
    makeTokenQueryUrl(&request, true,
                      MusicUtils::Algorithm::mdII(XM_SONG_SEARCH_DATA_URL, false).arg(m_searchText).arg(offset + 1).arg(m_pageSize),
                      MusicUtils::Algorithm::mdII(XM_SONG_SEARCH_URL, false));
    if(!m_manager || m_stateCode != MusicObject::NetworkQuery) return;
    MusicObject::setSslConfiguration(&request);

    m_reply = m_manager->get(request);
    connect(m_reply, SIGNAL(finished()), SLOT(downLoadFinished()));
    connect(m_reply, SIGNAL(error(QNetworkReply::NetworkError)), SLOT(replyError(QNetworkReply::NetworkError)));
}

void MusicXMQueryRequest::startToSingleSearch(const QString &text)
{
    if(!m_manager)
    {
        return;
    }

    TTK_LOGGER_INFO(QString("%1 startToSingleSearch %2").arg(getClassName()).arg(text));

    m_interrupt = true;

    QNetworkRequest request;
    if(!m_manager || m_stateCode != MusicObject::NetworkQuery) return;
    makeTokenQueryUrl(&request, false,
                      MusicUtils::Algorithm::mdII(XM_SONG_INFO_DATA_URL, false).arg(text),
                      MusicUtils::Algorithm::mdII(XM_SONG_INFO_URL, false));
    if(!m_manager || m_stateCode != MusicObject::NetworkQuery) return;
    MusicObject::setSslConfiguration(&request);

    QNetworkReply *reply = m_manager->get(request);
    connect(reply, SIGNAL(finished()), SLOT(singleDownLoadFinished()));
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), SLOT(replyError(QNetworkReply::NetworkError)));
}

void MusicXMQueryRequest::downLoadFinished()
{
    if(!m_reply || !m_manager)
    {
        deleteAll();
        return;
    }

    TTK_LOGGER_INFO(QString("%1 downLoadFinished").arg(getClassName()));
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
            if(value.contains("data"))
            {
                value = value["data"].toMap();
                value = value["data"].toMap();

                const QVariantMap &pagingVO = value["pagingVO"].toMap();
                m_pageTotal = pagingVO["count"].toInt();

                const QVariantList &datas = value["songs"].toList();
                for(const QVariant &var : qAsConst(datas))
                {
                    if(var.isNull())
                    {
                        continue;
                    }

                    value = var.toMap();
                    MusicObject::MusicSongInformation musicInfo;
                    musicInfo.m_singerName = MusicUtils::String::illegalCharactersReplaced(value["singers"].toString());
                    musicInfo.m_songName = MusicUtils::String::illegalCharactersReplaced(value["songName"].toString());
                    musicInfo.m_timeLength = MusicTime::msecTime2LabelJustified(value["length"].toInt());

                    musicInfo.m_songId = value["songId"].toString();
                    musicInfo.m_artistId = value["artistId"].toString();
                    musicInfo.m_albumId = value["albumId"].toString();

                    musicInfo.m_year = QString();
                    musicInfo.m_discNumber = "0";
                    musicInfo.m_trackNumber = value["track"].toString();

                    if(!m_querySimplify)
                    {
                        const QVariantMap &lyricInfo = value["lyricInfo"].toMap();
                        musicInfo.m_lrcUrl = lyricInfo["lyricFile"].toString();

                        musicInfo.m_smallPicUrl = value["albumLogo"].toString();
                        musicInfo.m_albumName = MusicUtils::String::illegalCharactersReplaced(value["albumName"].toString());

                        if(m_interrupt || !m_manager || m_stateCode != MusicObject::NetworkQuery) return;
                        readFromMusicSongAttribute(&musicInfo, value["listenFiles"], m_searchQuality, m_queryAllRecords);
                        if(m_interrupt || !m_manager || m_stateCode != MusicObject::NetworkQuery) return;

                        if(musicInfo.m_songAttrs.isEmpty())
                        {
                            continue;
                        }

                        MusicSearchedItem item;
                        item.m_songName = musicInfo.m_songName;
                        item.m_singerName = musicInfo.m_singerName;
                        item.m_albumName = musicInfo.m_albumName;
                        item.m_time = musicInfo.m_timeLength;
                        item.m_type = mapQueryServerString();
                        Q_EMIT createSearchedItem(item);
                    }
                    m_musicSongInfos << musicInfo;
                }
            }
        }
    }

    Q_EMIT downLoadDataChanged(QString());
    deleteAll();
}

void MusicXMQueryRequest::singleDownLoadFinished()
{
    QNetworkReply *reply = TTKObject_cast(QNetworkReply*, QObject::sender());

    TTK_LOGGER_INFO(QString("%1 singleDownLoadFinished").arg(getClassName()));
    Q_EMIT clearAllItems();
    m_musicSongInfos.clear();
    m_interrupt = false;

    if(reply && m_manager &&reply->error() == QNetworkReply::NoError)
    {
        const QByteArray &bytes = reply->readAll();

        QJson::Parser parser;
        bool ok;
        const QVariant &data = parser.parse(bytes, &ok);
        if(ok)
        {
            QVariantMap value = data.toMap();
            if(value.contains("data"))
            {
                value = value["data"].toMap();
                value = value["data"].toMap();
                value = value["songDetail"].toMap();

                MusicObject::MusicSongInformation musicInfo;
                musicInfo.m_singerName = MusicUtils::String::illegalCharactersReplaced(value["singers"].toString());
                musicInfo.m_songName = MusicUtils::String::illegalCharactersReplaced(value["songName"].toString());
                musicInfo.m_timeLength = MusicTime::msecTime2LabelJustified(value["length"].toInt());

                musicInfo.m_songId = value["songId"].toString();
                musicInfo.m_artistId = value["artistId"].toString();
                musicInfo.m_albumId = value["albumId"].toString();

                musicInfo.m_smallPicUrl = value["albumLogo"].toString();
                musicInfo.m_albumName = MusicUtils::String::illegalCharactersReplaced(value["albumName"].toString());

                musicInfo.m_year = QString();
                musicInfo.m_discNumber = "0";
                musicInfo.m_trackNumber = value["track"].toString();

                const QVariantMap &lyricInfo = value["lyricInfo"].toMap();
                musicInfo.m_lrcUrl = lyricInfo["lyricFile"].toString();

                if(m_interrupt || !m_manager || m_stateCode != MusicObject::NetworkQuery) return;
                readFromMusicSongAttribute(&musicInfo, value["listenFiles"], m_searchQuality, m_queryAllRecords);
                if(m_interrupt || !m_manager || m_stateCode != MusicObject::NetworkQuery) return;

                if(!musicInfo.m_songAttrs.isEmpty())
                {
                    MusicSearchedItem item;
                    item.m_songName = musicInfo.m_songName;
                    item.m_singerName = musicInfo.m_singerName;
                    item.m_albumName = musicInfo.m_albumName;
                    item.m_time = musicInfo.m_timeLength;
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
