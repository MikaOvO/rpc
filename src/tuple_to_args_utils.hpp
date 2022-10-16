#pragma once
#include <string.h>

template<size_t... Sequence>
struct TupleIndex {
};

template<size_t N, size_t... Sequence>
struct TupleSequence : TupleSequence<N - 1, N - 1, Sequence...> {
};

template<size_t... Sequence>
struct TupleSequence<0, Sequence...> {
    typedef TupleIndex<Sequence...> sequence;
};

