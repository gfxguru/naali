#include "DebugOperatorNew.h"
#include <boost/algorithm/string.hpp>
#include <QList>

#include "AssetCache.h"
#include "AssetAPI.h"
#include "IAsset.h"
#include "LoggingFunctions.h"

#include <QUrl>
#include <QFile>
#include <QDataStream>
#include <QFileInfo>
#include <QScopedPointer>

#include "MemoryLeakCheck.h"

DEFINE_POCO_LOGGING_FUNCTIONS("AssetCache")

QString SanitateAssetRefForCache(QString assetRef)
{ 
    assetRef.replace("/", "_");
    assetRef.replace("\\", "_");
    assetRef.replace(":", "_");
    assetRef.replace("*", "_");
    assetRef.replace("?", "_");
    assetRef.replace("\"", "_");
    assetRef.replace("'", "_");
    assetRef.replace("<", "_");
    assetRef.replace(">", "_");
    assetRef.replace("|", "_");
    return assetRef;
}

AssetCache::AssetCache(AssetAPI *owner, QString assetCacheDirectory) : 
    QNetworkDiskCache(owner),
    assetAPI(owner),
    cacheDirectory(GuaranteeTrailingSlash(assetCacheDirectory))
{
    LogInfo("Using AssetCache in directory \"" + assetCacheDirectory.toStdString() + "\".");

    setCacheDirectory(cacheDirectory);
    QDir assetDir(cacheDirectory);
    if (!assetDir.exists("data"))
        assetDir.mkdir("data");
    assetDataDir = QDir(cacheDirectory + "data");
    if (!assetDir.exists("metadata"))
        assetDir.mkdir("metadata");
    assetMetaDataDir = QDir(cacheDirectory + "metadata");
}

// QNetworkDiskCache overrides. Don't call directly, used by QNetworkAccessManager.

QIODevice* AssetCache::data(const QUrl &url)
{
    QScopedPointer<QFile> dataFile;
    QString absoluteDataFile = GetAbsoluteFilePath(false, url);
    if (QFile::exists(absoluteDataFile))
    {
        dataFile.reset(new QFile(absoluteDataFile));
        if (!dataFile->open(QIODevice::ReadWrite))
        {
            dataFile.reset();
            return 0;
        }
    }
    // It is the callers responsibility to delete this ptr as said by the Qt docs.
    // This will most likely happen when QNetworkReply->deleteLater() is called, meaning next qt mainloop cycle.
    return dataFile.take();
}

void AssetCache::insert(QIODevice* device)
{
    // We own this ptr from prepare()
    QHashIterator<QUrl, QFile*> it(preparedItems);
    while (it.hasNext())
    {
        it.next();
        if (it.value() == device)
        {
            preparedItems.remove(it.key());
            break;
        }
    }
    device->close();
    device->deleteLater();
}

QIODevice* AssetCache::prepare(const QNetworkCacheMetaData &metaData)
{
    if (!WriteMetadata(GetAbsoluteFilePath(true, metaData.url()), metaData))
        return 0;
    QScopedPointer<QFile> dataFile(new QFile(GetAbsoluteFilePath(false, metaData.url())));
    if (!dataFile->open(QIODevice::ReadWrite))
    {
        remove(metaData.url());
        dataFile.reset();
        return 0;
    }
    // Take ownership of the ptr
    QFile *dataPtr = dataFile.take();
    preparedItems[metaData.url()] = dataPtr;
    return dataPtr;
}

bool AssetCache::remove(const QUrl &url)
{
    // remove() is also used for canceling insertion after prepare()
    // we need to delete the QFile* ptr also in these cases
    // note: this is not a common operation
    QHashIterator<QUrl, QFile*> it(preparedItems);
    while (it.hasNext())
    {
        it.next();
        if (it.key() == url)
        {
            delete it.value();
            preparedItems.remove(it.key());
            break;
        }
    }

    // Remove the actual files related to this url
    bool success = true;
    QString absoluteMetaDataFile = GetAbsoluteFilePath(true,url);
    if (QFile::exists(absoluteMetaDataFile))
        success = QFile::remove(absoluteMetaDataFile);
    if (!success)
        return false;
    QString absoluteDataFile = GetAbsoluteFilePath(false, url);
    if (QFile::exists(absoluteDataFile))
        success = QFile::remove(absoluteDataFile);
    return success;
}

QNetworkCacheMetaData AssetCache::metaData(const QUrl &url)
{
    QNetworkCacheMetaData resultMetaData;
    QString absoluteMetaDataFile = GetAbsoluteFilePath(true, url);
    if (QFile::exists(absoluteMetaDataFile))
    {
        QFile metaDataFile(absoluteMetaDataFile);
        if (metaDataFile.open(QIODevice::ReadOnly))
        {
            QDataStream metaDataStream(&metaDataFile);
            metaDataStream >> resultMetaData;
            metaDataFile.close();
        }
    }
    return resultMetaData;
}

void AssetCache::updateMetaData(const QNetworkCacheMetaData &metaData)
{
    const QNetworkCacheMetaData oldMetaData = this->metaData(metaData.url());
    if (oldMetaData.isValid())
        if (oldMetaData != metaData)
            WriteMetadata(GetAbsoluteFilePath(true, metaData.url()), metaData);
}

void AssetCache::clear()
{
    ClearAssetCache();
}

qint64 AssetCache::expire()
{
    // Skip keeping cache at some static size, unlimited for now.
    return maximumCacheSize() / 2;
}

// Public Slots

QString AssetCache::GetDiskSource(const QString &assetRef)
{
    // Deny http:// and https:// asset references to be gotten from cache
    // as the QAccessManager will request it from the overrides above later!
    // You can get the path if you ask directly as a url.
    if (assetRef.startsWith("http://") || assetRef.startsWith("https://"))
        return "";
    return GetDiskSource(QUrl(assetRef, QUrl::TolerantMode));
}

QString AssetCache::GetDiskSource(const QUrl &assetUrl)
{
    QString absolutePath = assetDataDir.absolutePath() + "/" + SanitateAssetRefForCache(assetUrl.toString());
    if (QFile::exists(absolutePath))
        return absolutePath;
    return "";
}

QString AssetCache::GetDiskSourceByContentHash(const QString &contentHash)
{
    /// \todo Implement.
    LogWarning("AssetCache::GetDiskSourceByContentHash not implemented yet.");
    return "";
}

QString AssetCache::GetCacheDirectory() const
{
    return GuaranteeTrailingSlash(assetDataDir.absolutePath());
}

QString AssetCache::StoreAsset(AssetPtr asset)
{
    std::vector<u8> data;
    asset->SerializeTo(data);
    return StoreAsset(&data[0], data.size(), asset->Name(), asset->ContentHash()); /// \todo Content hash can mismatch here.
}

QString AssetCache::StoreAsset(const u8 *data, size_t numBytes, const QString &assetName, const QString &assetContentHash)
{
    QString absolutePath = GetAbsoluteDataFilePath(assetName);
    bool success = SaveAssetFromMemoryToFile(data, numBytes, absolutePath.toStdString().c_str());
    if (success)
        return absolutePath;
    return "";
}

void AssetCache::DeleteAsset(const QString &assetRef)
{
    DeleteAsset(QUrl(assetRef, QUrl::TolerantMode));
}

void AssetCache::DeleteAsset(const QUrl &assetUrl)
{
    if (!remove(assetUrl))
        LogWarning("AssetCache::DeleteAsset Failed to delete asset " + assetUrl.toString().toStdString());
}

void AssetCache::ClearAssetCache()
{
    ClearDirectory(assetDataDir.absolutePath());
    ClearDirectory(assetMetaDataDir.absolutePath());
}

// Private Slots

bool AssetCache::WriteMetadata(const QString &filePath, const QNetworkCacheMetaData &metaData)
{
    QFile metaDataFile(filePath);
    if (!metaDataFile.open(QIODevice::WriteOnly))
    {
        LogError("AssetCache::WriteMetadata Could not open metadata file: " + filePath.toStdString());
        return false;
    }
    if (!metaDataFile.resize(0))
    {
        LogError("AssetCache::WriteMetadata Could not reset metadata file: " + filePath.toStdString());
        return false;
    }

    QDataStream metaDataStream(&metaDataFile);
    metaDataStream << metaData;
    metaDataFile.close();
    return true;
}

QString AssetCache::GetAbsoluteFilePath(bool isMetaData, const QUrl &url)
{
    QString subDir = isMetaData ? "metadata" : "data";
    QDir assetDir(cacheDirectory + subDir);
    QString absolutePath = assetDir.absolutePath() + "/" + SanitateAssetRefForCache(url.toString());
    if (isMetaData)
        absolutePath.append(".metadata");
    return absolutePath;
}

QString AssetCache::GetAbsoluteDataFilePath(const QString &filename)
{
    return assetDataDir.absolutePath() + "/" + SanitateAssetRefForCache(filename);
}

void AssetCache::ClearDirectory(const QString &absoluteDirPath)
{
    QDir targetDir(absoluteDirPath);
    if (!targetDir.exists())
    {
        LogWarning("AssetCache::ClearDirectory called with non existing directory path.");
        return;
    }
    QFileInfoList entries = targetDir.entryInfoList(QDir::Files|QDir::NoSymLinks|QDir::NoDotAndDotDot);
    foreach(QFileInfo entry, entries)
    {
        if (entry.isFile())
        {
            if (!targetDir.remove(entry.fileName()))
                LogWarning("AssetCache::ClearDirectory could not remove file " + entry.absoluteFilePath().toStdString());
        }
    }
}
