#ifndef __NOCOPY_H__
#define __NOCOPY_H__

#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);   \
  TypeName& operator=(const TypeName&)
#endif

#endif
