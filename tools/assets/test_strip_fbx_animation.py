import struct
import unittest
from strip_fbx_animation import Node, parse, serialize, strip_animation

def integer(v): return b"L"+struct.pack("<q",v)
def string(v): return b"S"+struct.pack("<I",len(v))+v

class StripAnimationTests(unittest.TestCase):
    def fixture(self):
        vertices=b"d"+struct.pack("<III",3,0,24)+struct.pack("<3d",1,2,3)
        geometry=Node(b"Geometry",[integer(1),string(b"KitArms")],[Node(b"Vertices",[vertices])],True)
        objects=Node(b"Objects",[],[geometry,Node(b"AnimationStack",[integer(2)]),Node(b"AnimationCurve",[integer(3)])],True)
        connections=Node(b"Connections",[],[
            Node(b"C",[string(b"OO"),integer(1),integer(0)]),
            Node(b"C",[string(b"OO"),integer(3),integer(2)])],True)
        definitions=Node(b"Definitions",[],[
            Node(b"Count",[b"I"+struct.pack("<i",3)]),
            Node(b"ObjectType",[string(b"Geometry")]),
            Node(b"ObjectType",[string(b"AnimationStack")]),
            Node(b"ObjectType",[string(b"AnimationCurve")])],True)
        header=b"Kaydara FBX Binary  \x00\x1a\x00"+struct.pack("<I",7400)
        return serialize(header,[definitions,objects,connections],bytes(13)+b"footer")
    def test_preserves_geometry_and_bind_properties_while_removing_animation_graph(self):
        source=self.fixture()
        header,nodes,footer=parse(source)
        self.assertEqual(serialize(header,nodes,footer),source)
        geometry=next(n for n in nodes if n.name==b"Objects").children[0]
        _,result,end=parse(strip_animation(source))
        objects=next(n for n in result if n.name==b"Objects")
        self.assertEqual(objects.children,[geometry])
        connections=next(n for n in result if n.name==b"Connections")
        self.assertEqual(len(connections.children),1)
        self.assertEqual(connections.children[0].props[-2:],[integer(1),integer(0)])
        definitions=next(n for n in result if n.name==b"Definitions")
        self.assertEqual(definitions.children[0].props,[b"I"+struct.pack("<i",1)])
        self.assertEqual(len(definitions.children),2)
        self.assertEqual(end,footer)
    def test_rejects_already_stripped_input(self):
        with self.assertRaisesRegex(ValueError,"No animation"):
            strip_animation(strip_animation(self.fixture()))
    def test_rejects_unrecognized_format_and_version(self):
        with self.assertRaises(ValueError): strip_animation(b"not FBX")
        data=bytearray(self.fixture());struct.pack_into("<I",data,23,7500)
        with self.assertRaisesRegex(ValueError,"7400"): strip_animation(bytes(data))
    def test_rejects_truncated_or_out_of_bounds_nodes(self):
        data=self.fixture()
        with self.assertRaises(ValueError): strip_animation(data[:23])
        with self.assertRaises(ValueError): strip_animation(data[:45])
        damaged=bytearray(data);struct.pack_into("<I",damaged,27,len(data)+100)
        with self.assertRaises(ValueError): strip_animation(bytes(damaged))

if __name__=="__main__": unittest.main()
