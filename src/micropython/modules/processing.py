from sensor import Sensor
from parse_info import ParseType, SensorScheme, SensorComponent

class NodeKind:
    SOURCE = 0
    SINK = 1
    BIQUAD = 2
    PEAK_DETECTOR = 3
    ZERO_CROSSING = 4
    COMPONENT_EXTRACTOR = 5

class Node:
    name: str
    kind: NodeKind
    in_port_count: int

    def __init__(self, name: str, kind: NodeKind, in_port_count: int = 1):
        self.name = name
        self.kind = kind
        self.in_port_count = in_port_count

    def scheme_transform(self, schemes: list[SensorScheme]) -> SensorScheme | None:
        # TODO: throw error if scheme is invalid
        if not len(schemes) == self.in_port_count:
            return None
        scheme = schemes[0].copy()
        scheme.config_options = None # remove config options as stage is not configurable
        return scheme

    def __repr__(self):
        return f"Node(name={self.name}, kind={self.kind}, in_port_count={self.in_port_count})"

class BiQuadFilter(Node):
    def __init__(self, name: str, stages: int, coeffs: list[list[float]]):
        super().__init__(name, kind=NodeKind.BIQUAD, in_port_count=1)
        if stages != len(coeffs):
            raise ValueError("Number of stages must match number of coefficient sets.")
        for stage in coeffs:
            if len(stage) != 5:
                raise ValueError("Each stage must have exactly 5 coefficients.")
        self.stages = stages
        self.coeffs = coeffs

class PeakDetector(Node):
    def __init__(self, name: str):
        super().__init__(name, kind=NodeKind.PEAK_DETECTOR, in_port_count=1)

    def scheme_transform(self, schemes: list[SensorScheme]) -> SensorScheme | None:
        scheme = super().scheme_transform(schemes)
        if not scheme:
            return None
        if len(scheme.groups) != 1:
            return None
        if len(scheme.groups[0].components) != 1:
            return None
        scheme = scheme.copy()
        scheme.groups[0].components.append(SensorComponent(
            name="peak",
            parse_type=ParseType.INT8,
            unit="peak"
        ))
        return scheme

class ZeroCrossingDetector(Node):
    def __init__(self, name: str):
        super().__init__(name, kind=NodeKind.ZERO_CROSSING, in_port_count=1)

    def scheme_transform(self, schemes: list[SensorScheme]) -> SensorScheme | None:
        scheme = super().scheme_transform(schemes)
        if not scheme:
            return None
        scheme = scheme.copy()
        if len(scheme.groups) != 1:
            return None
        if len(scheme.groups[0].components) != 1:
            return None
        scheme.groups[0].components[0].parse_type = ParseType.INT8
        scheme.groups[0].components[0].unit = "zero_crossing"
        return scheme

class ComponentExtractor(Node):
    def __init__(self, name: str, group: str, component: str):
        super().__init__(name, kind=NodeKind.COMPONENT_EXTRACTOR, in_port_count=1)
        self.group = group
        self.component = component

    def scheme_transform(self, schemes: list[SensorScheme]) -> SensorScheme | None:
        scheme = super().scheme_transform(schemes)
        if not scheme:
            return None
        scheme = scheme.copy()
        scheme.groups = [g for g in scheme.groups if g.name == self.group]
        if not scheme.groups:
            return None
        scheme.groups[0].components = [c for c in scheme.groups[0].components if c.name == self.component]
        if not scheme.groups[0].components:
            return None
        return scheme

class Edge:
    src: Node
    dst: Node
    src_port: int

    def __init__(self, src: Node, dst: Node, src_port: int = 0):
        self.src = src
        self.dst = dst
        self.src_port = src_port

    def __repr__(self):
        return "Edge(src=" + str(self.src) + ", dst=" + str(self.dst) + ", src_port=" + str(self.src_port) + ")"

class Sink(Node):
    def __init__(self, name: str, on_event: callable):
        super().__init__(name, kind=NodeKind.SINK, in_port_count=1)
        self.on_event = on_event

    def scheme_transform(self, schemes: list[SensorScheme]) -> SensorScheme | None:
        return None
    
class Source(Node):
    def __init__(self, name: str, sensor: Sensor):
        super().__init__(name, kind=NodeKind.SOURCE, in_port_count=0)
        self.sensor = sensor

    def scheme_transform(self, schemes: list[SensorScheme]) -> SensorScheme | None:
        return self.sensor.scheme.copy()

class Pipeline:
    def __init__(self, name: str):
        self.name = name
        self._nodes: dict[str, Node] = {}
        self._edges: list[Edge] = []
        self._sinks: dict[str, Sink] = {}
        self._sources: dict[str, Source] = {}

    def source(self, name: str, sensor: Sensor) -> Pipeline:
        self._sources[name] = Source(name, sensor)
        return self

    def stage(self, node: Node) -> Pipeline:
        if isinstance(node, Source):
            self._sources[node.name] = node
        elif isinstance(node, Sink):
            self._sinks[node.name] = node
        else:
            self._nodes[node.name] = node
        return self

    def connect(self, src: str, dst: str, src_port: int = 0) -> Pipeline:
        if (src not in self._nodes and src not in self._sources) or (dst not in self._nodes and dst not in self._sinks):
            raise ValueError("Source or destination node does not exist.")
        if dst in self._sinks:
            dest = self._sinks[dst]
        else:
            dest = self._nodes[dst]
        if src in self._sources:
            src_node = self._sources[src]
        else:
            src_node = self._nodes[src]
        edge = Edge(src_node, dest, src_port)
        self._edges.append(edge)
        return self

    def sink(self, name: str, src: str, sink_func: callable) -> Pipeline:
        self._sinks[name] = Sink(name, sink_func)
        try:
            self.connect(src, name)
        except ValueError:
            del self._sinks[name]
            raise
        return self

    def output_schemes(self) -> dict[str, SensorScheme]:
        # 1) seed: source node -> sensor.scheme()
        out: dict[Node, SensorScheme] = {}
        for n in self._sources.values():
            out[n] = n.sensor.scheme.copy()  # or n.sensor.get_scheme()

        # 2) topo order (Kahn)
        nodes = list(self._nodes.values()) + list(self._sinks.values()) + list(self._sources.values())
        indeg: dict[Node,int] = {n:0 for n in nodes}
        for e in self._edges:
            indeg[e.dst] = indeg.get(e.dst,0)+1
        q = [n for n,d in indeg.items() if d==0]
        order = []
        while q:
            u = q.pop()
            order.append(u)
            for e in self._edges:
                if e.src is u:
                    indeg[e.dst] -= 1
                    if indeg[e.dst]==0: q.append(e.dst)

        if len(order) != len(nodes):
            raise ValueError("Cycle detected in processing pipeline.")

        # 3) forward propagate
        for n in order:
            if isinstance(n, Source) or isinstance(n, Sink):
                continue
            # collect input schemes in port order
            in_edges = [e for e in self._edges if e.dst is n]
            in_edges = sorted(in_edges, key=lambda e: getattr(e, "dst_port", 0))
            in_schemes = []
            for e in in_edges:
                s = out.get(e.src)
                if s is None:
                    raise ValueError(f"Unresolved input scheme for edge {e.src.name} -> {n.name}")
                in_schemes.append(s.copy())
            out[n] = n.scheme_transform(in_schemes)
            if out[n] is None:
                raise ValueError(f"Schema mismatch at node {n.name}")

        # 4) sink → its input scheme(s)
        result: dict[str, SensorScheme] = {}
        for name, sink in self._sinks.items():
            in_edges = [e for e in self._edges if e.dst is sink]
            in_edges.sort(key=lambda e: getattr(e, "dst_port", 0))
            if not in_edges:
                result[name] = None
                continue
            schemes = [out.get(e.src) for e in in_edges]
            if any(s is None for s in schemes):
                raise ValueError(f"Unresolved input at sink {name}")
            # If your sinks are single-input, return the single scheme:
            result[name] = schemes[0]
            # If multi-input sinks are possible, you could return `schemes` (list)
            # or implement a merge operation here.
        return result