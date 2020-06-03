#include "graph.h"
#include "../../rts/rts.h"

// Nodes (graph vertices) ////////////////////////////////////////////////////////////////////////////

void $Node__init__($Node self, $list nbors) {
  self->nbors = nbors;
}

void $Node__serialize__($Node self,$Serial$state state) {
  $step_serialize(self->nbors,state); 
}

$bool $Node__bool__($Node self) {
  return $true;
}

$str $Node__str__($Node self) {
  return from$UTF8("$Node");
}

$Node $Node__deserialize__($Serial$state state) {                   
  $Node res = $DNEW($Node,state);                    
  res->nbors = ($list)$step_deserialize(state);      
  return res;                                        
}

struct $Node$class $Node$methods = {"",UNASSIGNED,NULL,$Node__init__,$Node__bool__,$Node__str__,$Node__serialize__,$Node__deserialize__};

// IntNodes (graph vertices) ////////////////////////////////////////////////////////////////////////////
 
void $IntNode__init__($IntNode self, $list nbors, $int ival) {
  self->nbors = nbors;
  self->ival= ival;
}

$bool $IntNode__bool__($IntNode self) {
  return $true;
}

$str $IntNode__str__($IntNode self) {
  return from$UTF8("$IntNode");
}

void $IntNode__serialize__($IntNode self,$Serial$state state) {
  $step_serialize(self->nbors,state);
  $step_serialize(self->ival,state);
}

$IntNode $IntNode__deserialize__($Serial$state state) {
  $IntNode res = $DNEW($IntNode,state);                    
  res->nbors = ($list)$step_deserialize(state);
  res->ival = ($int)$step_deserialize(state);
  return res;
}

struct $IntNode$class $IntNode$methods = {"",UNASSIGNED,NULL,$IntNode__init__,$IntNode__bool__,$IntNode__str__,$IntNode__serialize__,$IntNode__deserialize__};


// FloatNodes (graph vertices) ////////////////////////////////////////////////////////////////////////////

void $FloatNode__init__($FloatNode self, $list nbors, $float fval) {
  self->nbors = nbors;
  self->fval= fval;
}

$bool $FloatNode__bool__($FloatNode self) {
  return $true;
}

$str $FloatNode__str__($FloatNode self) {
  return from$UTF8("$FloatNode");
}

void $FloatNode__serialize__($FloatNode self,$Serial$state state) {
  $step_serialize(self->nbors,state);
  $step_serialize(self->fval,state);
}

$FloatNode $FloatNode__deserialize__($Serial$state state) {
  $FloatNode res = $DNEW($FloatNode,state);                    
  res->nbors = ($list)$step_deserialize(state);
  res->fval = ($float)$step_deserialize(state);
  return res;
}

struct $FloatNode$class $FloatNode$methods = {"",UNASSIGNED,NULL,$FloatNode__init__,$FloatNode__bool__,$FloatNode__str__,$FloatNode__serialize__,$FloatNode__deserialize__};


// Graphs ////////////////////////////////////////////////////////////////////////////


void $Graph__init__($Graph self, $list nodes) {
  self->nodes = nodes;
}

$bool $Graph__bool__($Graph self) {
  return $true;
}

$str $Graph__str__($Graph self) {
  return from$UTF8("$Graph");
}

void $Graph__serialize__($Graph self, $Serial$state state) {
  $step_serialize(self->nodes,state);
}

$Graph $Graph__deserialize__($Serial$state state) {
  $Graph res = $DNEW($Graph,state);
  res->nodes = ($list)$step_deserialize(state);
  return res;
}

struct $Graph$class $Graph$methods = {"",UNASSIGNED,NULL,$Graph__init__,$Graph__bool__,$Graph__str__,$Graph__serialize__,$Graph__deserialize__};

void $register_graph(){
  $register(&$Node$methods);
  $register(&$IntNode$methods);
  $register(&$FloatNode$methods);
  $register(&$Graph$methods);
}