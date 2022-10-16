#pragma once

#include <tuple>

template<typename T>
struct FunctionTraits {
    
};

template<typename Ret, typename Arg, typename... Args>
struct FunctionTraits<Ret(Arg, Args...)> {
public:
    typedef Ret function_type(Arg, Args...);
    typedef Ret function_pointer_type(Arg, Args...);
    typedef Ret return_type;

    typedef std::tuple<Arg, Args...> arg_type;
    typedef std::tuple<std::remove_const_t<std::remove_reference_t<Arg>>,
                       std::remove_const_t<std::remove_reference_t<Args>>...>
                       bare_tuple_type;
};

template <typename Ret> 
struct FunctionTraits<Ret()> {
public:
  typedef Ret function_type();
  typedef Ret (*function_pointer_type)();
  typedef Ret return_type;
  
  typedef std::tuple<> tuple_type;
  typedef std::tuple<> bare_tuple_type;
};

template <typename Ret, typename... Args>
struct FunctionTraits<Ret (*)(Args...)> : FunctionTraits<Ret(Args...)> {
};

// template <typename Ret, typename... Args>
// struct FunctionTraits<Ret (* const)(Args...)> : FunctionTraits<Ret(Args...)> {
// };

template <typename Ret, typename ClassType, typename... Args>
struct FunctionTraits<Ret (ClassType::*)(Args...)> : FunctionTraits<Ret(Args...)> {
};

template <typename Ret, typename ClassType, typename... Args>
struct FunctionTraits<Ret (ClassType::*)(Args...) const> : FunctionTraits<Ret(Args...)> {
};