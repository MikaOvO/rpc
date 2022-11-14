#pragma once

#include <map>
#include <boost/asio.hpp>
#include <iostream>

#include "msgpack_utils.hpp"
#include "tuple_to_args_utils.hpp"
#include "common.hpp"
#include "connection.hpp"
#include "function_traits.hpp"

using namespace boost;

class Router {
public:
    template <typename T>
    void router(const char *data, size_t size, std::shared_ptr<T> conn) {
        auto req_id = conn->get_req_id();
        std::string result;
        try {
            auto p = unpack<std::tuple<std::string>>(data, size);
            auto func_name = std::get<0>(p);
            Log::WriteLogDefault(0, "call\n");
            auto it = invoker_.find(func_name);
            if (it == invoker_.end()) {
                result = pack_args_str(Result_FAIL, "unknown function: " + func_name);
                Log::WriteLogDefault(0, "unknown function\n");
                conn->response(req_id, std::move(result));
                return;
            } 
            it->second(data, size, result);
            if (result.size() >= BUFFER_SIZE) {
                result = pack_args_str(Result_FAIL, "Return size > 10MB.");
                Log::WriteLogDefault(0, "> 10 MB\n");
            }
            conn->response(req_id, std::move(result));
        } catch (std::exception &e) {
            result = pack_args_str(Result_FAIL, e.what());
            Log::WriteLogDefault(2, e.what() + '\n');
            conn->response(req_id, std::move(result));
        }
    }

    template<typename Func>
    void register_handler(std::string name, Func func) {
        auto f = std::bind(apply<Func>, func, 
                           std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
        invoker_[name] = f; 
    }
    
    template<typename Func, typename Self>
    void register_handler(std::string name, Func func, Self *self) {
        auto f = std::bind(apply_member<Func, Self>, func, self, 
                           std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
        invoker_[name] = f; 
    }

    template<typename Func, size_t... Sequence, typename... Args>
    static typename FunctionTraits<Func>::return_type 
    call_helper(const Func &func, std::tuple<Args...> args, TupleIndex<Sequence...>) {
        return func(std::get<Sequence>(args)...);
    }
    template<typename Func, typename Self, size_t... Sequence, typename... Args>
    static typename FunctionTraits<Func>::return_type
    call_member_helper(const Func &func, Self *self, std::tuple<Args...> args, TupleIndex<Sequence...>) {
        return (*self.*func)(std::get<Sequence>(args)...);
    }

    template<typename Func, typename... Args>
    static typename std::enable_if<std::is_void<typename FunctionTraits<Func>::return_type>::value>::type
    call(const Func &func, std::string &result, std::tuple<Args...> args) {
        call_helper(func, args, typename TupleSequenceWithout0<sizeof...(Args)>::sequence());
        result = pack_args_str(Result_OK);
    }

    template<typename Func, typename... Args>
    static typename std::enable_if<!std::is_void<typename FunctionTraits<Func>::return_type>::value>::type
    call(const Func &func, std::string &result, std::tuple<Args...> args) {
        auto r = call_helper(func, args, typename TupleSequenceWithout0<sizeof...(Args)>::sequence());
        result = pack_args_str(Result_OK, r);
    }
    template<typename Func, typename Self, typename... Args>
    static typename std::enable_if<std::is_void<typename FunctionTraits<Func>::return_type>::value>::type
    call_member(const Func &func, Self *self, std::string &result, std::tuple<Args...> args) {
        call_member_helper(func, self, args, typename TupleSequenceWithout0<sizeof...(Args)>::sequence());
        result = pack_args_str(Result_OK);
    }
    template<typename Func, typename Self, typename... Args>
    static typename std::enable_if<!std::is_void<typename FunctionTraits<Func>::return_type>::value>::type
    call_member(const Func &func, Self *self, std::string &result, std::tuple<Args...> args) {
        auto r = call_member_helper(func, self, args, typename TupleSequenceWithout0<sizeof...(Args)>::sequence());
        result = pack_args_str(Result_OK, r);
    }

    template<typename Func>
    static void apply(const Func &func, const char *data, size_t size, std::string &result) {
        using args_tuple = typename FunctionTraits<Func>::name_bare_tuple_type;
        try {
            auto args = unpack<args_tuple>(data, size);
            call(func, result, args);
        } catch (const std::exception &e) {
            result = pack_args_str(Result_FAIL, e.what());
        }
    }
    template<typename Func, typename Self>
    static void apply_member(const Func &func, Self *self, const char *data, size_t size, std::string &result) {
        using args_tuple = typename FunctionTraits<Func>::name_bare_tuple_type;
        try {
            auto args = unpack<args_tuple>(data, size);
            call_member(func, self, result, args);
        } catch (const std::exception &e) {
            result = pack_args_str(Result_FAIL, e.what());
        }
    }
    
private:
    std::unordered_map<std::string, std::function<void(
        const char*,
        size_t,
        std::string &result
    )>> invoker_;
};  