#include "ImageDownloader.h"

#include "megaapi.h"

#include <QNetworkRequest>

namespace
{
constexpr int DefaultTimeout = 30000;
constexpr int StatusCodeOK = 200;
}

ImageDownloader::ImageDownloader(QObject* parent)
    : ImageDownloader(DefaultTimeout, parent)
{
}

ImageDownloader::ImageDownloader(unsigned int timeout, QObject* parent)
    : QObject(parent)
    , mManager(new QNetworkAccessManager(this))
    , mTimeout(timeout)
{
    connect(mManager.get(), &QNetworkAccessManager::finished,
            this, &ImageDownloader::onRequestImgFinished);
}

void ImageDownloader::downloadImage(const QString& imageUrl,
                                    int width,
                                    int height,
                                    QImage::Format format)
{
    QUrl url(imageUrl);
    if (!url.isValid())
    {
        return;
    }

    QNetworkRequest request(url);
    request.setTransferTimeout(mTimeout);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);

    QNetworkReply* reply = mManager->get(request);
    if (reply)
    {
        auto imageData = std::make_shared<ImageData>(imageUrl, width, height, format);
        mReplies.insert(reply, imageData);
    }
    else
    {
        mega::MegaApi::log(mega::MegaApi::LOG_LEVEL_WARNING,
                           "Failed to create QNetworkReply for URL: %s", imageUrl.toUtf8().constData());
    }
}

void ImageDownloader::onRequestImgFinished(QNetworkReply* reply)
{
    reply->deleteLater();

    if (!mReplies.contains(reply))
    {
        mega::MegaApi::log(mega::MegaApi::LOG_LEVEL_WARNING,
                           "Received finished signal for unknown QNetworkReply");
        emit downloadFinishedWithError(QString(), Error::InvalidUrl);
        return;
    }

    auto imageData = mReplies.take(reply);
    QByteArray bytes;
    if (!validateReply(reply, imageData->url, bytes))
    {
        return;
    }

    processImageData(bytes, imageData);
}


bool ImageDownloader::validateReply(QNetworkReply* reply,
                                    const QString& url,
                                    QByteArray& bytes)
{
    bool success = true;
    QVariant statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    if (!statusCode.isValid() || (statusCode.toInt() != StatusCodeOK) || (reply->error() != QNetworkReply::NoError))
    {
        mega::MegaApi::log(mega::MegaApi::LOG_LEVEL_WARNING,
                           "Error downloading image %s : %d",
                           reply->errorString().toUtf8().constData(), reply->error());
        emit downloadFinishedWithError(url, Error::NetworkError, reply->error());
        success = false;
    }

    if(success)
    {
        bytes = reply->readAll();
        if (bytes.isEmpty())
        {
            mega::MegaApi::log(mega::MegaApi::LOG_LEVEL_WARNING,
                               "Downloaded image data is empty");
            emit downloadFinishedWithError(url, Error::EmptyData, reply->error());
            success = false;
        }
    }

    return success;
}

void ImageDownloader::processImageData(const QByteArray& bytes,
                                       const std::shared_ptr<ImageData>& imageData)
{
    QImage image(imageData->size, imageData->format);
    if (image.loadFromData(bytes))
    {
        emit downloadFinished(image, imageData->url);
    }
    else
    {
        mega::MegaApi::log(mega::MegaApi::LOG_LEVEL_WARNING,
                           "Failed to load image from downloaded data");
        emit downloadFinishedWithError(imageData->url, Error::InvalidImage, QNetworkReply::UnknownContentError);
    }
}
