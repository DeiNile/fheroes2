/***************************************************************************
 *   Free Heroes of Might and Magic II: https://github.com/ihhub/fheroes2  *
 *   Copyright (C) 2021                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "mp2_helper.h"
#include "mp2.h"
#include "serialize.h"

namespace MP2
{
    void loadTile( StreamBase & stream, mp2tile_t & tile )
    {
        tile.surfaceType = stream.getLE16();
        tile.objectName1 = stream.get();
        tile.level1IcnImageIndex = stream.get();
        tile.quantity1 = stream.get();
        tile.quantity2 = stream.get();
        tile.objectName2 = stream.get();
        tile.level2IcnImageIndex = stream.get();
        tile.flags = stream.get();
        tile.mapObjectType = stream.get();
        tile.nextAddonIndex = stream.getLE16();
        tile.level1ObjectUID = stream.getLE32();
        tile.level2ObjectUID = stream.getLE32();
    }

    void loadAddon( StreamBase & stream, mp2addon_t & addon )
    {
        addon.nextAddonIndex = stream.getLE16();
        addon.objectNameN1 = stream.get() * 2;
        addon.indexNameN1 = stream.get();
        addon.quantityN = stream.get();
        addon.objectNameN2 = stream.get();
        addon.indexNameN2 = stream.get();
        addon.level1ObjectUID = stream.getLE32();
        addon.level2ObjectUID = stream.getLE32();
    }
}
