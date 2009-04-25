//
// This file is part of the Marble Desktop Globe.
//
// This program is free software licensed under the GNU LGPL. You can
// find a copy of this license in LICENSE.txt in the top directory of
// the source code.
//
// Copyright 2007      Torsten Rahn     <tackat@kde.org>"
//


#include "SphericalScanlineTextureMapper.h"

#include <cmath>

#include <QtCore/QDebug>
#include <QtGui/QImage>

#include "global.h"
#include "GeoPolygon.h"
#include "MarbleDirs.h"
#include "Quaternion.h"
#include "TextureTile.h"
#include "TileLoader.h"
#include "ViewParams.h"
#include "ViewportParams.h"
#include "MathHelper.h"

// Defining INTERLACE will make sure that for two subsequent scanlines
// every second scanline will be a deep copy of the first scanline.
// This results in a more coarse resolution and in a speedup for the 
// texture mapping of approx. 25%.

// #define INTERLACE

using namespace Marble;

SphericalScanlineTextureMapper::SphericalScanlineTextureMapper( TileLoader *tileLoader,
                                                                QObject * parent )
    : AbstractScanlineTextureMapper( tileLoader, parent ),
      m_interpolate( false ),
      m_nBest( 0 ),
      m_n( 0 ),
      m_nInverse( 0.0 )
{
    m_interlaced = false;
}


void SphericalScanlineTextureMapper::resizeMap(int width, int height)
{
    AbstractScanlineTextureMapper::resizeMap( width, height );

    // Find the optimal interpolation interval m_n for the 
    // current image canvas width
    m_nBest = 2;

    int  nEvalMin = m_imageWidth - 1;
    for ( int it = 1; it < 48; ++it ) {
        int nEval = ( m_imageWidth - 1 ) / it + ( m_imageWidth - 1 ) % it;
        if ( nEval < nEvalMin ) {
            nEvalMin = nEval;
            m_nBest = it; 
        }
    }
    // qDebug() << QString( "Optimized n = %1, remainder: %2" )
    //.arg(m_nBest).arg( ( m_imageWidth ) % m_nBest );
}


void SphericalScanlineTextureMapper::mapTexture( ViewParams *viewParams )
{
    QImage       *canvasImage = viewParams->canvasImage();
    const qint64  radius      = viewParams->radius();

    const bool highQuality  = ( viewParams->mapQuality() == Marble::High
				|| viewParams->mapQuality() == Marble::Print );
    const bool printQuality = ( viewParams->mapQuality() == Marble::Print );

    // Scanline based algorithm to texture map a sphere

    // Initialize needed variables:
    qreal  lon = 0.0;
    qreal  lat = 0.0;

    const qreal  inverseRadius = 1.0 / (qreal)(radius);

    m_tilePosX = 65535;
    m_tilePosY = 65535;
    m_toTileCoordinatesLon = (qreal)(globalWidth() / 2 - m_tilePosX);
    m_toTileCoordinatesLat = (qreal)(globalHeight() / 2 - m_tilePosY);

    // Reset backend
    m_tileLoader->resetTilehash();
    selectTileLevel( viewParams );

    // Evaluate the degree of interpolation
    m_n        = ( m_imageRadius < radius * radius ) ? m_nBest : 8;

    if ( printQuality ) {
        m_n = 1;    // Don't interpolate for print quality.
    }

    m_nInverse = 1.0 / (qreal)(m_n);

    // Calculate north pole position to decrease pole distortion later on
    Quaternion northPole( 0.0, (qreal)( M_PI * 0.5 ) );

    northPole.rotateAroundAxis( viewParams->planetAxis().inverse() );

    // Calculate axis matrix to represent the planet's rotation.
    matrix  planetAxisMatrix;
    viewParams->planetAxis().toMatrix( planetAxisMatrix );

    int skip = m_interlaced ? 1 : 0;

    // Calculate the actual y-range of the map on the screen 
    const int yTop = ( ( m_imageHeight / 2 - radius < 0 )
                       ? 0 : m_imageHeight / 2 - radius );
    const int yBottom = ( (yTop == 0)
                          ? m_imageHeight - skip
                          : yTop + radius + radius - skip );

    bool interlaced = ( m_interlaced 
			|| viewParams->mapQuality() == Marble::Low );

    for ( int y = yTop; y < yBottom ; ++y ) {

        // Evaluate coordinates for the 3D position vector of the current pixel
        const qreal qy = inverseRadius * (qreal)( m_imageHeight / 2 - y );
        const qreal qr = 1.0 - qy * qy;

        // rx is the radius component in x direction
        int rx = (int)sqrt( (qreal)( radius * radius 
                                      - ( ( y - m_imageHeight / 2 )
                                          * ( y - m_imageHeight / 2 ) ) ) );

        // Calculate the actual x-range of the map within the current scanline.
        // 
        // If the circular border of the earth disk is still visible then xLeft
        // equals the scanline position of the most left pixel that gets covered
        // by the earth disk. In terms of math this equals the half image width minus 
        // the radius component on the current scanline in x direction ("rx").
        //
        // If the zoom factor is high enough then the whole screen gets covered
        // by the earth and the border of the earth disk isn't visible anymore.
        // In that situation xLeft equals zero.
        // For xRight the situation is similar.

        const int xLeft  = ( ( m_imageWidth / 2 - rx > 0 )
                             ? m_imageWidth / 2 - rx : 0 ); 
        const int xRight = ( ( m_imageWidth / 2 - rx > 0 )
                             ? xLeft + rx + rx : canvasImage->width() );

        QRgb * scanLine = (QRgb*)( canvasImage->scanLine( y ) ) + xLeft;

        int  xIpLeft  = 1;
        int  xIpRight = m_n * (int)( xRight / m_n - 1) + 1; 

        if ( m_imageWidth / 2 - rx > 0 ) {
            xIpLeft  = m_n * (int)( xLeft  / m_n + 1 );
            xIpRight = m_n * (int)( xRight / m_n - 1 );
        }

        // Decrease pole distortion due to linear approximation ( y-axis )
        bool crossingPoleArea = false;
        int northPoleY = m_imageHeight / 2 - (int)( radius * northPole.v[Q_Y] );
        if ( northPole.v[Q_Z] > 0
             && northPoleY - ( m_n * 0.75 ) <= y
             && northPoleY + ( m_n * 0.75 ) >= y ) 
        {
            crossingPoleArea = true;
        }

        int ncount = 0;


        for ( int x = xLeft; x < xRight; ++x ) {
            // Prepare for interpolation

            int  leftInterval = xIpLeft + ncount * m_n;

            if ( x >= xIpLeft && x <= xIpRight ) {

                // Decrease pole distortion due to linear approximation ( x-axis )
                int northPoleX = m_imageWidth / 2 + (int)( radius * northPole.v[Q_X] );

//                qDebug() << QString("NorthPole X: %1, LeftInterval: %2").arg( northPoleX ).arg( leftInterval );
                if ( crossingPoleArea
                     && northPoleX >= leftInterval + m_n
                     && northPoleX < leftInterval + 2 * m_n
                     && x < leftInterval + 3 * m_n )
                {
                    m_interpolate = false;
                }
                else {
                    x += m_n - 1;
                    m_interpolate = !printQuality;
                    ++ncount;
                } 
            }
            else
                m_interpolate = false;

            // Evaluate more coordinates for the 3D position vector of
            // the current pixel.
            const qreal qx = (qreal)( x - m_imageWidth / 2 ) * inverseRadius;

            const qreal qr2z = qr - qx * qx;
            const qreal qz = ( qr2z > 0.0 ) ? sqrt( qr2z ) : 0.0;

            // Create Quaternion from vector coordinates and rotate it
            // around globe axis
            Quaternion qpos( 0.0, qx, qy, qz );
            qpos.rotateAroundAxis( planetAxisMatrix );

            qpos.getSpherical( lon, lat );
//            qDebug() << QString("lon: %1 lat: %2").arg(lon).arg(lat);
            // Approx for m_n-1 out of n pixels within the boundary of
            // xIpLeft to xIpRight

            if ( m_interpolate ) {
                pixelValueApprox( lon, lat, scanLine, highQuality );

                scanLine += ( m_n - 1 );
            }

//          Comment out the pixelValue line and run Marble if you want
//          to understand the interpolation:

//          Uncomment the crossingPoleArea line to check precise 
//          rendering around north pole:

//            if ( !crossingPoleArea )
            if ( x < m_imageWidth ) {
                    pixelValue( lon, lat, scanLine, highQuality );
            }
            m_prevLon = lon;
            m_prevLat = lat; // preparing for interpolation

            ++scanLine;
        }

        // copy scanline to improve performance
        if ( interlaced && y + 1 < yBottom ) { 

            int pixelByteSize = canvasImage->bytesPerLine() / m_imageWidth;

            memcpy( canvasImage->scanLine( y + 1 ) + xLeft * pixelByteSize, 
                    canvasImage->scanLine( y ) + xLeft * pixelByteSize, 
                    ( xRight - xLeft ) * pixelByteSize );
            ++y;
        }
    }

    m_tileLoader->cleanupTilehash();
}


// This method interpolates color values for skipped pixels in a scanline.
// 
// While moving along the scanline we don't move from pixel to pixel but
// leave out m_n pixels each time and calculate the exact position and 
// color value for the new pixel. The pixel values in between get 
// approximated through linear interpolation across the direct connecting 
// line on the original tiles directly.
// This method will do by far most of the calculations for the 
// texturemapping, so we move towards integer math to improve speed.

void SphericalScanlineTextureMapper::pixelValueApprox(const qreal& lon,
						      const qreal& lat, QRgb *scanLine, bool smooth )
{
    // stepLon/Lat: Distance between two subsequent approximated positions

    qreal stepLat = lat - m_prevLat;
    qreal stepLon = lon - m_prevLon;

    // As long as the distance is smaller than 180 deg we can assume that 
    // we didn't cross the dateline.

    if ( fabs(stepLon) < M_PI ) {
        if ( smooth ) {

            m_prevLon = rad2PixelX( m_prevLon );
            m_prevLat = rad2PixelY( m_prevLat );

            const qreal itStepLon = ( rad2PixelX( lon ) - m_prevLon ) * m_nInverse;
            const qreal itStepLat = ( rad2PixelY( lat ) - m_prevLat ) * m_nInverse;
        
            // To improve speed we unroll 
            // AbstractScanlineTextureMapper::pixelValue(...) here and 
            // calculate the performance critical issues via integers
    
            qreal itLon = m_prevLon + m_toTileCoordinatesLon;
            qreal itLat = m_prevLat + m_toTileCoordinatesLat;

            const int tileWidth = m_tileLoader->tileWidth();
            const int tileHeight = m_tileLoader->tileHeight();
            const int jmax = m_n;

/*
            int oldR = 0;
            int oldG = 0;
            int oldB = 0;
*/
            QRgb oldRgb = qRgb( 0, 0, 0 );

            qreal oldPosX = -1;
            qreal oldPosY = 0;

            for ( int j=1; j < jmax; ++j ) {
                qreal posX = itLon + itStepLon * j;
                qreal posY = itLat + itStepLat * j;
    
                if ( posX >= tileWidth 
                     || posX < 0.0
                     || posY >= tileHeight
                     || posY < 0.0 )
                {
                    nextTile( posX, posY );
                    itLon = m_prevLon + m_toTileCoordinatesLon;
                    itLat = m_prevLat + m_toTileCoordinatesLat;
                    posX = itLon + itStepLon * j;
                    posY = itLat + itStepLat * j;
                    oldPosX = -1;
                }
    
                *scanLine = m_tile->pixel( posX, posY ); 

                // Just perform bilinear interpolation if there's a color change compared to the 
                // last pixel that was evaluated. This speeds up things greatly for maps like OSM
                if ( *scanLine != oldRgb ) {
                    if ( oldPosX != -1 ) {
                        *(scanLine - 1) = m_tile->pixelF( oldPosX, oldPosY, *(scanLine - 1) );
                        oldPosX = -1;
                    }
                    oldRgb = m_tile->pixelF( posX, posY, *scanLine );
                    *scanLine = oldRgb;
                }
                else {
                    oldPosX = posX;
                    oldPosY = posY;
                }
                
/*
                if ( needsFilter( *scanLine, oldR, oldB, oldG  ) ) {
                    *scanLine = m_tile->pixelF( posX, posY );
                }
*/
                ++scanLine;
            }
        }
        else {
            m_prevLon = rad2PixelX( m_prevLon );
            m_prevLat = rad2PixelY( m_prevLat );

            const int itStepLon = (int)( ( rad2PixelX( lon ) - m_prevLon ) * m_nInverse * 128.0 );
            const int itStepLat = (int)( ( rad2PixelY( lat ) - m_prevLat ) * m_nInverse * 128.0 );
        
            // To improve speed we unroll 
            // AbstractScanlineTextureMapper::pixelValue(...) here and 
            // calculate the performance critical issues via integers
    
            int itLon = (int)( ( m_prevLon + m_toTileCoordinatesLon ) * 128.0 );
            int itLat = (int)( ( m_prevLat + m_toTileCoordinatesLat ) * 128.0 );

            const int tileWidth = m_tileLoader->tileWidth();
            const int tileHeight = m_tileLoader->tileHeight();
            const int jmax = m_n;

            for ( int j = 1; j < jmax; ++j ) {
                int iPosX = ( itLon + itStepLon * j ) >> 7;
                int iPosY = ( itLat + itStepLat * j ) >> 7;
    
                if ( iPosX >= tileWidth 
                     || iPosX < 0
                     || iPosY >= tileHeight
                     || iPosY < 0 )
                {
                    nextTile( iPosX, iPosY );
                    itLon = (int)( ( m_prevLon + m_toTileCoordinatesLon ) * 128.0 );
                    itLat = (int)( ( m_prevLat + m_toTileCoordinatesLat ) * 128.0 );
                    iPosX = ( itLon + itStepLon * j ) >> 7;
                    iPosY = ( itLat + itStepLat * j ) >> 7;
                }

                *scanLine = m_tile->pixel( iPosX, iPosY ); 
                ++scanLine;
            }
        }
    }

    // For the case where we cross the dateline between (lon, lat) and 
    // (prevlon, prevlat) we need a more sophisticated calculation.
    // However as this will happen rather rarely, we use 
    // pixelValue(...) directly to make the code more readable.

    else {
        stepLon = ( TWOPI - fabs(stepLon) ) * m_nInverse;
        stepLat = stepLat * m_nInverse;
        // We need to distinguish two cases:  
        // crossing the dateline from east to west ...

        if ( m_prevLon < lon ) {

            const int jmax = m_n;
            for ( int j = 1; j < jmax; ++j ) {
                m_prevLat += stepLat;
                m_prevLon -= stepLon;
                if ( m_prevLon <= -M_PI ) 
                    m_prevLon += TWOPI;
                pixelValue( m_prevLon, m_prevLat, scanLine, smooth );
                ++scanLine;
            }
        }

        // ... and vice versa: from west to east.

        else { 
            qreal curStepLon = lon - m_n * stepLon;

            const int jmax = m_n;
            for ( int j = 1; j < jmax; ++j ) {
                m_prevLat += stepLat;
                curStepLon += stepLon;
                qreal  evalLon = curStepLon;
                if ( curStepLon <= -M_PI )
                    evalLon += TWOPI;
                pixelValue( evalLon, m_prevLat, scanLine, smooth );
                ++scanLine;
            }
        }
    }
}

bool SphericalScanlineTextureMapper::needsFilter( const QRgb& rgb, int& oldR, int& oldB, int &oldG  )
{
    int red = qRed( rgb );
    int green = qGreen( rgb );
    int blue = qBlue( rgb );

    bool differs = false;

    if ( abs( red - oldR ) > 0 || abs( blue - oldB ) > 0 || abs( green - oldG ) > 0 ) {
        differs = true;
    }

    oldR = red;
    oldG = green;
    oldB = blue;

    return differs;
}
