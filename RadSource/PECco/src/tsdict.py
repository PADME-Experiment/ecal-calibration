from __future__ import print_function
import threading

class TSDict(object):
    def __init__(self, dict={}):
        self.accessLock = threading.Lock()
        self.data = {}

    def __lockRelease(self, fun, *args, **kwds):
        self.accessLock.acquire()
        try:
            tmp = fun(*args, **kwds)
        finally:
            self.accessLock.release()
        return tmp

    def __getitem__(self, key):
        return self.__lockRelease(self.data.__getitem__, key)

    def __setitem__(self, key, value):
        return self.__lockRelease(self.data.__setitem__, key, value)

    def __delitem__(self, key):
        return self.__lockRelease(self.data.__delitem__, key)

    def __repr__(self):
        return self.__lockRelease(self.data.__repr__)

    def __iter__(self):
        return iter(self.__lockRelease(self.data.keys))
    
    def __len__(self):
        return self.__lockRelease(self.data.__len__)

    def __str__(self):
        return repr(self)

    def clear(self):
        return self.__lockRelease(self.data.clear)

    def copy(self):
        return self.__lockRelease(self.data.copy)

    def has_key(self, key):
        return self.__lockRelease(self.data.has_key, key)

    def items(self):
        return self.__lockRelease(self.data.items)

    def iteritems(self):
        return iter(self.__lockRelease(self.data.items))

    def iterkeys(self):
        return iter(self.__lockRelease(self.data.keys))

    def itervalues(self):
        return iter(self.__lockRelease(self.data.values))

    def keys(self):
        return self.__lockRelease(self.data.keys)

    def values(self):
        return self.__lockRelease(self.data.values)
       
    #'clear', 'copy', 'fromkeys', 'get', 'has_key', 'items', 'iteritems', 'iterkeys', 'itervalues', 'keys', 'pop', 'popitem', 'setdefault', 'update', 'values', 'viewitems', 'viewkeys', 'viewvalues'

    def __getattr__(self, name):
        print("Sorry: ", name, " is not implemented")
        def donil(*args): pass
        return donil