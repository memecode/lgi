import gdb
import pprint

class LStringPrinter:
    def __init__(self, val):
        self.val = val

    def to_string (self):
        s = self.val['Str']
        if s:
            len = str(s['Len'])
            refs = str(s['Refs'])
            value = s['Str'].reinterpret_cast(gdb.lookup_type("char").pointer()).string()
            return "(LString) refs=" + refs + " len=" + len + ", str='" + value + "'"
        
        return "(LString) null"

class LRectPrinter:
    def __init__(self, val):
        self.val = val

    def to_string (self):
        return "(LRect) {x1},{y1}-{x2},{y2} ({wid}x{ht})".format(
            x1 = self.val['x1'],
            y1 = self.val['y1'],
            x2 = self.val['x2'],
            y2 = self.val['y2'],
            wid = self.val['x2']-self.val['x1']+1,
            ht = self.val['y2']-self.val['y1']+1)

def lookup_type(val):
    if str(val.type) == 'LString':
        return LStringPrinter(val)
    elif str(val.type) == 'LRect':
        return LRectPrinter(val)
    return None

gdb.pretty_printers.append(lookup_type)