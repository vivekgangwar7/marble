//
// C++ Interface: gpmapscale
//
// Description: KAtlasMapScale 

// KAtlasMapScale resembles the scale in terms of value and visualisation
//
// Author: Torsten Rahn <tackat@kde.org>, (C) 2006
//
// Copyright: See COPYING file that comes with this distribution

#ifndef KATLASMAPSCALE_H
#define KATLASMAPSCALE_H

#include <QObject>
#include <QPixmap>
#include <QFont>
#include <QFontMetrics>

/**
@author Torsten Rahn
*/

class KAtlasMapScale : public QObject {

Q_OBJECT

public:
	KAtlasMapScale(QObject *parent = 0);

	void setInvScale(int invscale){ m_invscale = invscale; }
	int invScale()const{ return m_invscale; }


	void setScaleBarHeight(int scalebarheight){ m_scalebarheight = scalebarheight; }
	int scaleBarHeight()const{ return m_scalebarheight; }

	int scaleBarWidth()const{ return m_scalebarwidth; }

//	void setFont(QFont font){ m_font = font; }

	void paintScaleBar(QPainter*, int, int);
	QPixmap& drawScaleBarPixmap(int, int);
//	QPicture scaleBar(int radius, int width);

public slots:

	void setScaleBarWidth(int scalebarwidth){ m_scalebarwidth = scalebarwidth; }

private:
	void calcScaleBar();
	QPixmap m_pixmap;
	int m_invscale, m_radius;
	int m_scalebarwidth, m_scalebarheight;
	double m_scalebarkm;

	QFont m_font;
	int m_leftmargin, m_rightmargin, m_fontheight;
	int m_bestdivisor, m_pixelinterval, m_valueinterval;

	QString m_unit;
};

#endif // KATLASMAPSCALE_H
