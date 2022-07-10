#ifndef _OBJC_WRAPPER_H_
#define _OBJC_WRAPPER_H_

#ifdef __OBJC__
	#define ObjCWrapper(Type, Name) \
		class Name \
		{ \
		public: \
			Type * _Nullable p; \
			Name(Type * _Nullable i) { p = i; } \
			Name(long i = 0) { p = NULL; } \
			Name &operator=(Type * _Nullable i) { p = i; return *this; } \
			Name &operator=(long i) { p = NULL; return *this; } \
			operator bool() { return p != NULL; } \
			operator Type* _Nullable() { return p; } \
			bool operator ==(const Name &obj) { return p == obj.p; } \
		};
#else
	#define ObjCWrapper(Type, Name) \
		class Name \
		{ \
			void * _Nullable p; \
		public: \
			Name(void * _Nullable i) { p = i; } \
			Name(long i = 0) { p = NULL; } \
			Name &operator=(void * _Nullable i) { p = i; return *this; } \
			Name &operator=(long i) { p = NULL; return *this; } \
			operator bool() { return p != NULL; } \
			operator void * _Nullable() { return p; } \
			bool operator ==(const void * _Nullable obj) { return p == obj; } \
		};
#endif

#endif
