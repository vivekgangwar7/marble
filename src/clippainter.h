#ifndef CLIPPAINTER_H
#define CLIPPAINTER_H

#include <QPainter>

/**
@author Torsten Rahn
*/

class ClipPainter : public QPainter {
public:
	ClipPainter();
	ClipPainter(QPaintDevice*, bool);
	~ClipPainter(){};
	void drawPolygon ( const QPolygon &, Qt::FillRule fillRule = Qt::OddEvenFill );
	void drawPolyline ( const QPolygon & );
private:
	void drawPolyobject ( const QPolygon & );

	bool m_clip;
	int left, right, top, bottom;
	int imgwidth, imgheight;
	int currentpos, currentxpos, currentypos;
	int lastpos;

	void manageOffScreen();
	const QPoint borderPoint();

	QPoint currentPoint, lastPoint; 
	QPolygon m_clipped;

};

#endif // CLIPPAINTER_H
