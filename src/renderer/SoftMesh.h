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
// Every face cut to about 9 cm (a mattress bent on its bones bends smoothly all round).
const std::vector<MeshTriangle>& refinedAll(int model,const Mesh& mesh);
// The depth, in the mesh's own height units, to lower vertex `p` (mesh coordinates) of `model` by:
// the pad's depth there, full at the padding's top and easing to nothing a little below it.
float bend(int model,const SoftPad& pad,const Mesh& mesh,const Point3& p);
// The light a vertex of the pressed padding takes, against the same vertex lying flat (1 where it
// does): Lambert for the pressed surface's own normal (SoftPad::normalMesh, recomputed from the grid)
// toward `light` (a unit direction to the lamp over it), over Lambert for the flat top, and a little
// occlusion down in the hollow. A hollow so shades as one: the slope facing the lamp lit, the far one
// dark, its rim catching the light.
float shade(int model,const SoftPad& pad,const Mesh& mesh,const Point3& p,float bent,Point3 light);
}
