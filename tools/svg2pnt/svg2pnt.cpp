#include <QApplication>
#include "svgxmlhandler.h"
#include <QDebug>

int main(int argc, char *argv[])
{
	QString sourcefile;
	QString targetfile;

	QApplication app(argc, argv);

	for ( int i = 1; i < argc; ++i ) {
		if ( strcmp( argv[ i ], "-o" ) == 0 ) {
			targetfile = QString(argv[i+1]);
			sourcefile = QString(argv[i+2]);

			SVGXmlHandler handler(targetfile);
			QFile xmlFile(sourcefile);
			QXmlInputSource inputSource(&xmlFile);
			QXmlSimpleReader reader;

			reader.setContentHandler(&handler);
			reader.parse( inputSource );
			return 0;
		}
	}

	qDebug(" svg2pnt -o targetfile sourcefile");
	app.exit();
}
