#pragma once

#include "Seed.hxx"
#include <functional>
#include <memory>

namespace Solstice::Arzachel {

// Generator<T> represents a pure function Seed → T
// All generators are deterministic and stateless
template<typename T>
class Generator {
public:
    using FunctionType = std::function<T(const Seed&)>;

    // Default constructor (null generator)
    Generator() : MFunc(nullptr) {}

    // Construct from function
    explicit Generator(FunctionType Func) : MFunc(std::move(Func)) {}

    // Copy constructor
    Generator(const Generator& Other) : MFunc(Other.MFunc) {}

    // Move constructor
    Generator(Generator&& Other) noexcept : MFunc(std::move(Other.MFunc)) {}

    // Copy assignment
    Generator& operator=(const Generator& Other) {
        if (this != &Other) {
            MFunc = Other.MFunc;
        }
        return *this;
    }

    // Move assignment
    Generator& operator=(Generator&& Other) noexcept {
        if (this != &Other) {
            MFunc = std::move(Other.MFunc);
        }
        return *this;
    }

    // Evaluation: apply generator to seed
    T operator()(const Seed& S) const {
        return MFunc(S);
    }

    // Functor map: transform the output type
    template<typename U>
    Generator<U> Map(std::function<U(T)> Transform) const {
        return Generator<U>([This = *this, Transform = std::move(Transform)](const Seed& S) {
            return Transform(This(S));
        });
    }

    // Monadic bind: chain generators
    template<typename U>
    Generator<U> FlatMap(std::function<Generator<U>(T)> Transform) const {
        return Generator<U>([This = *this, Transform = std::move(Transform)](const Seed& S) {
            T Value = This(S);
            Generator<U> NextGen = Transform(Value);
            // Derive new seed for the next generator to avoid correlation
            Seed NS = S.Derive(static_cast<uint64_t>(std::hash<T>{}(Value)));
            return NextGen(NS);
        });
    }

    // Applicative combine: combine two generators
    template<typename U>
    Generator<T> Combine(const Generator<U>& Other, std::function<T(U)> CombineFunc) const {
        return Generator<T>([This = *this, Other, CombineFunc = std::move(CombineFunc)](const Seed& S) {
            // Derive separate seeds for each generator
            Seed S1 = S.Derive(0);
            Seed S2 = S.Derive(1);
            U OtherValue = Other(S2);
            return CombineFunc(OtherValue);
        });
    }

    // Static factory: constant value (pure value generator)
    static Generator<T> Constant(const T& Value) {
        return Generator<T>([Value](const Seed&) {
            return Value;
        });
    }

    // Static factory: wrap a function
    static Generator<T> FromFunction(FunctionType Func) {
        return Generator<T>(std::move(Func));
    }

private:
    FunctionType MFunc;
};

} // namespace Solstice::Arzachel
