#include <quickjs.hpp>
#include <cassert>
#include <iostream>

int main() {
    qjs::Runtime rt;
    qjs::Context ctx(rt);

    // get 40 + 2
    qjs::Value result = ctx.eval("40 + 2");
    // check if result is assignable
    auto result2 = result;

    // check if throw does not throw nothing
    qjs::throwIfException(ctx.get(), result);

    //
    assert(result.as<int>() == 42);

    //
    qjs::Value newval = ctx.newValue(54);
    assert(newval.as<int>() == 54);

    // 
    newval = ctx.newValue("Fineload");
    assert(newval.as<std::string>() == "Fineload");

    //
    result = ctx.eval("40asd43");
    try{
        qjs::throwIfException(ctx.get(), result);
        //
        assert(false);
    } catch (qjs::Exception e){
        assert (true);
    }

    //
    assert(ctx.newArray().isArray());

    //
    assert(ctx.newObject().isObject());

    // 
    auto obj = ctx.newObject();
    obj.set("name", 1);

    //
    assert(obj.get("name").is<int>());
    
    //
    assert(obj.get("name").as<int>() == 1);

    std::cout << "Tests succeeded" << std::endl;

    return 0;
}
