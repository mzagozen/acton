#pragma once

struct $Iterator$class {
  char *$GCINFO;
  int $class_id;
  $Super$class $superclass;
  void (*__init__)($Iterator);
  $bool (*__bool__)($Iterator);
  $str (*__str__)($Iterator);
  void (*__serialize__)($Iterator,$Serial$state);
  $Iterator (*__deserialize__)($Serial$state);
  $WORD (*__next__)($Iterator);
};

struct $Iterator {
  struct $Iterator$class *$class;
};

extern struct $Iterable$Iterator$class $Iterable$Iterator$methods;
extern struct $Iterable$Iterator *$Iterable$Iterator$witness;

extern struct $Iterator$class $Iterator$methods;
extern struct $Iterator *$Iterator$witness;

$WORD $next($Iterator);