#pragma once

#include "../VM/BytecodeVM.hxx"
#include <stdexcept>
#include <tuple>
#include <type_traits>

namespace Solstice::Scripting::NativeBinding {

template<class T>
struct Converter;

template<>
struct Converter<int64_t> {
    static int64_t From(const Value& value) {
        if (std::holds_alternative<int64_t>(value)) return std::get<int64_t>(value);
        if (std::holds_alternative<double>(value)) return static_cast<int64_t>(std::get<double>(value));
        throw std::runtime_error("Expected int64_t argument");
    }
    static Value To(int64_t value) { return value; }
};

template<>
struct Converter<float> {
    static float From(const Value& value) {
        if (std::holds_alternative<double>(value)) return static_cast<float>(std::get<double>(value));
        if (std::holds_alternative<int64_t>(value)) return static_cast<float>(std::get<int64_t>(value));
        throw std::runtime_error("Expected float argument");
    }
    static Value To(float value) { return static_cast<double>(value); }
};

template<>
struct Converter<double> {
    static double From(const Value& value) {
        if (std::holds_alternative<double>(value)) return std::get<double>(value);
        if (std::holds_alternative<int64_t>(value)) return static_cast<double>(std::get<int64_t>(value));
        throw std::runtime_error("Expected double argument");
    }
    static Value To(double value) { return value; }
};

template<>
struct Converter<bool> {
    static bool From(const Value& value) {
        if (std::holds_alternative<int64_t>(value)) return std::get<int64_t>(value) != 0;
        throw std::runtime_error("Expected bool/int argument");
    }
    static Value To(bool value) { return static_cast<int64_t>(value ? 1 : 0); }
};

template<>
struct Converter<std::string> {
    static std::string From(const Value& value) {
        if (std::holds_alternative<std::string>(value)) return std::get<std::string>(value);
        throw std::runtime_error("Expected string argument");
    }
    static Value To(const std::string& value) { return value; }
};

template<>
struct Converter<Math::Vec2> {
    static Math::Vec2 From(const Value& value) {
        if (std::holds_alternative<Math::Vec2>(value)) return std::get<Math::Vec2>(value);
        throw std::runtime_error("Expected Vec2 argument");
    }
    static Value To(const Math::Vec2& value) { return value; }
};

template<>
struct Converter<Math::Vec3> {
    static Math::Vec3 From(const Value& value) {
        if (std::holds_alternative<Math::Vec3>(value)) return std::get<Math::Vec3>(value);
        throw std::runtime_error("Expected Vec3 argument");
    }
    static Value To(const Math::Vec3& value) { return value; }
};

template<>
struct Converter<Math::Vec4> {
    static Math::Vec4 From(const Value& value) {
        if (std::holds_alternative<Math::Vec4>(value)) return std::get<Math::Vec4>(value);
        throw std::runtime_error("Expected Vec4 argument");
    }
    static Value To(const Math::Vec4& value) { return value; }
};

template<>
struct Converter<Math::Quaternion> {
    static Math::Quaternion From(const Value& value) {
        if (std::holds_alternative<Math::Quaternion>(value)) return std::get<Math::Quaternion>(value);
        throw std::runtime_error("Expected Quaternion argument");
    }
    static Value To(const Math::Quaternion& value) { return value; }
};

template<>
struct Converter<std::shared_ptr<Dictionary>> {
    static std::shared_ptr<Dictionary> From(const Value& value) {
        if (std::holds_alternative<std::shared_ptr<Dictionary>>(value)) return std::get<std::shared_ptr<Dictionary>>(value);
        throw std::runtime_error("Expected object argument");
    }
    static Value To(const std::shared_ptr<Dictionary>& value) { return value; }
};

template<>
struct Converter<Value> {
    static Value From(const Value& value) { return value; }
    static Value To(const Value& value) { return value; }
};

template<class T>
using BareT = std::remove_cv_t<std::remove_reference_t<T>>;

template<class Ret, class... Args, class Fn, size_t... Index>
Value InvokeTyped(Fn&& fn, const std::vector<Value>& args, std::index_sequence<Index...>) {
    if (args.size() != sizeof...(Args)) {
        throw std::runtime_error("Native argument count mismatch");
    }
    if constexpr (std::is_void_v<Ret>) {
        fn(Converter<BareT<Args>>::From(args[Index])...);
        return static_cast<int64_t>(0);
    } else {
        Ret result = fn(Converter<BareT<Args>>::From(args[Index])...);
        return Converter<BareT<Ret>>::To(result);
    }
}

template<class Ret, class... Args, class Fn>
BytecodeVM::NativeFunc Bind(Fn&& fn) {
    return [f = std::forward<Fn>(fn)](const std::vector<Value>& args) -> Value {
        return InvokeTyped<Ret, Args...>(f, args, std::index_sequence_for<Args...>{});
    };
}

template<class Ret, class... Args, class TObject>
BytecodeVM::NativeFunc BindMethod(TObject* object, Ret(TObject::*method)(Args...)) {
    return Bind<Ret, Args...>([object, method](Args... args) -> Ret {
        return (object->*method)(std::forward<Args>(args)...);
    });
}

template<class Ret, class... Args, class TObject>
BytecodeVM::NativeFunc BindMethod(TObject* object, Ret(TObject::*method)(Args...) const) {
    return Bind<Ret, Args...>([object, method](Args... args) -> Ret {
        return (object->*method)(std::forward<Args>(args)...);
    });
}

template<class Ret, class... Args, class Fn>
void Register(BytecodeVM& vm, const std::string& name, Fn&& fn) {
    vm.RegisterNative(name, Bind<Ret, Args...>(std::forward<Fn>(fn)));
}

} // namespace Solstice::Scripting::NativeBinding
