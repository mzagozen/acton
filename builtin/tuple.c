/*
 * Copyright (C) 2019-2021 Data Ductus AB
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdarg.h>

void B_tupleD_init(B_tuple self,int size ,...) {
    va_list args;
    va_start(args,size);
    self->size = size;
    self->components = malloc(size*sizeof($WORD));
    for (int i=0; i<size; i++)
        self->components[i] = va_arg(args,$WORD);
    va_end(args);
}

B_bool B_tupleD_bool(B_tuple self) {
    return toB_bool(self->size>0);
}

B_str B_tupleD_str(B_tuple self) {
    B_list s2 = B_listD_new(self->size);
    for (int i=0; i< self->size; i++) {
        B_value elem = (B_value)self->components[i];
        B_listD_append(s2,elem->$class->__repr__(elem));
    }
    return B_strD_join_par('(',s2,')');
}


void B_tupleD_serialize(B_tuple self, $Serial$state state) {
    B_int prevkey = (B_int)B_dictD_get(state->done,(B_Hashable)B_HashableD_WORDG_witness,self,NULL);
    if (prevkey) {
        $val_serialize(-TUPLE_ID,&prevkey->val,state);
        return;
    }
    B_dictD_setitem(state->done,(B_Hashable)B_HashableD_WORDG_witness,self,toB_int(state->row_no));
    long len = (long)self->size;
    $val_serialize(TUPLE_ID,&len,state);
    for (int i=0; i<self->size; i++) {
        $step_serialize(self->components[i],state);
    }
}

B_tuple B_tupleD_deserialize(B_tuple self, $Serial$state state) {
    $ROW this = state->row;
    state->row = this->next;
    state->row_no++;
    if (this->class_id < 0) {
        return (B_tuple)B_dictD_get(state->done,(B_Hashable)B_HashableD_intG_witness,toB_int((long)this->blob[0]),NULL);
    } else {
        int len = (int)(long)this->blob[0];
        B_tuple res = malloc(sizeof(struct B_tuple));
        B_dictD_setitem(state->done,(B_Hashable)B_HashableD_intG_witness,toB_int(state->row_no-1),res);
        res->components = malloc(len * sizeof($WORD));
        res->$class = &B_tupleG_methods;
        res->size = len;
        for (int i = 0; i < len; i++) 
            res->components[i] = $step_deserialize(state);
        return res;
    }
}

struct B_tupleG_class B_tupleG_methods = {
    "tuple",
    UNASSIGNED,
    ($SuperG_class)&B_valueG_methods,
    B_tupleD_init,
    B_tupleD_serialize,
    B_tupleD_deserialize,
    B_tupleD_bool,
    B_tupleD_str,
    B_tupleD_str
};

// Iterators over tuples ///////////////////////////////////////////////////////

static $WORD B_IteratorB_tupleD_next(B_IteratorB_tuple self) {
    return self->nxt >= self->src->size ? NULL : self->src->components[self->nxt++];
}

void B_IteratorB_tupleD_init(B_IteratorB_tuple self, B_tuple lst) {
    self->src = lst;
    self->nxt = 0;
}

B_bool B_IteratorB_tupleD_bool(B_IteratorB_tuple self) {
    return $True;
}

B_str B_IteratorB_tupleD_str(B_IteratorB_tuple self) {
    char *s;
    asprintf(&s,"<tuple iterator object at %p>",self);
    return to$str(s);
}
void B_IteratorB_tupleD_serialize(B_IteratorB_tuple self,$Serial$state state) {
    $step_serialize(self->src,state);
    $step_serialize(toB_int(self->nxt),state);
}

B_IteratorB_tuple B_IteratorB_tuple$_deserialize(B_IteratorB_tuple res, $Serial$state state) {
    if (!res)
        res = $DNEW(B_IteratorB_tuple,state);
    res->src = $step_deserialize(state);
    res->nxt = fromB_int((B_int)$step_deserialize(state));
    return res;
}

struct B_IteratorB_tupleG_class B_IteratorB_tupleG_methods = {"B_IteratorB_tuple",UNASSIGNED,($SuperG_class)&B_IteratorG_methods,B_IteratorB_tupleD_init,
                                                        B_IteratorB_tupleD_serialize,B_IteratorB_tuple$_deserialize,B_IteratorB_tupleD_bool,B_IteratorB_tupleD_str,B_IteratorB_tupleD_str,B_IteratorB_tupleD_next};


// Iterable ///////////////////////////////////////////////////////////////

B_Iterator B_IterableD_tupleD___iter__(B_IterableD_tuple wit, B_tuple self) {
    return (B_Iterator)$NEW(B_IteratorB_tuple,self);
}

void B_IterableD_tupleD___init__(B_IterableD_tuple self) {
    return;
}

void B_IterableD_tupleD___serialize__(B_IterableD_tuple self, $Serial$state state) {
}

B_IterableD_tuple B_IterableD_tupleD___deserialize__(B_IterableD_tuple self, $Serial$state state) {
    B_IterableD_tuple res = $DNEW(B_IterableD_tuple,state);
    return res;
}
struct B_IterableD_tupleG_class B_IterableD_tupleG_methods = {
    "B_IterableD_tuple",
    UNASSIGNED,
    ($SuperG_class)&B_IterableG_methods,
    B_IterableD_tupleD___init__,
    B_IterableD_tupleD___serialize__,
    B_IterableD_tupleD___deserialize__,
    (B_bool (*)(B_IterableD_tuple))$default__bool__,
    (B_str (*)(B_IterableD_tuple))$default__str__,
    (B_str (*)(B_IterableD_tuple))$default__str__,
    B_IterableD_tupleD___iter__
};
struct B_IterableD_tuple B_IterableD_tuple$instance = {&B_IterableD_tupleG_methods};
struct B_IterableD_tuple *B_IterableD_tupleG_witness = &B_IterableD_tuple$instance;

// Sliceable ///////////////////////////////////////////////////////////////

void B_SliceableD_tupleD___serialize__(B_SliceableD_tuple self, $Serial$state state) {
}

B_SliceableD_tuple B_SliceableD_tupleD___deserialize__(B_SliceableD_tuple self, $Serial$state state) {
    B_SliceableD_tuple res = $DNEW(B_SliceableD_tuple,state);
    return res;
}

void B_SliceableD_tupleD___init__ (B_SliceableD_tuple wit) {
    return;
}

$WORD B_SliceableD_tupleD___getitem__ (B_SliceableD_tuple wit, B_tuple self, B_int n) {
    int size = self->size;
    int ix = fromB_int(n);
    int ix0 = ix < 0 ? size + ix : ix;
    if (ix0 < 0 || ix0 >= size) {
        $RAISE((B_BaseException)$NEW(B_IndexError,to$str("getitem: indexing outside tuple")));
    }
    return self->components[ix0];
}


void B_SliceableD_tupleD___setitem__ (B_SliceableD_tuple wit, B_tuple self, B_int ix, $WORD elem) {
    fprintf(stderr,"%s\n","internal error: setitem on immutable tuple");
    exit(-1);
}

void B_SliceableD_tupleD___delitem__ (B_SliceableD_tuple wit, B_tuple self, B_int ix) {
    fprintf(stderr,"%s\n","internal error: delitem on immutable tuple");
    exit(-1);
}
  

B_tuple B_SliceableD_tupleD___getslice__ (B_SliceableD_tuple wit, B_tuple self, B_slice slc) {
    int size = self->size;
    int start, stop, step, slen;
    normalize_slice(slc, size, &slen, &start, &stop, &step);
    //slice notation have been eliminated and default values applied.
    // slen now is the length of the slice
    B_tuple res = malloc(sizeof(struct B_tuple));
    res->$class = self->$class;
    res->size = slen;
    res->components = malloc(slen * sizeof($WORD));
    int t = start;
    for (int i=0; i<slen; i++) {
        res->components[i] = self->components[t];
        t += step;
    }
    return res;
}

void B_SliceableD_tupleD___setslice__ (B_SliceableD_tuple wit, B_tuple self, B_Iterable wit2, B_slice slc, $WORD iter) {
    fprintf(stderr,"%s\n","internal error: setslice on immutable tuple");
    exit(-1);
}

void B_SliceableD_tupleD___delslice__ (B_SliceableD_tuple wit, B_tuple self, B_slice slc) {
    fprintf(stderr,"%s\n","internal error: delslice on immutable tuple");
    exit(-1);
}

struct B_SliceableD_tupleG_class B_SliceableD_tupleG_methods = {
    "B_SliceableD_tuple",
    UNASSIGNED,
    ($SuperG_class)&B_SliceableG_methods,
    B_SliceableD_tupleD___init__,
    B_SliceableD_tupleD___serialize__,
    B_SliceableD_tupleD___deserialize__,
    (B_bool (*)(B_SliceableD_tuple))$default__bool__,
    (B_str (*)(B_SliceableD_tuple))$default__str__,
    (B_str (*)(B_SliceableD_tuple))$default__str__,
    B_SliceableD_tupleD___getitem__,
    B_SliceableD_tupleD___setitem__,
    B_SliceableD_tupleD___delitem__,
    B_SliceableD_tupleD___getslice__,
    B_SliceableD_tupleD___setslice__,
    B_SliceableD_tupleD___delslice__
};
struct  B_SliceableD_tuple B_SliceableD_tuple$instance = {&B_SliceableD_tupleG_methods};
struct B_SliceableD_tuple *B_SliceableD_tupleG_witness = &B_SliceableD_tuple$instance;

// Hashable ///////////////////////////////////////////////////////////////

void B_HashableD_tupleD___init__ (B_HashableD_tuple wit, int n, B_Hashable *comps) {
    wit->W_HashableB_tuple$size = n;
    wit->W_Hashable = comps;
}

void B_HashableD_tupleD___serialize__(B_HashableD_tuple self, $Serial$state state) {
    $step_serialize(toB_int(self->W_HashableB_tuple$size), state);
    // we need to serialize the array of Hashables!!
}

B_HashableD_tuple B_HashableD_tupleD___deserialize__(B_HashableD_tuple self, $Serial$state state) {
    B_HashableD_tuple res = $DNEW(B_HashableD_tuple,state);
    res->W_HashableB_tuple$size = fromB_int($step_deserialize(state));
    res->W_Hashable = NULL; // We do not get hash functions for the tuple!
    return res;
}

B_bool B_HashableD_tupleD___eq__ (B_HashableD_tuple wit, B_tuple tup1, B_tuple tup2) {
    //type-checking guarantees that sizes are equal
    for (int i=0; i<tup1->size; i++)
        if (!wit->W_Hashable[i]->$class->__eq__(wit->W_Hashable[i],tup1->components[i],tup2->components[i]))
            return $False;
    return $True;
}

B_bool B_HashableD_tupleD___ne__ (B_HashableD_tuple wit, B_tuple tup1, B_tuple tup2) {
    return toB_bool(!fromB_bool(B_HashableD_tupleD___eq__(wit,tup1,tup2)));
}
    
  
B_int B_HashableD_tupleD___hash__ (B_HashableD_tuple wit, B_tuple tup) {
    return toB_int(B_tupleD_hash(wit,tup));
}

struct B_HashableD_tupleG_class B_HashableD_tupleG_methods = {
    "B_HashableD_tuple",
    UNASSIGNED,
    ($SuperG_class)&B_HashableG_methods,
    B_HashableD_tupleD___init__,
    B_HashableD_tupleD___serialize__,
    B_HashableD_tupleD___deserialize__,
    (B_bool (*)(B_HashableD_tuple))$default__bool__,
    (B_str (*)(B_HashableD_tuple))$default__str__,
    (B_str (*)(B_HashableD_tuple))$default__str__,
    B_HashableD_tupleD___eq__,
    B_HashableD_tupleD___ne__,
    B_HashableD_tupleD___hash__
};

