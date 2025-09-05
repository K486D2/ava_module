#ifndef ERRDEF_H
#define ERRDEF_H

#ifdef __cpluscplus
extern "C" {
#endif

enum errdef_e {
  MEINVAL,
  MEBUSY,
  MEACCES,
  METIMEOUT,
  MECREATE,
};

#ifdef __cpluscplus
}
#endif

#endif // !ERRDEF_H
