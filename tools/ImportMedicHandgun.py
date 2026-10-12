"""Blender -b --python tools/ImportMedicHandgun.py; original UVs, physical metres."""
import bpy,shutil
from pathlib import Path
root=Path(__file__).resolve().parents[1]
original=Path(r'O:\retro official\Weapons & Items\RetroWeaponPack_V1\Assets\RetroWeaponsPack\Guns\Pistol_01')
source=root/'source-assets/medic-handgun';source.mkdir(parents=True,exist_ok=True)
if not (source/'Pistol_01.fbx').exists():shutil.copyfile(original/'FbxFiles/Pistol_01.fbx',source/'Pistol_01.fbx')
if not (source/'Pistol_01_Albedo.png').exists():shutil.copyfile(original/'Textures/Pistol_01_Albedo.png',source/'Pistol_01_Albedo.png')
bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.fbx(filepath=str(source/'Pistol_01.fbx'))
lines=['# Medic handgun; source UVs; metres; grip at origin; forward +Z'];count=0
for obj in bpy.data.objects:
 if obj.type!='MESH':continue
 obj.data.calc_loop_triangles();uv=obj.data.uv_layers.active.data
 for triangle in obj.data.loop_triangles:
  for index,loop in zip(triangle.vertices,triangle.loops):
   p=obj.matrix_world@obj.data.vertices[index].co
   lines+=['v %.7f %.7f %.7f'%(p.y,p.z-.075,p.x-.02),'vt %.7f %.7f'%tuple(uv[loop].uv)]
  count+=3;lines.append(f'f {count-2}/{count-2} {count-1}/{count-1} {count}/{count}')
output=root/'src/assets/actors/cleanup'
(output/'medic-handgun.obj').write_text('\n'.join(lines)+'\n')
shutil.copyfile(source/'Pistol_01_Albedo.png',output/'medic-handgun.png')
print('MEDIC_HANDGUN',count//3,'triangles; original texture retained')
