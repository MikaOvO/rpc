#pragma once
#include <string.h>
#include <tuple>

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

template<size_t N, size_t... Sequence>
struct TupleSequenceWithout0 : TupleSequenceWithout0<N - 1, N - 1, Sequence...> {
};

template<size_t... Sequence>
struct TupleSequenceWithout0<1, Sequence...> {
    typedef TupleIndex<Sequence...> sequence;
};

// template<typename Func, typename... Args, size_t... Index>
// void call_helper(Func func, std::tuple<Args...> args, TupleIndex<Index...>) {
//     func(std::get<Index>(args)...);
// }

// template<typename Func, typename... Args>
// void call(Func func, std::tuple<Args...> args) {
//     call_helper(func, args, typename TupleSequence<sizeof...(Args)>::sequence());
// }
