import sys
import lldb

# Add to ~/.lldbinit
#       command script import %lgi/src/mac/lgi_types_xcode.py

def is_null(val):
    # Returns True if the SBValue is invalid OR represents a null pointer
    return not val.IsValid() or val.GetValueAsUnsigned() == 0

def LStringFn(valobj, internal_dict, options):
    Str = valobj.GetChildMemberWithName("Str")
    if is_null(Str):
        return "NULL"
    len = Str.GetChildMemberWithName("Len").GetValueAsUnsigned()
    char = valobj.target.GetBasicType(lldb.eBasicTypeChar)
    addr = Str.GetChildMemberWithName("Str").GetAddress()
    txt = valobj.target.CreateValueFromAddress("name", addr, char).AddressOf()
    return 'len: ' + str(len) + ', str: ' + str(txt)


def LArrayFn(valobj, internal_dict, options):
   len = valobj.GetChildMemberWithName('len')
   p = valobj.GetChildMemberWithName('p')
   return 'len: ' + str(len) + ', p: ' + str(p)

def __lldb_init_module(debugger, dict):
    print("loading Lgi type summary support...")
    debugger.HandleCommand('type summary add LString -F lgi_types_xcode.LStringFn')
    debugger.HandleCommand('type summary add LArray -F lgi_types_xcode.LArrayFn')