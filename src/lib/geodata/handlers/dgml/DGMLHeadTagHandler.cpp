/*
    Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>

    This file is part of the KDE project

    This library is free software you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "DGMLHeadTagHandler.h"

#include <QDebug>

#include "DGMLElementDictionary.h"
#include "GeoParser.h"
#include "GeoSceneDocument.h"

using namespace GeoSceneElementDictionary;

DGML_DEFINE_TAG_HANDLER(Head)

DGMLHeadTagHandler::DGMLHeadTagHandler()
    : GeoTagHandler()
{
}

DGMLHeadTagHandler::~DGMLHeadTagHandler()
{
}

GeoNode* DGMLHeadTagHandler::parse(GeoParser& parser) const
{
    Q_ASSERT(parser.isStartElement() && parser.isValidElement(dgmlTag_Head));

    // Checking for parent item
    GeoStackItem parentItem = parser.parentElement();
    if (parentItem.represents(dgmlTag_Document))
        return parentItem.nodeAs<GeoSceneDocument>()->head();

    return 0;
}
