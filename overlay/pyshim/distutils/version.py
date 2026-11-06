import re
class LooseVersion:
    def __init__(self, v): self.v = [int(x) if x.isdigit() else x for x in re.split(r"(\d+)", str(v)) if x and x != "."]
    def _c(self, o): return o.v if isinstance(o, LooseVersion) else LooseVersion(o).v
    def __lt__(self, o): return self.v < self._c(o)
    def __le__(self, o): return self.v <= self._c(o)
    def __gt__(self, o): return self.v > self._c(o)
    def __ge__(self, o): return self.v >= self._c(o)
    def __eq__(self, o): return self.v == self._c(o)
