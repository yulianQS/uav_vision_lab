#!/usr/bin/env python3
"""Build a front-facing AprilTag range from official binary tag images.

Use SDF geometry per black cell to avoid texture filtering and UV orientation
issues. Pillow is needed only when regenerating the world.
"""
from pathlib import Path
import shutil
import xml.etree.ElementTree as ET
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
PX4 = ROOT.parent / 'PX4-Autopilot'
tree = ET.parse(PX4 / 'Tools/simulation/gz/worlds/default.sdf')
world = tree.getroot().find('world')
world.set('name', 'tag_range')

def text(parent, name, value):
    ET.SubElement(parent, name).text = str(value)

def box(link, name, pose, size, color):
    visual = ET.SubElement(link, 'visual', name=name)
    text(visual, 'pose', pose)
    geometry = ET.SubElement(visual, 'geometry')
    text(ET.SubElement(geometry, 'box'), 'size', size)
    material = ET.SubElement(visual, 'material')
    for field in ('ambient', 'diffuse', 'emissive'):
        text(material, field, color)
    text(visual, 'cast_shadows', 'false')

for tag_id, (x, y, z) in enumerate(((8, 0, 2), (12, 4, 2), (16, -4, 2))):
    model = ET.SubElement(world, 'model', name=f'tag36h11_{tag_id}')
    text(model, 'static', 'true')
    text(model, 'pose', f'{x} {y} {z} 0 0 0')
    link = ET.SubElement(model, 'link', name='board')
    box(link, 'white_margin', '0 0 0 0 0 0', '0.02 1 1', '1 1 1 1')
    collision = ET.SubElement(link, 'collision', name='board_collision')
    text(ET.SubElement(ET.SubElement(collision, 'geometry'), 'box'), 'size', '0.02 1 1')
    with Image.open(ROOT / f'assets/apriltag/tag36_11_{tag_id:05d}.png') as image:
        image = image.convert('L')
        assert image.size == (10, 10), 'Expected 8-cell black boundary plus 1-cell white margin'
        for v in range(10):
            for u in range(10):
                if image.getpixel((u, v)) < 128:
                    # Camera points +X; image right is -Y and image down is -Z.
                    py, pz = (4.5-u)*0.1, (4.5-v)*0.1
                    box(link, f'cell_{u}_{v}', f'-0.0106 {py:.4f} {pz:.4f} 0 0 0',
                        '0.001 0.10001 0.10001', '0 0 0 1')
ET.indent(tree, space='  ')
out = ROOT / 'worlds/tag_range.sdf'
tree.write(out, encoding='utf-8', xml_declaration=True)
shutil.copyfile(PX4 / 'Tools/simulation/gz/LICENSE', ROOT / 'worlds/PX4-LICENSE')
print(out)
