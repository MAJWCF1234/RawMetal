"""Blender -b --python tools/ImportMedicHandgun.py; original UVs, physical metres.

One OBJ object per moving part, so the player's pistol can work its slide,
hammer and magazine: PistolHammer, Pistol (frame, barrel, safety and stop),
PistolSlide and PistolTrigger, in the order the merged file always had, then
PistolMagazine (the pack's separate magazine body, seated in the grip) last.
The NPC draw skips the magazine, so the actors' handgun is unchanged.
"""
import bpy,shutil
from pathlib import Path
root=Path(__file__).resolve().parents[1]
original=Path(r'O:\retro official\Weapons & Items\RetroWeaponPack_V1\Assets\RetroWeaponsPack\Guns\Pistol_01')
source=root/'source-assets/medic-handgun';source.mkdir(parents=True,exist_ok=True)
for name,folder in (('Pistol_01.fbx','FbxFiles'),('Pistol_01_AdditionalMeshes.fbx','FbxFiles'),('Pistol_01_Albedo.png','Textures')):
 if not (source/name).exists():shutil.copyfile(original/folder/name,source/name)
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.fbx(filepath=str(source/'Pistol_01.fbx'))
parts={'Hammer_Mesh':'PistolHammer','Main_Mesh':'Pistol','Slide_Mesh':'PistolSlide','Trigger_Mesh':'PistolTrigger'}
lines=['# Medic handgun; source UVs; metres; grip at origin; forward +Z'];count=0
def write(obj,name,material=None):
 global count
 lines.append('o '+name)
 obj.data.calc_loop_triangles();uv=obj.data.uv_layers.active.data
 for triangle in obj.data.loop_triangles:
  if material is not None and triangle.material_index!=material:continue
  for index,loop in zip(triangle.vertices,triangle.loops):
   p=obj.matrix_world@obj.data.vertices[index].co
   lines.extend(['v %.7f %.7f %.7f'%(p.y,p.z-.075,p.x-.02),'vt %.7f %.7f'%tuple(uv[loop].uv)])
  count+=3;lines.append(f'f {count-2}/{count-2} {count-1}/{count-1} {count}/{count}')
for obj in [o for o in bpy.data.objects if o.type=='MESH']:write(obj,parts[obj.name])
# The magazine body only: its cartridges use the pack's projectile texture.
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.fbx(filepath=str(source/'Pistol_01_AdditionalMeshes.fbx'))
write(bpy.data.objects['Pistol_01_Magazine_Full_Mesh'],'PistolMagazine',0)
output=root/'src/assets/actors/cleanup'
(output/'medic-handgun.obj').write_text('\n'.join(lines)+'\n')
shutil.copyfile(source/'Pistol_01_Albedo.png',output/'medic-handgun.png')
print('MEDIC_HANDGUN',count//3,'triangles; original texture retained')
