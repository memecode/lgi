import gdb
import time

class LStringPrettyPrinter:
	def __init__(self, val):
		self.val = val

	def to_string(self):
		return self.val['Str']

	def display_hint(self):
		return 'string'

def lstring_pretty_printer(val):
	if str(val.type) == 'LString':
		return LStringPrettyPrinter(val)
	else:
		return None

gdb.pretty_printers.append(lstring_pretty_printer)