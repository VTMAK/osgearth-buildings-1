/* -*-c++-*- */
/* osgEarth - Dynamic map generation toolkit for OpenSceneGraph
 * Copyright 2008-2016 Pelican Mapping
 * http://osgearth.org
 *
 * osgEarth is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */
#include "BuildingFactory"
#include "BuildingSymbol"
#include <osgEarthSymbology/Geometry>

using namespace osgEarth;
using namespace osgEarth::Buildings;
using namespace osgEarth::Features;
using namespace osgEarth::Symbology;

#define LC "[BuildingFactory] "

BuildingFactory::BuildingFactory()
{
    //nop
}

bool
BuildingFactory::create(FeatureCursor*    input,
                        BuildingVector&   output,
                        ProgressCallback* progress)
{
    if ( !input )
        return false;

    // iterate over all the input features
    while( input->hasMore() )
    {
        // for each feature, check that it's a polygon
        Feature* feature = input->nextFeature();
        if ( feature )
        {
            Building* building = createBuilding(feature);
            if ( building )
            {
                output.push_back( building );
            }
        }
    }

    return true;
}

Building*
BuildingFactory::createBuilding(Feature* feature)
{
    if ( feature == 0L )
        return 0L;

    osg::ref_ptr<Building> building;

    Geometry* geometry = feature->getGeometry();

    if ( geometry && geometry->getComponentType() == Geometry::TYPE_POLYGON && geometry->isValid() )
    {
        // TODO: validate the 
        // Calculate a local reference frame for this building:
        osg::Vec2d center2d = geometry->getBounds().center2d();
        GeoPoint centerPoint( feature->getSRS(), center2d.x(), center2d.y(), 0.0, ALTMODE_ABSOLUTE );

        osg::Matrix local2world, world2local;
        centerPoint.createLocalToWorld( local2world );
        world2local.invert( local2world );

        // Transform feature geometry into the local frame. This way we can do all our
        // building creation in cartesian space.
        GeometryIterator iter(geometry, true);
        while(iter.hasMore())
        {
            Geometry* part = iter.next();
            for(Geometry::iterator i = part->begin(); i != part->end(); ++i)
            {
                osg::Vec3d world;
                feature->getSRS()->transformToWorld( *i, world );
                (*i) = world * world2local;
            }
        }

        // Next, iterate over the polygons and set up the Building object.
        GeometryIterator iter2( geometry, false );
        while(iter2.hasMore())
        {
            Polygon* footprint = dynamic_cast<Polygon*>(iter2.next());
            if ( footprint && footprint->isValid() )
            {
                // A footprint is the minumum info required to make a building.
                building = new Building();

                // Install the reference frame of the footprint geometry:
                building->setReferenceFrame( local2world );

                // Do initial cleaning of the footprint and install is:
                cleanPolygon( footprint );
                building->setFootprint( footprint );
            }
            else
            {
                OE_WARN << LC << "Feature " << feature->getFID() << " is not a polygon. Skipping..\n";
            }
        }
    }

    return building.release();
}

void
BuildingFactory::cleanPolygon(Polygon* polygon)
{
    polygon->open();

    polygon->removeDuplicates();

    polygon->rewind( Polygon::ORIENTATION_CCW );

    // TODO: remove colinear points
}