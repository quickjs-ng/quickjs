#pragma once

#include <map>
#include <quickjs.h>
#include <stdexcept>
#include <vector>

namespace qjs {

class Value {
public:
  static std::vector<qjs::Value> makeVector(JSContext *ctx, int argc,
                                            JSValueConst *argv) {
    std::vector<qjs::Value> out;
    out.reserve(argc);

    for (int i = 0; i < argc; i++) {
      out.emplace_back(ctx, JS_DupValue(ctx, argv[i]));
    }

    return out;
  }

  void assign(JSContext *ctx, JSValue value, bool takeOwnership = true) {
    free();
    ctx_ = ctx;
    if (takeOwnership){
      value_ = value;
    } else {
      value_ = JS_DupValue(ctx, value);    
    }
  }

  void assign(JSContext *ctx, double value){
    assign(ctx, JS_NewFloat64(ctx, value));
  }

  void assign(JSContext *ctx, const std::string& value){
    assign(ctx, JS_NewString(ctx, value.c_str()));
  }

  void assignArray(JSContext *ctx){
    assign(ctx, JS_NewArray(ctx));
  }

  void assignObject(JSContext *ctx){
    assign(ctx, JS_NewObject(ctx));
  }

  #define ASSIGN_MOVE(val) {                \
    assign(val.ctx_, val.value_, false);    \
    val.value_ = JS_UNDEFINED;              \
    val.ctx_ = nullptr;                     \
  }

  template <typename T> Value(JSContext *ctx, T t){
    assign(ctx, t);
  }

  void assign(const Value &val) {
    assign(val.ctx_, val.value_, false);
  }

  Value(){
  }

  Value(JSContext *ctx, JSValue value, bool takeOwnership = true) { 
    assign(ctx, value, takeOwnership); 
  }

  Value &operator=(Value &&val) noexcept {
    ASSIGN_MOVE(val);
    return *this;
  }

  Value &operator=(const Value &val) {
    assign(val);
    return *this;
  }

  Value(const Value &val) { assign(val); };

  Value(Value &&val) noexcept { ASSIGN_MOVE(val); }

  JSContext *context() const { return ctx_; }

  JSValue raw() const { return value_; }

  JSValue rawdup() const { return JS_DupValue(ctx_, value_); }

  bool set(size_t index, qjs::Value value){
    if (isArray()){
        auto ctx = context();
        auto val = raw();
        JS_SetPropertyUint32(ctx, val, index, value.rawdup());
        return true;
    } else {
        return false;
    }
  }

  bool set(std::string index, qjs::Value value){
    if (isObject()){
        JS_SetPropertyStr(context(), raw(), index.c_str(), value.rawdup());
        return true;
    } else {
        return false;
    }
  }

  template <typename T> bool set(size_t index, T value){
    return set(index, qjs::Value(ctx_, value));
  }

  template <typename T> bool set(std::string index, T value){
    return set(index, qjs::Value(ctx_, value));
  }

  qjs::Value get(size_t index){
    if (isArray()){
        auto ctx = context();
        auto new_val = JS_GetPropertyUint32(ctx, raw(), index);
        return Value(ctx, new_val);
    } else {
        return Value(nullptr, JS_UNDEFINED);
    }
  }

  qjs::Value get(const std::string & name){
    if (isObject()){
        auto ctx = context();
        auto new_val = JS_GetPropertyStr(ctx, raw(), name.c_str());
        return Value(ctx, new_val);
    } else {
        return Value(nullptr, JS_UNDEFINED);
    }
  }

  template <typename T> bool is() const;

  template <typename T> T as() const;

  bool isException() const { return JS_IsException(raw()); }

  bool isUndefined() const { return JS_IsUndefined(raw()); }

  bool isNull() const { return JS_IsNull(raw()); }

  bool isBool() const { return JS_IsBool(raw()); }

  bool isNumber() const { return JS_IsNumber(raw()); }

  bool isString() const { return JS_IsString(raw()); }

  bool isObject() const { return JS_IsObject(raw()); }

  bool isFunction() const { return JS_IsFunction(context(), raw()); }

  bool isArray() const { return JS_IsArray(raw()); }

  bool isSymbol() const { return JS_VALUE_GET_TAG(raw()) == JS_TAG_SYMBOL; }

  bool isBigInt() const { return JS_VALUE_GET_TAG(raw()) == JS_TAG_BIG_INT; }

  ~Value() { free(); }

private:
  void free() {
    if (ctx_ != nullptr) {
      JS_FreeValue(ctx_, value_);
      ctx_ = nullptr;
    }
  }
  JSContext *ctx_ = nullptr;
  JSValue value_ = JS_UNDEFINED;
};

class Runtime {
public:
  Runtime() {
    rt_ = JS_NewRuntime();
    if (!rt_)
      throw std::runtime_error("JS_NewRuntime failed");
  }

  ~Runtime() {
    if (rt_)
      JS_FreeRuntime(rt_);
  }

  Runtime(const Runtime &) = delete;
  Runtime &operator=(const Runtime &) = delete;

  Runtime(Runtime &&other) noexcept : rt_(other.rt_) { other.rt_ = nullptr; }

  Runtime &operator=(Runtime &&other) noexcept {
    if (this != &other) {
      if (rt_)
        JS_FreeRuntime(rt_);
      rt_ = other.rt_;
      other.rt_ = nullptr;
    }
    return *this;
  }

  JSRuntime *get() const { return rt_; }

private:
  JSRuntime *rt_{};
};

class Context {
public:
  explicit Context(Runtime &runtime) {
    ctx_ = JS_NewContext(runtime.get());
    if (!ctx_)
      throw std::runtime_error("JS_NewContext failed");
  }

  ~Context() {
    if (ctx_)
      JS_FreeContext(ctx_);
  }

  Context(const Context &) = delete;
  Context &operator=(const Context &) = delete;

  JSContext *get() const { return ctx_; }

  template<typename T> qjs::Value newValue(T t){
    return qjs::Value(ctx_, t);
  }

  qjs::Value newObject() {
    qjs::Value val;
    val.assignObject(ctx_);
    return val;
  }

  qjs::Value newArray() {
    qjs::Value val;
    val.assignArray(ctx_);
    return val;
  }

  qjs::Value eval(const std::string &code,
                  const std::string &filename = "<eval>") {
    return Value(ctx_, JS_Eval(ctx_, code.c_str(), code.size(),
                               filename.c_str(), JS_EVAL_TYPE_GLOBAL));
  }

private:
  JSContext *ctx_{};
};

template <> inline bool Value::is<std::vector<Value>>() const {
  return isArray();
}

template <> inline bool Value::is<std::map<std::string, Value>>() const {
  return isObject();
}

template <> inline bool Value::is<std::nullptr_t>() const { return isNull(); }

template <> inline bool Value::is<bool>() const { return isBool(); }

template <> inline bool Value::is<int32_t>() const {
  return isNumber();
}

template <> inline bool Value::is<int64_t>() const {
  return isNumber();
}

template <> inline bool Value::is<double>() const { return isNumber(); }

template <> inline bool Value::is<std::string>() const { return isString(); }

class Exception : public std::runtime_error {
public:
  explicit Exception(JSContext *ctx, const std::string &error = "") : std::runtime_error(extract(ctx, error)) {}

private:
  static std::string extract(JSContext *ctx, const std::string& error) {
    JSValue exc = JS_GetException(ctx);

    JSAtom atom = JS_NewAtom(ctx, "stack");
    std::string result;

    JSValue stack = JS_GetProperty(ctx, exc, atom);

    JS_FreeAtom(ctx, atom);

    if (!JS_IsUndefined(stack)) {
      const char *s = JS_ToCString(ctx, stack);

      if (s) {
        result = s;
        JS_FreeCString(ctx, s);
      }
    }

    {
      const char *s = JS_ToCString(ctx, exc);

      if (s) {
        result += s;
        JS_FreeCString(ctx, s);
      }
    }

    JS_FreeValue(ctx, stack);
    JS_FreeValue(ctx, exc);
    if (error.empty()){
      return result;
    } else {
      return error + " " + result;
    }
  }
};

template <> inline std::string Value::as<std::string>() const {
  auto ctx_ = context();
  const char *str = JS_ToCString(context(), raw());

  if (!str)
    throw Exception(ctx_);

  std::string result(str);

  JS_FreeCString(ctx_, str);

  return result;
}
/*
template <> inline std::nullptr_t Value::as<std::nullptr_t>() const {
  if (!JS_IsNull(raw()))
    throw std::bad_cast();

  return nullptr;
}*/

template <> inline bool Value::as<bool>() const {
  auto value_ = raw();
  auto ctx_ = context();
  int r = JS_ToBool(ctx_, value_);

  if (r < 0)
    throw Exception(ctx_);

  return r;
}

template <> inline int32_t Value::as<int32_t>() const {
  int32_t v;
  auto value_ = raw();
  auto ctx_ = context();

  if (JS_ToInt32(ctx_, &v, value_))
    throw Exception(ctx_);

  return v;
}

template <> inline int64_t Value::as<int64_t>() const {
  int64_t v;
  auto value_ = raw();
  auto ctx_ = context();

  if (JS_ToInt64(ctx_, &v, value_))
    throw Exception(ctx_);

  return v;
}

template <> inline double Value::as<double>() const {
  double v;
  auto value_ = raw();
  auto ctx_ = context();

  if (JS_ToFloat64(ctx_, &v, value_))
    throw Exception(ctx_);

  return v;
}

inline void throwIfException(JSContext *ctx, JSValueConst value) {
  if (!JS_IsException(value)){
    return;
  }

    throw qjs::Exception(ctx);
}

inline void throwIfException(JSContext *ctx, qjs::Value &value) {
  throwIfException(ctx, value.raw());
}

} // namespace qjs
