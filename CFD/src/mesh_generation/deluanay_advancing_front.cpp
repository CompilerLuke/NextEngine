#include "mesh_generation/delaunay_advancing_front.h"
#include "mesh_generation/delaunay.h"

DelaunayFront::DelaunayFront(Delaunay& delaunay, CFDVolume& volume, CFDSurface& surface) : delaunay(delaunay), volume(volume), surface(surface) {
    
}
