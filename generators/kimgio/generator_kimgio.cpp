/***************************************************************************
 *   Copyright (C) 2005 by Albert Astals Cid <aacid@kde.org>               *
 *   Copyright (C) 2006-2007 by Pino Toscano <pino@kde.org>                *
 *   Copyright (C) 2006-2007 by Tobias Koenig <tokoe@kde.org>              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "generator_kimgio.h"

#include <QBuffer>
#include <QImageReader>
#include <QPainter>
#include <QPrinter>
#include <QMimeType>
#include <QMimeDatabase>

#include <KAboutData>
#include <kaction.h>
#include <kactioncollection.h>
#include <kicon.h>
#include <kimageio.h>
#include <klocale.h>

#include <kexiv2/kexiv2.h>

#include <core/page.h>

static KAboutData createAboutData()
{
    KAboutData aboutData(
         "okular_kimgio",
         i18n( "Image Backend" ),
         "0.1.2",
         i18n( "A simple image backend" ),
         KAboutLicense::GPL,
         i18n( "© 2005, 2009 Albert Astals Cid\n© 2006-2007 Pino Toscano\n© 2006-2007 Tobias Koenig" )
    );
    aboutData.addAuthor( i18n( "Albert Astals Cid" ), QString(), "aacid@kde.org" );
    aboutData.addAuthor( i18n( "Pino Toscano" ), QString(), "pino@kde.org" );
    aboutData.addAuthor( i18n( "Tobias Koenig" ), QString(), "tokoe@kde.org" );
    return aboutData;
}

OKULAR_EXPORT_PLUGIN( KIMGIOGenerator, createAboutData() )

KIMGIOGenerator::KIMGIOGenerator( QObject *parent, const QVariantList &args )
    : Generator( parent, args )
{
    setFeature( ReadRawData );
    setFeature( Threaded );
    setFeature( TiledRendering );
    setFeature( PrintNative );
    setFeature( PrintToFile );
}

KIMGIOGenerator::~KIMGIOGenerator()
{
}

bool KIMGIOGenerator::loadDocument( const QString & fileName, QVector<Okular::Page*> & pagesVector )
{
    QMimeDatabase db;
    const QString mime = db.mimeTypeForFile(fileName, QMimeDatabase::MatchContent).name();
    const QStringList types = KImageIO::typeForMime(mime);
    const QByteArray type = !types.isEmpty() ? types[0].toAscii() : QByteArray();
    QImageReader reader( fileName, type );
    if ( !reader.read( &m_img ) ) {
        emit error( i18n( "Unable to load document: %1", reader.errorString() ), -1 );
        return false;
    }
    docInfo.set( Okular::DocumentInfo::MimeType, mime );

    // Apply transformations dictated by Exif metadata
    KExiv2Iface::KExiv2 exifMetadata;
    if ( exifMetadata.load( fileName ) ) {
        exifMetadata.rotateExifQImage( m_img, exifMetadata.getImageOrientation() );
    }

    pagesVector.resize( 1 );

    Okular::Page * page = new Okular::Page( 0, m_img.width(), m_img.height(), Okular::Rotation0 );
    pagesVector[0] = page;

    return true;
}

bool KIMGIOGenerator::loadDocumentFromData( const QByteArray & fileData, QVector<Okular::Page*> & pagesVector )
{
    QMimeDatabase db;
    const QString mime = db.mimeTypeForData(fileData).name();
    const QStringList types = KImageIO::typeForMime(mime);
    const QByteArray type = !types.isEmpty() ? types[0].toAscii() : QByteArray();
    
    QBuffer buffer;
    buffer.setData( fileData );
    buffer.open( QIODevice::ReadOnly );

    QImageReader reader( &buffer, type );
    if ( !reader.read( &m_img ) ) {
        emit error( i18n( "Unable to load document: %1", reader.errorString() ), -1 );
        return false;
    }
    docInfo.set( Okular::DocumentInfo::MimeType, mime );

    // Apply transformations dictated by Exif metadata
    KExiv2Iface::KExiv2 exifMetadata;
    if ( exifMetadata.loadFromData( fileData ) ) {
        exifMetadata.rotateExifQImage( m_img, exifMetadata.getImageOrientation() );
    }

    pagesVector.resize( 1 );

    Okular::Page * page = new Okular::Page( 0, m_img.width(), m_img.height(), Okular::Rotation0 );
    pagesVector[0] = page;

    return true;
}

bool KIMGIOGenerator::doCloseDocument()
{
    m_img = QImage();

    return true;
}

QImage KIMGIOGenerator::image( Okular::PixmapRequest * request )
{
    // perform a smooth scaled generation
    if ( request->isTile() )
    {
        const QRect srcRect = request->normalizedRect().geometry( m_img.width(), m_img.height() );
        const QRect destRect = request->normalizedRect().geometry( request->width(), request->height() );

        QImage destImg( destRect.size(), QImage::Format_RGB32 );
        destImg.fill( Qt::white );

        QPainter p( &destImg );
        p.setRenderHint( QPainter::SmoothPixmapTransform );
        p.drawImage( destImg.rect(), m_img, srcRect );

        return destImg;
    }
    else
    {
        int width = request->width();
        int height = request->height();
        if ( request->page()->rotation() % 2 == 1 )
            qSwap( width, height );

        return m_img.scaled( width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
    }
}

bool KIMGIOGenerator::print( QPrinter& printer )
{
    QPainter p( &printer );

    QImage image( m_img );

    if ( ( image.width() > printer.width() ) || ( image.height() > printer.height() ) )

        image = image.scaled( printer.width(), printer.height(),
                              Qt::KeepAspectRatio, Qt::SmoothTransformation );

    p.drawImage( 0, 0, image );

    return true;
}

Okular::DocumentInfo KIMGIOGenerator::generateDocumentInfo( const QSet<Okular::DocumentInfo::Key> &keys ) const
{
    return docInfo;
}

#include "generator_kimgio.moc"

