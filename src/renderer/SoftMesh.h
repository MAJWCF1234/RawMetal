#pragma once
// Drawing soft padding pressed in (SoftBodies.h): the model's padding cut fine enough to bend, and how
// far a vertex of it goes down. The CPU path that draws the prop or fixture moves each vertex down by
// bend() before it scales and places the model, so either renderer draws the dent.
#include "Mesh.h"
#include "../game/SoftBodies.h"
#include <vector>
namespace retro::softmesh {
// The model's triangles with the faces at its padding's top split to about 9 cm (the mesh's own metres);
// built once per model.
const std::vector<MeshTriangle>& refined(int model,const Mesh& mesh);
// The depth, in the mesh's own height units, to lower vertex `p` (mesh coordinates) of `model` by:
// the pad's depth there, full at the padding's top and easing to nothing a little below it.
float bend(int model,const SoftPad& pad,const Mesh& mesh,const Point3& p);
// The light a vertex bent that far keeps: a pressed hollow reads darker (the cloth gathered and
// shadowed in it), down to 40% from 70% of the padding's give. The dent's slope alone barely shades.
float shade(int model,float bent);
}
